/* Tests for parallel fetch pool — bounded concurrency, alerts-first, shouldAbort.
 * Copyright (C) 2026  Lumixen
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

#include "data_models.h"
#include "fetch_executor.h"
#include "fetch_operation.h"
#include "owm_provider.h"
#include "provider_fetch_operations.h"
#include "provider_factory.h"

void setUp(void) {}
void tearDown(void) {}

// Mock operation for concurrency testing
static std::atomic<int> g_active{0};
static std::atomic<int> g_maxActive{0};
static std::atomic<int> g_executed{0};

class MockFetchOperation : public FetchOperation {
 public:
  MockFetchOperation(const char *name, bool critical, uint32_t delayMs, ProviderResult result)
      : name_(name), critical_(critical), delayMs_(delayMs), result_(result) {}
  ProviderResult execute() override {
    int cur = ++g_active;
    int prevMax = g_maxActive.load();
    while (cur > prevMax && !g_maxActive.compare_exchange_weak(prevMax, cur)) {
    }
    g_executed++;
    if (delayMs_ > 0) vTaskDelay(pdMS_TO_TICKS(delayMs_));
    --g_active;
    return result_;
  }
  const char *name() const override { return name_; }
  bool shouldAbortOnFailure() const override { return critical_; }

 private:
  const char *name_;
  bool critical_;
  uint32_t delayMs_;
  ProviderResult result_;
};

static void resetCounters() {
  g_active = 0;
  g_maxActive = 0;
  g_executed = 0;
}

static void test_pool_limits_concurrency_to_two(void) {
  resetCounters();
  std::vector<std::unique_ptr<FetchOperation>> ops;
  // Use small delays to test pool, but not too long to avoid QEMU hang
  ops.push_back(std::make_unique<MockFetchOperation>("A", true, 20, ProviderResult::ok()));
  ops.push_back(std::make_unique<MockFetchOperation>("B", true, 20, ProviderResult::ok()));
  ops.push_back(std::make_unique<MockFetchOperation>("C", true, 20, ProviderResult::ok()));
  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(3, g_executed.load());
  TEST_ASSERT_EQUAL(3, results.size());
  for (auto &r : results) TEST_ASSERT_TRUE(r.isOk());
  // max concurrent should be <=2 (pool limit)
  TEST_ASSERT_LESS_OR_EQUAL(2, g_maxActive.load());
}

static void test_execute_parallel_single_op_no_task(void) {
  resetCounters();
  std::vector<std::unique_ptr<FetchOperation>> ops;
  ops.push_back(std::make_unique<MockFetchOperation>("Solo", true, 10, ProviderResult::ok()));
  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(1, results.size());
  TEST_ASSERT_TRUE(results[0].isOk());
  TEST_ASSERT_EQUAL(1, g_executed.load());
}

static void test_execute_parallel_empty(void) {
  std::vector<std::unique_ptr<FetchOperation>> ops;
  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(0, results.size());
}

static void test_execute_parallel_propagates_results(void) {
  resetCounters();
  std::vector<std::unique_ptr<FetchOperation>> ops;
  ops.push_back(std::make_unique<MockFetchOperation>("Ok", true, 5, ProviderResult::ok()));
  ops.push_back(std::make_unique<MockFetchOperation>("Err", false, 5, ProviderResult::error("fail detail")));
  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(2, results.size());
  TEST_ASSERT_TRUE(results[0].isOk());
  TEST_ASSERT_FALSE(results[1].isOk());
  TEST_ASSERT_EQUAL_STRING("fail detail", results[1].detail().c_str());
}

static void test_create_fetch_operations_alerts_first(void) {
  forecast_t fc;
  air_quality_t aq;
  std::vector<weather_alert_t> alerts;
  auto fetchBundle = createFetchBundle(fc, aq, alerts);
  auto &ops = fetchBundle.ops;
  auto &providers = fetchBundle.providers;
  if (providers.weather && providers.airQuality && providers.alert) {
    TEST_ASSERT_GREATER_OR_EQUAL(1, ops.size());
    TEST_ASSERT_EQUAL_STRING("Alerts API", ops[0]->name());
    TEST_ASSERT_FALSE(ops[0]->shouldAbortOnFailure());
    bool foundWeather = false, foundAir = false;
    for (size_t i = 1; i < ops.size(); ++i) {
      if (String(ops[i]->name()) == String(providers.weather->getApiName())) foundWeather = true;
      if (String(ops[i]->name()) == "Air Pollution API") foundAir = true;
    }
    TEST_ASSERT_TRUE(foundWeather);
    TEST_ASSERT_TRUE(foundAir);
#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP) && defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
    // OWM weather and alerts must be two interface views of one provider.
    auto *weatherOwm = dynamic_cast<OWMProvider *>(providers.weather.get());
    auto *alertsOwm = dynamic_cast<OWMProvider *>(providers.alert.get());
    TEST_ASSERT_NOT_NULL(weatherOwm);
    TEST_ASSERT_NOT_NULL(alertsOwm);
    TEST_ASSERT_EQUAL_PTR(weatherOwm, alertsOwm);
#endif
  } else {
    TEST_PASS_MESSAGE("Skipped — not all providers configured for this test config");
  }
  // providers owned by shared_ptr, no manual delete
}

static void test_should_abort_flags(void) {
  MockFetchOperation wop("Weather", true, 0, ProviderResult::ok());
  MockFetchOperation aop("Air", true, 0, ProviderResult::ok());
  MockFetchOperation alop("Alerts", false, 0, ProviderResult::ok());
  TEST_ASSERT_TRUE(wop.shouldAbortOnFailure());
  TEST_ASSERT_TRUE(aop.shouldAbortOnFailure());
  TEST_ASSERT_FALSE(alop.shouldAbortOnFailure());
}

static void test_owm_provider_always_defined(void) {
  // OWMProvider should be always defined now, even when config is openmeteo
  OWMProvider p;
  TEST_ASSERT_EQUAL_STRING("One Call API", p.getApiName());
  // mapWeatherCode should work
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, OWMProvider::mapWeatherCode(800));
}

void setup() {
  delay(200);
  UNITY_BEGIN();
  RUN_TEST(test_pool_limits_concurrency_to_two);
  RUN_TEST(test_execute_parallel_single_op_no_task);
  RUN_TEST(test_execute_parallel_empty);
  RUN_TEST(test_execute_parallel_propagates_results);
  RUN_TEST(test_create_fetch_operations_alerts_first);
  RUN_TEST(test_should_abort_flags);
  RUN_TEST(test_owm_provider_always_defined);
  UNITY_END();
}

void loop() {}
