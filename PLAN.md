# Flexible provider fetch orchestration with build-time-only capabilities

## Summary

Redesign remote data fetching around one `weather_report_t` object and one unified provider interface. Providers return any number of fetch operations, allowing one request to populate several data groups or several requests to populate one provider’s basic data.

Data ownership tags exist only in Python during configuration/build validation. They are not represented in the runtime C++ API, `FetchOperation`, or executor. Runtime failure handling uses a per-operation required/optional flag.

The Python validator will enforce exclusive ownership: after resolving the configured `remoteProviders` entries, no two distinct logical providers may advertise/own the same tag. A multi-group source is represented by one provider entry rather than being implicitly coalesced from role-specific entries.

## Progress checklist

Update this checklist as work progresses. Mark an item complete only after the corresponding implementation or verification has been completed; add notes or follow-up items here when the plan changes.

### Decisions confirmed

- [x] Use one aggregate `weather_report_t` for remote and local rendering inputs.
- [x] Keep current/hourly/daily forecast data and their fields non-optional.
- [x] Make air quality and alerts optional top-level groups.
- [x] Keep ownership tags in Python only; do not add runtime tag metadata.
- [x] Require exclusive ownership of each effective tag among distinct logical providers.
- [x] Derive build-time capability requirements from the configured layout.
- [x] Continue rendering when optional air-quality or alert fetches fail.
- [x] Keep current/hourly/daily forecast failures fatal.
- [x] Allow one provider to return multiple independent fetch operations.
- [x] Use two QEMU configuration variants for now; defer mixed-provider coverage.
- [x] Configure remote sources as a `remoteProviders` list rather than fixed weather/AQ/alerts blocks.
- [x] Store OWM `apiKey` on each OWM provider entry instead of as a global setting.
- [x] Parse provider results directly into their assigned `weather_report_t` groups.

### Implementation

- [x] Add `weather_report_t` and group reset/engagement helpers.
- [x] Update renderer and `main.cpp` to consume the aggregate object.
- [x] Introduce the unified provider interface and migrate operation creation.
- [x] Migrate Open-Meteo weather and air-quality providers.
- [x] Migrate OWM One Call v3 and remove fetch-order-dependent piggyback caching.
- [x] Migrate OWM air quality and MeteoAlarm alerts.
- [x] Replace fixed provider factory bundles with flattened provider-owned operations.
- [x] Add Python provider capability declarations and layout-derived validation.
- [x] Add exclusive duplicate ownership detection for `remoteProviders` entries.
- [x] Migrate schema, example, pinned test, and device configuration documentation to `remoteProviders`.
- [x] Update README/provider-extension documentation.

### Verification

- [x] Update all parser and provider fixture tests for `weather_report_t`.
- [x] Add tests for one provider returning multiple operations.
- [x] Add tests for required versus optional operation failures.
- [x] Add Python validation tests for missing and duplicate tags.
- [x] Run the normal PlatformIO build successfully.
- [x] Run all pinned QEMU configuration suites successfully.

## Data model and rendering

- Add a unified `weather_report_t`, preferably in `include/weather_report.h`, containing:
  - `forecast_t forecast` as the mandatory weather model. Keep current/hourly/daily structures and their individual fields non-optional.
  - `std::optional<air_quality_t> air_quality`.
  - `std::optional<std::vector<weather_alert_t>> alerts`.
  - The locally produced `sensor_readings` and `moon_state_t` values, so all data inputs required by rendering have one aggregate owner.
- Keep the existing compact forecast field types and default/reset behavior. Do not convert individual weather fields to `std::optional`; only air quality and alerts are optional top-level groups.
- Add group-specific reset/engagement helpers so a provider task clears or commits only its own output. This matters when multiple operations from the same provider run concurrently.
- Update renderer top-level entry points to accept `const weather_report_t &`, using a mutable alert vector only where the existing alert filtering mutates its input:
  - `drawCurrentConditions` reads mandatory forecast data, local sensor/moon data, and conditionally draws air quality.
  - `drawOutlookGraph` and `drawForecast` read forecast data.
  - `drawAlerts` treats disengaged alerts as empty.
- Update `src/main.cpp` to replace the separate forecast, air-quality, alerts, moon, and sensor render inputs with one static `weather_report_t`. Battery/RSSI/status strings remain transient UI state because they are not remote-provider output.
- Optional air-quality or alert data must never be dereferenced without checking engagement. If unavailable, the display refresh continues and the affected section is skipped.

## Unified provider interface

- Add one provider interface in `include/remote_data_provider.h`:
  - `getApiName()` for diagnostics.
  - `createFetchOperations(weather_report_t &out)` returning `std::vector<std::unique_ptr<FetchOperation>>`.
- Remove the role-specific `WeatherProvider`, `AirQualityProvider`, and `AlertProvider` interfaces from orchestration. All remote implementations will instead derive from `RemoteDataProvider`.
- Do not add runtime tag/mask methods. The provider’s Python capability declaration is the ownership contract used during build validation.
- Keep `FetchOperation`’s existing `execute()`, `name()`, and `shouldAbortOnFailure()` contract without renaming it:
  - Forecast operations are required.
  - Air-quality and alert operations are optional.
  - An OWM combined forecast+alerts operation is required because it also supplies mandatory forecast data.
  - A standalone alerts-only operation is optional.
- Keep `executeParallel()` generic and unchanged in scheduling behavior: it receives a flat list of arbitrary operations, preserves result order, and enforces the existing maximum concurrency of two.
- Replace the fixed adapters in `provider_fetch_operations.*` with a generic callback operation or provider-owned operation classes. Operations must keep the provider alive through the fetch bundle and write directly to their assigned report groups. If a future provider has multiple operations contributing to one group, that provider is responsible for coordinating writes and handling partial failure.
- Document that dependent provider calls remain inside one operation or use provider-internal synchronization; independent calls may be returned as separate operations. No runtime dependency graph is introduced.

## Provider migrations

- Rename and convert the concrete providers to the unified interface:
  - `OpenMeteoWeatherProvider` -> `OpenMeteoForecastProvider`.
  - Keep `OpenMeteoAirQualityProvider`.
  - `OWMProvider` -> `OpenWeatherMapOneCallV3Provider`.
  - `OWMAirQualityProvider` -> `OpenWeatherMapAirQualityProvider`.
  - Keep `MeteoAlarmAlertProvider`.

### Open-Meteo weather

- Return one required forecast operation for the existing forecast endpoint.
- Change the deserializer target to `weather_report_t` or `out.forecast` through a helper.
- Reset only the forecast output and preserve the current required-time-key checks, streaming parser behavior, retry behavior, and error messages.

### Open-Meteo and OWM air quality

- Return one optional air-quality operation.
- Engage `out.air_quality` and parse directly into it. Reset the optional group on any parse or fetch failure so partial or stale data is never rendered.
- Preserve pollutant mapping, time-window selection, null handling, streaming parser, truncation handling, and fixture behavior.
- A valid but empty API response leaves `air_quality` disengaged rather than presenting zero-filled data as a successful populated group.

### OWM One Call v3

- Make `OpenWeatherMapOneCallV3Provider` one unified provider instance rather than two interface views with fetch-order-dependent cache/mutex piggyback behavior.
- A single `openweathermap_onecall_v3` configuration entry owns current/hourly/daily forecast and alerts, and returns one required One Call v3 operation that requests and maps them in one response.
- Remove the old separate weather/alert cache, mutex, and fetch-order-dependent request modes; the provider entry has one fixed v3 ownership contract.
- Keep condition mapping and fixture deserialization seams, target the unified weather report, and engage the optional alerts group when the response contains it.

### MeteoAlarm

- Return one optional alert operation.
- Engage/reset only `out.alerts` around successful/failed parsing.
- Preserve country validation, HTTPS feed behavior, polygon filtering, duplicate-hazard merging, alert capping, early connection close, retries, and localized errors.

### Future OWM v4-style providers

- A provider can return separate operations for current, hourly, daily, and any other endpoint. Each operation gets its own name and required/optional status.
- Parse directly into the assigned report groups. If multiple operations contribute to one group, the provider is responsible for coordinating writes and handling partial failure.
- The executor runs independent operations concurrently up to the existing pool limit.

## Factory and orchestration

- Replace the role-specific `ProviderBundle` with a collection of shared unified providers.
- Change `createFetchBundle` to accept only `weather_report_t &` and:
  1. construct providers selected by configuration;
  2. ask each provider for its operation list;
  3. flatten those lists into one operation vector;
  4. retain providers in the bundle until all operations and result handling finish.
- Construct one `OpenWeatherMapOneCallV3Provider` and one combined operation for the single `openweathermap_onecall_v3` entry; there are no separate weather and alert selections to coalesce.
- Update `main.cpp` result handling:
  - If an operation fails and `shouldAbortOnFailure()` is true, use the existing cloud/network error display path.
  - If an optional operation fails, log it, disengage its optional output group, and continue.
  - Required forecast providers reject incomplete payloads in their deserializer. Add a final render-data sanity check if needed by the split-task implementation.

## Remote-provider configuration

- Replace the fixed `weatherAPI`, `airQualityAPI`, and `alertsAPI` blocks with a discriminated `remoteProviders` list.
- Each list entry identifies one concrete remote provider and contains only that provider’s settings:
  - `open_meteo_forecast` with `transport`.
  - `open_meteo_air_quality` with `transport`.
  - `openweathermap_onecall_v3` with `transport` and `apiKey`.
  - `openweathermap_air_quality` with `transport` and `apiKey`.
  - `meteoalarm_alert` with `country`.
- Provider identifiers are stable lowercase configuration IDs and are not required to match C++ class names.
- Provider-specific Pydantic models use `extra="forbid"`; `apiKey` is accepted only by OWM entries and `country` only by MeteoAlarm.
- Remove the global `owmApikey` field. `scripts/config.py` emits provider-specific generated constants such as `OPENWEATHERMAP_ONECALL_V3_API_KEY` and `OPENWEATHERMAP_AIR_QUALITY_API_KEY`.
- List order has no semantic meaning and must not control fetch order or ownership resolution.
- The initial pinned configurations become:
  - `openmeteo.yml`: Open-Meteo forecast, Open-Meteo air quality, and MeteoAlarm alert entries.
  - `owm.yml`: OWM One Call v3, OWM air quality, and no separate alert entry because One Call v3 owns alerts.

## Python capability declarations and exclusive ownership validation

- Add `scripts/provider_capabilities.py` as the only location for ownership tags. Use stable names such as:
  - `current_forecast`
  - `hourly_forecast`
  - `daily_forecast`
  - `air_quality`
  - `alerts`
- Model capability entries as the **effective ownership of a selected `remoteProviders` entry**, not merely every data type an API could theoretically return.
- Resolve each `remoteProviders` entry into one logical provider instance and its effective tags:
  - `open_meteo_forecast` and `openweathermap_onecall_v3` own current/hourly/daily forecast.
  - `openweathermap_onecall_v3` also owns alerts.
  - `open_meteo_air_quality` and `openweathermap_air_quality` own air quality.
  - `meteoalarm_alert` owns alerts.
- Reject duplicate ownership before checking required tags:
  - Build a `tag -> owner` map from the resolved logical instances.
  - If a tag is already owned by another logical provider instance, raise a clear configuration error naming the tag and both owners/providers.
  - Do not silently union duplicate providers or choose precedence/fallback; a tag has exactly one owner in a valid configuration.
  - There is no implicit coalescing or precedence between list entries; one provider entry must own each tag.
- Integrate validation into `scripts/config.py` after `ConfigSchema` validation and before generating `include/config.h`:
  - Always require current/hourly/daily forecast ownership because those sections are the base renderer contract.
  - Add `air_quality` to the required capability set when `leftPanelLayout` contains `AIR_QUALITY`.
  - Do not require `alerts`; no alert source and a failed alert request are valid states.
  - Report missing required tags and duplicate ownership as distinct, actionable errors.
- Do not generate runtime tag enums, masks, or tag metadata into `config.h`. The generated header contains only ordinary configuration values/macros needed by C++.
- Treat this as build-time validation performed by the PlatformIO `extra_scripts` hook. Runtime correctness is enforced separately by operation required/optional status and deserializer validation.

## Planned test configurations

- Keep two committed QEMU configurations for now:
  - `test/configs/openmeteo.yml`: Open-Meteo weather + Open-Meteo air quality + MeteoAlarm alerts.
  - `test/configs/owm.yml`: OWM v3 forecast + OWM v3 alerts + OWM air quality.
- [x] Remove the separate `test/configs/owm_piggyback.yml` variant and fold its combined OWM fetch-executor coverage into the `test_owm` suite.
- [x] Remove the `test/src/test_owm_piggyback/` suite after its generic executor tests are moved to `test/src/test_owm/`.
- Defer a mixed OWM-weather/Open-Meteo-air-quality configuration until a later change adds explicit mixed-provider coverage.

## Tests and verification

- Update parser fixture tests to use `weather_report_t` and access `data.forecast`, `data.air_quality`, and `data.alerts`.
  - Preserve all existing weather/AQ/OWM/MeteoAlarm field, bounds, reset, null, and malformed-input assertions.
  - Assert successful optional parsers engage their output group and failures disengage it.
  - Verify OWM combined parsing fills forecast and alerts in one operation path.
- Extend fetch-executor tests with a mock unified provider returning multiple operations:
  - all operations execute;
  - result order is preserved;
  - concurrency remains capped at two;
  - one provider can return several operations;
  - optional failures do not abort the fetch flow;
  - required failures do abort it.
- Replace the current “alerts first” and provider-interface alias assertions with:
  - unified provider/fetch-bundle assertions;
  - one shared OWM combined operation in the `owm.yml` configuration;
  - required versus optional operation assertions.
- Add Python capability-validation tests or a lightweight standalone test script covering:
  - both committed configurations;
  - missing air-quality capability with `AIR_QUALITY` enabled;
  - duplicate ownership of one tag producing a validation error;
  - OWM One Call v3 plus a separate MeteoAlarm entry being rejected because both own alerts;
  - duplicate OWM entries being rejected rather than coalesced;
  - alerts-disabled configurations remaining valid.
- Run:
  - `~/.platformio/penv/bin/pio run -e lolin_d32`
  - `bash test/run_tests_docker.sh`
  - `python scripts/config.py --validate ...` for the committed configurations.
  The normal build must end with `[SUCCESS]`, and all QEMU configuration roots must pass.

## Explicit assumptions and decisions

- Tags are build/configuration metadata only; no `DataTag` enum, bitmask, or tag-aware runtime operation API is introduced.
- Ownership is exclusive among distinct logical provider instances. Duplicate tags are a configuration error, not a runtime conflict to resolve.
- A source that supplies multiple data groups is represented by one `remoteProviders` entry and may own the union of those groups’ tags. `openweathermap_onecall_v3` owning forecast and alerts is the initial example.
- The redesign is a full migration of orchestration rather than a compatibility layer preserving the three old provider interfaces.
- The initial QEMU matrix intentionally uses only the Open-Meteo and combined OWM configurations; mixed-provider combinations are deferred.
- `forecast_t` remains the mandatory compact forecast model nested in `weather_report_t`; individual forecast fields remain non-optional.
- Air quality and alerts are optional top-level data groups. Their absence or runtime fetch failure never prevents rendering, even if the air-quality panel is enabled. The enabled panel still requires an owning provider at build time.
- Current/hourly/daily forecast data are mandatory and their operation failures remain fatal.
- The bounded executor remains dependency-free; providers serialize dependent calls in one operation until a real dependency requirement justifies a later scheduler extension.
- The existing fixed API configuration blocks and global `owmApikey` are replaced by the `remoteProviders` list and provider-local settings; unrelated user-facing configuration keys remain unchanged.
