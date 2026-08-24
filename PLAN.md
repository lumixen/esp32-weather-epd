# Plan: asynchronous fetch execution and local sensor reporting

## Goal

Remove the duplicated `inTemp`, `inHumidity`, and `inPressure` state from
`src/main.cpp`, while allowing the local environment sensor to start before
WiFi and remote requests. Sensor values should be written directly to
`weather_report_t::sensor` and awaited only when MQTT or rendering needs them.

The relevant type is `sensor_readings`; `weather_report_t` contains a `sensor`
member of that type.

## Current constraints

- `executeParallel()` is currently blocking.
- Its `WorkerContext`, results, and completion semaphore live on the stack and
  are safe only because the function waits for every worker before returning.
- Provider operations borrow provider instances through callbacks capturing
  `this`, so providers must remain alive while operations execute.
- `RemoteDataProvider` is explicitly named for remote sources, while the BME
  sensor is a local source.
- The current sensor task starts before WiFi and signals completion through a
  semaphore. MQTT and rendering currently copy values from three globals.

## Design

### Configuration and capability ownership

Put local and remote report producers in one configuration list named
`providers`. The list is a declarative inventory of available data sources;
its order must not determine execution order because local sensor work starts
before WiFi while remote operations require WiFi.

Example:

```yaml
providers:
  - provider: open_meteo_forecast
    transport: HTTPS_VERIFY
  - provider: open_meteo_air_quality
    transport: HTTPS_VERIFY
  - provider: meteoalarm_alert
    country: netherlands
  - provider: bme280
    pinPwr: 27
    pinSDA: 21
    pinSCL: 22
    address: 0x76
```

Omitting `bme280` disables the local sensor. The existing top-level `bme:`
block should be removed rather than maintaining two sources of truth. The
BME-specific fields become fields on the `bme280` provider entry, allowing
future local sensor implementations to have their own configuration without
adding more top-level device sections.

Extend the capability ownership model with scalar indoor-sensor tags:

```python
"bme280": {"in_temperature", "in_humidity", "in_pressure"}
```

Use the canonical tags `in_temperature`, `in_humidity`, and `in_pressure`
throughout validation and provider metadata. These tags describe ownership of
`weather_report_t::sensor` fields; they are separate from display layout keys
and MQTT topic names.

The validator should:

- inspect every entry in `providers`, not only remote entries;
- reject duplicate ownership of any capability tag;
- continue requiring `current_forecast`, `hourly_forecast`, and
  `daily_forecast` from remote providers;
- continue requiring `air_quality` when the layout requests it;
- treat indoor sensor tags as optional capabilities;
- report `Provider capabilities` rather than `Remote provider capabilities`.

The generated header should preserve the existing BME compile-time interface
for now (`BME_TYPE_BME280`, `BME_PIN_*`, and `BME_ADDRESS`), but generate those
values from the `bme280` provider entry. It should also emit a provider
selection macro such as `LOCAL_PROVIDER_BME280`. With no local sensor entry,
generate the equivalent of `BME_TYPE_NONE`.

The schema migration should rename `remoteProviders` to `providers`, replace
`RemoteProviderConfig` with a discriminated union containing both remote and
local provider configurations, and update `config.example.yml`, device
examples, and committed test configurations. The capability validator and
header generator must use the new field consistently.

The unified configuration list does not need to map one-to-one onto a
runtime `FetchBundle`. Keep `FetchBundle` for the existing remote provider
batch, and construct the single local sensor operation separately:

```cpp
std::unique_ptr<FetchOperation> createEnvironmentSensorOperation(
    weather_report_t &report);
FetchBundle createFetchBundle(weather_report_t &report);  // remote providers
```

The local operation is started asynchronously before WiFi. The remote bundle
is constructed and executed after WiFi and time synchronization. This keeps
the configuration unified without introducing execution-phase enums or
forcing a one-operation sensor batch to look like a remote provider bundle.

### 1. Add an asynchronous executor handle

Introduce a move-only `FetchExecution` handle in `fetch_executor.h/.cpp` with
an API along these lines:

```cpp
class FetchExecution {
 public:
  FetchExecution(FetchExecution &&) noexcept;
  FetchExecution &operator=(FetchExecution &&) noexcept;
  ~FetchExecution();

  bool wait(TickType_t timeout = portMAX_DELAY);
  bool isComplete() const;
  const std::vector<ProviderResult> &results() const;
};

FetchExecution executeParallelAsync(
    std::vector<std::unique_ptr<FetchOperation>> operations);
```

The asynchronous call must return without waiting. `wait()` is the explicit
completion point and should be safe to call more than once. The destructor
should wait for an active execution rather than allowing worker tasks to
access destroyed state.

The execution state must be heap-owned and retain:

- the operation list;
- the results;
- the worker bookkeeping and completion semaphore.

The async API takes ownership of the operation vector. The first intended
caller is the self-contained environment-sensor operation, so no provider
keep-alive bundle is needed. If remote operations are made asynchronous in a
future change, their provider lifetime must be added explicitly rather than
relying on a dangling callback capture.

The async executor should always launch a worker for a single operation. The
current single-operation synchronous fast path would otherwise make the
sensor job block its caller.

### 2. Preserve the blocking API

Keep the existing blocking `executeParallel()` API unchanged for remote
providers. The asynchronous operation-vector API is an additional primitive,
not a forced migration of the current remote bundle path. Preserve the
blocking executor's existing behavior and resource-failure fallback
semantics.

Do not add cancellation in this change. HTTP and sensor operations are not
currently cancellation-aware; waiting for completion is the safe shutdown
contract.

### 3. Add a local environment sensor operation

Add an `EnvironmentSensorFetchOperation` (or equivalent factory function)
that creates one optional `FetchOperation`. The operation should:

1. construct the configured `EnvSensor` implementation;
2. initialize it;
3. read temperature, humidity, and pressure;
4. assign a completed `sensor_readings` value to `report.sensor`;
5. return a non-aborting `ProviderResult`.

Read into a temporary aggregate and assign it only after the readings are
collected, so another task cannot observe a partially populated sensor value.
A failed or unavailable sensor must leave the optional fields disengaged and
must not abort weather fetching or rendering.

Do not generalize `RemoteDataProvider` in this change. It accurately
describes the existing HTTP providers, while the local sensor is a single
self-contained operation rather than a remote provider collection. Keep the
operation class available to tests regardless of `BME_TYPE_NONE`; the
production factory should only construct the hardware-backed operation when a
local sensor provider is configured.

### 4. Start the sensor job early in `main.cpp`

After the report and basic startup state are initialized, but before WiFi
starts, launch the local job:

```cpp
std::vector<std::unique_ptr<FetchOperation>> sensorOperations;
sensorOperations.push_back(createEnvironmentSensorOperation(environment_data));
auto sensorExecution = executeParallelAsync(std::move(sensorOperations));
```

Then continue with battery handling, WiFi, time synchronization, and the
existing blocking remote-provider execution from
`createFetchBundle(environment_data)`.

The handle must remain alive through all success and error paths until the
sensor data has been consumed. Any function that can publish MQTT before the
normal success path must either receive the handle or invoke a shared wait
helper that has access to it.

### 5. Wait immediately before MQTT/rendering

On the normal path:

```cpp
sensorExecution.wait();

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
publishMqtt(..., environment_data.sensor);
#endif

// Render using environment_data.sensor.
```

The wait is a synchronization point only; it should not copy values into a
second sensor structure. `publishMqtt()` should source values directly from
`environment_data.sensor` or receive a `const sensor_readings &`.

For network/time error paths, wait before MQTT publication as well if local
sensor readings are expected in the error status. WiFi failure paths that do
not publish MQTT do not need to force a sensor wait before sleeping.

### 6. Remove the old main-file sensor state

After the new path is working, remove from `src/main.cpp`:

- `inTemp`;
- `inHumidity`;
- `inPressure`;
- `envSensorReadingTask()`;
- `sensorReadingDoneSemaphore`;
- `getSensorReadings()`;
- the final `environment_data.sensor = getSensorReadings()` copy.

The BME hardware abstraction remains in `env_sensor.h` and
`env_sensor_bme280.*`; only orchestration and report ownership move.

## Testing

### Executor tests

Extend `test/src/test_owm/fetch_executor.inc` with tests that verify:

- `executeParallelAsync()` returns before a deliberately delayed operation
  completes;
- `wait()` eventually returns all operation results;
- a single operation is also started asynchronously;
- empty jobs are already complete;
- repeated `wait()` calls are safe;
- operation lifetime remains valid until completion.

Keep the existing blocking API tests, including the single-operation behavior,
unless the compatibility wrapper intentionally changes their implementation.

### Sensor operation tests

Add `test/src/test_openmeteo/environment_sensor.inc` to the Open-Meteo Unity
root and register it from `test/src/test_openmeteo/test_openmeteo.cpp`. Keep
this suite in the existing Open-Meteo configuration: its pinned configuration
uses `BME_TYPE_NONE`, but the operation logic can still be tested with an
injected fake sensor. The production factory should return no operation when
BME is disabled, while the operation class remains constructible with a fake
for unit testing.

Use a fake `EnvSensor` rather than exercising physical I2C. The simplest seam
is for `EnvironmentSensorFetchOperation` to own an injected
`std::unique_ptr<EnvSensor>`; production construction passes a
`BME280EnvSensor`, while tests pass a fake implementation. This also makes the
sensor lifetime explicit for asynchronous execution without adding a global
mock or test-only preprocessor branch.

Cover the operation synchronously first:

- **Successful readings:** `begin()` succeeds, all three values are present,
  the report receives the expected `sensor_readings`, and all sensor methods
  are called in the expected order.
- **Partially unavailable readings:** one or more getter methods return an
  empty optional. The operation remains non-aborting, returns success if the
  sensor initialized, and stores the available values while leaving missing
  fields disengaged.
- **Initialization failure:** `begin()` returns false, no getter is called,
  the operation returns an error, and `shouldAbortOnFailure()` is false.
- **Stale-data clearing:** initialize `report.sensor` with sentinel values,
  then verify a failed operation does not leave those values visible. A failed
  read should leave the report sensor fields disengaged.
- **Atomic publication:** use a fake that records the sequence and verify the
  report is assigned only after all three reads have completed. This prevents
  a partially populated aggregate from becoming visible to another task.
- **Lifetime and cleanup:** verify the owned sensor is destroyed after the
  operation finishes, including the initialization-failure path.

Add asynchronous integration coverage for the executor/operation boundary:

- Use a fake getter that blocks on a FreeRTOS test semaphore.
- Start it with `executeParallelAsync()` and verify the call returns before
  the getter is released.
- Verify the report remains unchanged while the fake is blocked.
- Release the fake, call `wait()`, and verify the final report and
  `ProviderResult`.
- Verify repeated `wait()` calls and destruction of a completed execution are
  safe.

Keep configuration/build coverage separate from sensor behavior tests:

- A configuration containing `bme280` generates the BME macros and the
  `in_temperature`, `in_humidity`, and `in_pressure` capability ownership.
- A configuration without `bme280` generates the equivalent of
  `BME_TYPE_NONE` and creates no sensor operation.
- Unknown provider fields and invalid BME fields are rejected by the schema.
- The BME-enabled and BME-disabled pinned configurations both compile.

The QEMU configuration includes both `BME_TYPE_NONE` and BME-enabled build
paths, but QEMU does not provide a real BME device. The Open-Meteo suite
should test the hardware-independent operation with the fake implementation
and verify the disabled factory path; the BME-enabled build verifies that the
production construction path compiles. No sensor-operation test may depend on
physical hardware.

## Validation

Run the project formatter and both build/test paths:

```sh
./scripts/format.sh --check
~/.platformio/penv/bin/pio run -e lolin_d32
bash test/run_tests_docker.sh
```

The build must cover MQTT-enabled, BME-enabled, and BME-disabled conditional
paths. The full test command must finish with every test case succeeding.
