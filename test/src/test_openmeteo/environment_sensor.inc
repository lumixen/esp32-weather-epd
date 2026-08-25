/* Tests for the hardware-independent local environment sensor operation.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#include "environment_sensor_fetch_operation.h"
#include "fetch_executor.h"
#include "../test_harness.h"

namespace environment_sensor_tests {

void setUp(void) {}
void tearDown(void) {}

class FakeSensor final : public EnvSensor {
 public:
  FakeSensor(bool beginResult, std::optional<float> temperature, std::optional<float> humidity,
             std::optional<float> pressure, bool *destroyed = nullptr)
      : beginResult_(beginResult),
        temperature_(temperature),
        humidity_(humidity),
        pressure_(pressure),
        destroyed_(destroyed) {}
  ~FakeSensor() override {
    if (destroyed_ != nullptr) {
      *destroyed_ = true;
    }
  }

  bool begin() override {
    calls.push_back(0);
    return beginResult_;
  }
  std::optional<float> getTemperature() override {
    calls.push_back(1);
    return temperature_;
  }
  std::optional<float> getHumidity() override {
    calls.push_back(2);
    return humidity_;
  }
  std::optional<float> getPressure() override {
    calls.push_back(3);
    return pressure_;
  }

  std::vector<int> calls;

 private:
  bool beginResult_;
  std::optional<float> temperature_;
  std::optional<float> humidity_;
  std::optional<float> pressure_;
  bool *destroyed_;
};

static sensor_readings sentinelReadings() { return {.temperature = 1.0f, .humidity = 2.0f, .pressure = 3.0f}; }

static void test_successful_readings_are_published_atomically(void) {
  weather_report_t report{};
  report.sensor = sentinelReadings();
  bool destroyed = false;
  auto sensor = std::make_unique<FakeSensor>(true, 21.5f, 48.0f, 1012.3f, &destroyed);
  auto *fake = sensor.get();
  EnvironmentSensorFetchOperation operation(report, std::move(sensor));

  ProviderResult result = operation.execute();
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(21.5f, report.sensor.temperature.value());
  TEST_ASSERT_EQUAL_FLOAT(48.0f, report.sensor.humidity.value());
  TEST_ASSERT_EQUAL_FLOAT(1012.3f, report.sensor.pressure.value());
  TEST_ASSERT_EQUAL(4, fake->calls.size());
  TEST_ASSERT_EQUAL(0, fake->calls[0]);
  TEST_ASSERT_EQUAL(1, fake->calls[1]);
  TEST_ASSERT_EQUAL(2, fake->calls[2]);
  TEST_ASSERT_EQUAL(3, fake->calls[3]);
  TEST_ASSERT_FALSE(destroyed);
}

static void test_unavailable_values_are_optional(void) {
  weather_report_t report{};
  auto sensor = std::make_unique<FakeSensor>(true, 19.0f, std::nullopt, 1000.0f);
  EnvironmentSensorFetchOperation operation(report, std::move(sensor));

  TEST_ASSERT_TRUE(operation.execute().isOk());
  TEST_ASSERT_TRUE(report.sensor.temperature.has_value());
  TEST_ASSERT_FALSE(report.sensor.humidity.has_value());
  TEST_ASSERT_TRUE(report.sensor.pressure.has_value());
}

static void test_initialization_failure_clears_stale_data_and_does_not_abort(void) {
  weather_report_t report{};
  report.sensor = sentinelReadings();
  bool destroyed = false;
  auto sensor = std::make_unique<FakeSensor>(false, 1.0f, 2.0f, 3.0f, &destroyed);
  auto *fake = sensor.get();
  auto operation = std::make_unique<EnvironmentSensorFetchOperation>(report, std::move(sensor));

  ProviderResult result = operation->execute();
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_FALSE(operation->shouldAbortOnFailure());
  TEST_ASSERT_EQUAL(1, fake->calls.size());
  TEST_ASSERT_FALSE(report.sensor.temperature.has_value());
  TEST_ASSERT_FALSE(report.sensor.humidity.has_value());
  TEST_ASSERT_FALSE(report.sensor.pressure.has_value());
  operation.reset();
  TEST_ASSERT_TRUE(destroyed);
}

static void test_factory_is_disabled_without_local_provider(void) {
#if defined(BME_TYPE_NONE)
  weather_report_t report{};
  TEST_ASSERT_NULL(createEnvironmentSensorOperation(report).get());
#endif
}

static SemaphoreHandle_t blockedGetterEntered;
static SemaphoreHandle_t blockedGetterRelease;

class BlockingSensor final : public EnvSensor {
 public:
  bool begin() override { return true; }
  std::optional<float> getTemperature() override {
    xSemaphoreGive(blockedGetterEntered);
    xSemaphoreTake(blockedGetterRelease, portMAX_DELAY);
    return 22.0f;
  }
  std::optional<float> getHumidity() override { return 40.0f; }
  std::optional<float> getPressure() override { return 1008.0f; }
};

static void test_async_sensor_operation_keeps_report_unpublished_until_complete(void) {
  weather_report_t report{};
  blockedGetterEntered = xSemaphoreCreateBinary();
  blockedGetterRelease = xSemaphoreCreateBinary();
  TEST_ASSERT_NOT_NULL(blockedGetterEntered);
  TEST_ASSERT_NOT_NULL(blockedGetterRelease);

  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<EnvironmentSensorFetchOperation>(report, std::make_unique<BlockingSensor>()));
  auto execution = executeParallelAsync(std::move(operations));
  TEST_ASSERT_TRUE(xSemaphoreTake(blockedGetterEntered, pdMS_TO_TICKS(1000)) == pdTRUE);
  TEST_ASSERT_FALSE(execution.isComplete());
  TEST_ASSERT_FALSE(report.sensor.temperature.has_value());

  xSemaphoreGive(blockedGetterRelease);
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1000)));
  TEST_ASSERT_TRUE(report.sensor.temperature.has_value());
  TEST_ASSERT_EQUAL_FLOAT(22.0f, report.sensor.temperature.value());
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1)));

  vSemaphoreDelete(blockedGetterEntered);
  vSemaphoreDelete(blockedGetterRelease);
  blockedGetterEntered = nullptr;
  blockedGetterRelease = nullptr;
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(environment_sensor_tests::test_successful_readings_are_published_atomically);
  RUN_TEST(environment_sensor_tests::test_unavailable_values_are_optional);
  RUN_TEST(environment_sensor_tests::test_initialization_failure_clears_stale_data_and_does_not_abort);
  RUN_TEST(environment_sensor_tests::test_factory_is_disabled_without_local_provider);
  RUN_TEST(environment_sensor_tests::test_async_sensor_operation_keeps_report_unpublished_until_complete);
}

}  // namespace environment_sensor_tests
