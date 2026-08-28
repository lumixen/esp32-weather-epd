# Plan: Move Open-Meteo Forecast to the ESP-IDF HTTP Client

## Objective

Migrate the Open-Meteo weather forecast provider from the Arduino `HTTPClient`/`WiFiClient` transport to the ESP-IDF `esp_http_client` transport, while preserving the provider API, streaming JSON parser, forecast mapping, retry behavior, and existing test coverage.

The Open-Meteo air-quality provider is already migrated and should be used as the closest reference implementation. The NOAA, OpenWeatherMap, and MeteoSwiss providers demonstrate the established ESP-IDF request pattern.

## Scope

### In scope

- Replace the Open-Meteo forecast network transport.
- Reuse `espHttpGetWithRetry()` for WiFi checks, retries, status handling, and client cleanup.
- Reuse `EspHttpClientStream` to expose the ESP-IDF response body through the existing Arduino `Stream` parser interface.
- Preserve TLS verification and the existing HTTP/HTTPS configuration macros.
- Preserve all current forecast deserialization and validation behavior.
- Remove the obsolete Arduino HTTP retry and stream-adapter helpers.
- Remove remaining production dependencies on the Arduino `HTTPClient` error constants.

### Out of scope

- Changing the Open-Meteo API query.
- Changing the provider-agnostic forecast model.
- Changing the public `OpenMeteoForecastProvider` interface.
- Migrating the Open-Meteo air-quality provider again.
- Changing generated configuration files or the configuration schema.
- Adding a new per-provider HTTP abstraction.

## Current implementation

`src/open_meteo_weather_provider.cpp` currently:

1. Selects `WiFiClient` or `WiFiClientSecure` based on the transport macro.
2. Configures certificate verification or insecure TLS.
3. Builds the Open-Meteo forecast URI.
4. Calls the legacy `httpGetWithRetry()` helper.
5. Parses the response through `deserializeCall(Stream &, forecast_t &)`, which uses `consumeJsonStream()` and the SAX `WeatherHandler`.

The parser already supports bounded streaming and does not require a complete response buffer. Only the transport layer needs to change.

## Implementation steps

### 1. Replace legacy transport includes

In `src/open_meteo_weather_provider.cpp`:

- Remove `#include <WiFiClient.h>`.
- Remove the conditional `#include <WiFiClientSecure.h>`.
- Remove the `client_utils.h` include if it is no longer used elsewhere in the file.
- Add:
  - `esp_http_client_stream.h`
  - `esp_http_client_utils.h`

Keep `Arduino.h`, `cert.h`, `_locale.h`, `json_stream_utils.h`, and the provider headers as needed.

### 2. Build the full request URL

Retain the existing URI and all query parameters exactly as they are today:

- Latitude and longitude
- Current fields
- Hourly fields
- Daily fields
- `wind_speed_unit=ms`
- `timezone=auto`
- `timeformat=unixtime`
- `forecast_days=5`
- `forecast_hours=HOURLY_GRAPH_MAX`

Construct the URL with the existing transport macros:

- `http://` for `OPEN_METEO_FORECAST_TRANSPORT_HTTP`
- `https://` for both HTTPS modes

Continue using `OM_ENDPOINT` and retain the sanitized URL used for request logging.

### 3. Configure `esp_http_client`

Create an `esp_http_client_config_t` initialized to zero and set:

```cpp
config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
```

For `OPEN_METEO_FORECAST_TRANSPORT_HTTPS_VERIFY`, set:

```cpp
config.cert_pem = cert_ISRG_Root_X1;
```

Do not call `setInsecure()` for the no-verify mode. The ESP-IDF client uses the absence of `cert_pem` for the existing no-verification behavior used by the other migrated providers.

### 4. Use `espHttpGetWithRetry()`

Replace the `httpGetWithRetry()` call with `espHttpGetWithRetry(url, sanitizedUri, config, callback)`.

Inside the response callback:

1. Construct `EspHttpClientStream stream(client)`.
2. Call the existing `deserializeCall(stream, forecast)`.
3. Check `stream.hadReadError()` after parsing.
4. Convert a transport read failure with `espHttpErrorResult(stream.readError())`.
5. Otherwise return the parser result.

The callback must not close or clean up the ESP-IDF client. `espHttpGetWithRetry()` owns that lifecycle on every success and failure path.

### 5. Preserve parser behavior

Do not rewrite `WeatherHandler` or `deserializeCall()` unless compilation or transport error propagation requires a narrowly targeted change.

Preserve these guarantees:

- The forecast is reset before parsing.
- A failed parse leaves the forecast reset.
- Parsing is performed in bounded chunks without a DOM.
- Leading whitespace is ignored.
- Parsing stops once the root JSON document closes.
- Extra bytes after a completed JSON document are ignored.
- A response is accepted only when `current.time`, `hourly.time`, and `daily.time` are present and non-empty where required.
- Unknown fields and sections are ignored.
- Hourly and daily arrays are capped at the model capacities.
- Missing optional fields retain their existing default values.

### 6. Remove obsolete Arduino HTTP helpers

Once the forecast provider is migrated, remove the now-unused legacy transport code:

- Delete `httpGetWithRetry()` from `include/client_utils.h` and `src/client_utils.cpp`.
- Delete the unused `StreamInput` adapter from `include/client_utils.h`.
- Remove `HTTPClient.h`, `WiFiClient.h`, and related functional/provider-result dependencies that were needed only by those helpers.
- Keep `client_utils` itself because `startWiFi()` and `killWiFi()` are still used by `main.cpp`.
- Keep `getHttpResponsePhrase()` for HTTP status and WiFi status messages, but remove its obsolete Arduino `HTTPC_ERROR_*` cases and the `HTTPClient.h` dependency from `src/display_utils.cpp`.
- Remove stale test includes and comments referring to the deleted legacy helper.

### 7. Verify read-error precedence

Ensure the production callback follows the same pattern as the migrated providers:

```cpp
ProviderResult result = deserializeCall(stream, forecast);
if (stream.hadReadError())
  return espHttpErrorResult(stream.readError());
return result;
```

This ensures an ESP-IDF transport failure is not accidentally reported as a successful or merely incomplete parse result.

Expected classifications:

- HTTP status other than 200: handled by `espHttpGetWithRetry()`.
- Connection/open failure: handled by `espHttpGetWithRetry()`.
- Negative response read result: converted through `espHttpErrorResult()`.
- Empty response: existing empty-input parser error.
- Truncated JSON: existing incomplete-input parser error.
- Malformed JSON: existing invalid-input parser error.
- Valid JSON error payload without forecast time keys: existing invalid-input result.

## Files to change

### Expected production changes

- `src/open_meteo_weather_provider.cpp`
- `include/client_utils.h`
- `src/client_utils.cpp`
- `src/display_utils.cpp`

### Possible test change

- `test/src/test_openmeteo/open_meteo_weather_provider.inc`

The existing tests call `deserializeCall()` through an Arduino `StringStream`, so they should remain valid without modification. Add tests only if a focused regression case for the transport-compatible stream behavior is needed.

### Files expected to remain unchanged

- `include/open_meteo_weather_provider.h`
- `src/provider_factory.cpp`
- `scripts/config.py`
- `test/configs/openmeteo.yml`
- Generated `include/config.h`
- `include/esp_http_client_stream.h`
- `include/esp_http_client_utils.h`
- `src/esp_http_client_utils.cpp`

## Validation

Run the focused Open-Meteo QEMU suite:

```sh
bash test/run_tests_docker.sh --config openmeteo -v
```

This must continue to pass the weather provider tests covering:

- Provider name
- WMO weather-code mapping
- Current-field mapping
- Hourly mapping and day/night flags
- Daily mapping
- Array capacity limits
- Short arrays
- Optional fields
- Missing required fields
- Minimal valid forecast payloads
- Exponent-form numbers
- Invalid JSON
- Truncated bodies
- Trailing bytes after a valid document

Build the normal firmware:

```sh
~/.platformio/penv/bin/pio run -e lolin_d32
```

Verify formatting:

```sh
./scripts/format.sh --check
```

## Acceptance criteria

- No Open-Meteo forecast code uses `WiFiClient`, `WiFiClientSecure`, `HTTPClient`, or `httpGetWithRetry()`.
- Open-Meteo forecast requests use `esp_http_client` through `espHttpGetWithRetry()`.
- HTTPS verification still uses `cert_ISRG_Root_X1` when configured.
- Retry and HTTP status behavior remains consistent with the other ESP-IDF providers.
- Streaming parsing remains bounded and does not allocate the complete response.
- Existing Open-Meteo QEMU tests pass.
- The normal `lolin_d32` firmware build succeeds.
- Formatting checks pass.
