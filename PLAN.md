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

FetchExecution executeParallelAsync(FetchBundle bundle);
```

The asynchronous call must return without waiting. `wait()` is the explicit
completion point and should be safe to call more than once. The destructor
should wait for an active execution rather than allowing worker tasks to
access destroyed state.

The execution state must be heap-owned and retain:

- the operation list;
- the provider objects needed by callbacks;
- the results;
- the worker bookkeeping and completion semaphore.

Pass a complete `FetchBundle` into the async API by move, so the execution
handle owns all provider and operation lifetimes. If necessary, move
`FetchBundle` and `ProviderBundle` declarations into a small shared header to
avoid an include cycle between `provider_factory.h` and `fetch_executor.h`.

The async executor should always launch a worker for a single operation. The
current single-operation synchronous fast path would otherwise make a sensor
job block its caller.

### 2. Preserve the blocking API

Keep `executeParallel()` as a compatibility/convenience API. Implement it in
terms of the asynchronous primitive where practical:

```cpp
auto execution = executeParallelAsync(std::move(bundle));
execution.wait();
return execution.results();
```

Retain the existing operation-vector overload during the migration so the
current provider code and tests do not all need to change at once. Preserve
its existing behavior and resource-failure fallback semantics.

Do not add cancellation in this change. HTTP and sensor operations are not
currently cancellation-aware; waiting for completion is the safe shutdown
contract.

### 3. Add a local environment sensor provider

Add an `EnvironmentSensorProvider` that creates one optional
`FetchOperation`. The operation should:

1. construct the configured `EnvSensor` implementation;
2. initialize it;
3. read temperature, humidity, and pressure;
4. assign a completed `sensor_readings` value to `report.sensor`;
5. return a non-aborting `ProviderResult`.

Read into a temporary aggregate and assign it only after the readings are
collected, so another task cannot observe a partially populated sensor value.
A failed or unavailable sensor must leave the optional fields disengaged and
must not abort weather fetching or rendering.

Because the existing provider base is named `RemoteDataProvider`, either:

- generalize it to `DataProvider` and use it for both local and remote report
  producers; or
- introduce a parallel local-provider interface and keep the remote API
  unchanged.

Prefer the generic `DataProvider` direction because the provider operation
and report-writing model is already shared. Keep the factory output separated
into local and remote bundles so remote operations are never started before
WiFi:

```cpp
FetchBundle createSensorFetchBundle(weather_report_t &report);
FetchBundle createFetchBundle(weather_report_t &report);  // remote providers
```

The sensor provider should be conditionally compiled out for `BME_TYPE_NONE`.

### 4. Start the sensor job early in `main.cpp`

After the report and basic startup state are initialized, but before WiFi
starts, launch the local job:

```cpp
auto sensorExecution = executeParallelAsync(
    createSensorFetchBundle(environment_data));
```

Then continue with battery handling, WiFi, time synchronization, and the
existing blocking remote provider execution.

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
- operation/provider lifetime remains valid until completion.

Keep the existing blocking API tests, including the single-operation behavior,
unless the compatibility wrapper intentionally changes their implementation.

### Sensor provider tests

Make the sensor provider testable without physical I2C hardware, preferably
through an injectable sensor factory or an equivalent test seam. Cover:

- successful readings;
- individually unavailable readings;
- initialization failure;
- non-aborting failure behavior;
- assignment to `weather_report_t::sensor` only after the read completes.

The QEMU configuration already includes both `BME_TYPE_NONE` and BME-enabled
build paths, but QEMU does not provide a real BME device, so provider tests
must not depend on live hardware.

## Validation

Run the project formatter and both build/test paths:

```sh
./scripts/format.sh --check
~/.platformio/penv/bin/pio run -e lolin_d32
bash test/run_tests_docker.sh
```

The build must cover MQTT-enabled, BME-enabled, and BME-disabled conditional
paths. The full test command must finish with every test case succeeding.
