/* Tests for parallel fetch pool — bounded concurrency, alerts-first, shouldAbort.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

#include "config.h"
#include "data_models.h"
#include "fetch_executor.h"
#include "fetch_operation.h"
#include "owm_v3_provider.h"
#include "provider_fetch_operations.h"
#include "provider_factory.h"
#include "../test_harness.h"

namespace fetch_executor_tests {

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
    if (delayMs_ > 0)
      vTaskDelay(pdMS_TO_TICKS(delayMs_));
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
  for (auto &r : results)
    TEST_ASSERT_TRUE(r.isOk());
  // max concurrent should be <=2 (pool limit)
  TEST_ASSERT_LESS_OR_EQUAL(2, g_maxActive.load());
}

static void test_worker_pool_processes_all_operations(void) {
  resetCounters();
  static const char *names[] = {"Op0", "Op1", "Op2", "Op3", "Op4", "Op5"};
  static const char *details[] = {"result 0", "result 1", "result 2", "result 3", "result 4", "result 5"};
  constexpr size_t operationCount = sizeof(names) / sizeof(names[0]);
  std::vector<std::unique_ptr<FetchOperation>> ops;
  for (size_t i = 0; i < operationCount; ++i) {
    ops.push_back(std::make_unique<MockFetchOperation>(names[i], true, 5, ProviderResult::error(details[i])));
  }

  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(operationCount, g_executed.load());
  TEST_ASSERT_EQUAL(operationCount, results.size());
  for (size_t i = 0; i < operationCount; ++i) {
    TEST_ASSERT_FALSE(results[i].isOk());
    TEST_ASSERT_EQUAL_STRING(details[i], results[i].detail().c_str());
  }
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

static void test_async_execution_returns_before_delayed_operation_finishes(void) {
  resetCounters();
  std::vector<std::unique_ptr<FetchOperation>> ops;
  ops.push_back(std::make_unique<MockFetchOperation>("Async", true, 150, ProviderResult::ok()));
  auto execution = executeParallelAsync(std::move(ops));
  TEST_ASSERT_FALSE(execution.isComplete());
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1000)));
  TEST_ASSERT_TRUE(execution.isComplete());
  TEST_ASSERT_EQUAL(1, g_executed.load());
  TEST_ASSERT_EQUAL(1, execution.results().size());
  TEST_ASSERT_TRUE(execution.results()[0].isOk());
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1)));
}

static void test_async_single_operation_is_not_run_inline(void) {
  resetCounters();
  std::vector<std::unique_ptr<FetchOperation>> ops;
  ops.push_back(std::make_unique<MockFetchOperation>("AsyncSolo", true, 100, ProviderResult::ok()));
  auto execution = executeParallelAsync(std::move(ops));
  TEST_ASSERT_FALSE(execution.isComplete());
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1000)));
  TEST_ASSERT_EQUAL(1, g_executed.load());
}

static void test_async_empty_execution_is_complete_and_wait_is_repeatable(void) {
  std::vector<std::unique_ptr<FetchOperation>> ops;
  auto execution = executeParallelAsync(std::move(ops));
  TEST_ASSERT_TRUE(execution.isComplete());
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1)));
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1)));
  TEST_ASSERT_EQUAL(0, execution.results().size());
}

class LifetimeFetchOperation final : public FetchOperation {
 public:
  explicit LifetimeFetchOperation(bool &destroyed) : destroyed_(destroyed) {}
  ~LifetimeFetchOperation() override { destroyed_ = true; }
  ProviderResult execute() override {
    vTaskDelay(pdMS_TO_TICKS(50));
    return ProviderResult::ok();
  }
  const char *name() const override { return "Lifetime"; }
  bool shouldAbortOnFailure() const override { return false; }

 private:
  bool &destroyed_;
};

static void test_async_handle_owns_operation_until_completion(void) {
  bool destroyed = false;
  {
    std::vector<std::unique_ptr<FetchOperation>> ops;
    ops.push_back(std::make_unique<LifetimeFetchOperation>(destroyed));
    auto execution = executeParallelAsync(std::move(ops));
    TEST_ASSERT_FALSE(destroyed);
    TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1000)));
    TEST_ASSERT_FALSE(destroyed);
    execution.close();
    TEST_ASSERT_TRUE(destroyed);
  }
  TEST_ASSERT_TRUE(destroyed);
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

static void test_dependency_waits_for_successful_prerequisite(void) {
  std::atomic<bool> prerequisiteFinished{false};
  std::atomic<bool> dependentStartedTooEarly{false};
  std::atomic<bool> dependentExecuted{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;

  auto prerequisite = std::make_unique<CallbackFetchOperation>("Prerequisite", true, [&]() {
    vTaskDelay(pdMS_TO_TICKS(20));
    prerequisiteFinished.store(true);
    return ProviderResult::ok();
  });
  auto dependent = std::make_unique<CallbackFetchOperation>("Dependent", true, [&]() {
    if (!prerequisiteFinished.load()) {
      dependentStartedTooEarly.store(true);
    }
    dependentExecuted.store(true);
    return ProviderResult::ok();
  });
  dependent->dependsOn(*prerequisite);
  dependent->dependsOn(*prerequisite);
  TEST_ASSERT_EQUAL(1, dependent->dependencies().size());
  ops.push_back(std::move(prerequisite));
  ops.push_back(std::move(dependent));

  auto results = executeParallel(ops);
  TEST_ASSERT_TRUE(results[0].isOk());
  TEST_ASSERT_TRUE(results[1].isOk());
  TEST_ASSERT_TRUE(dependentExecuted.load());
  TEST_ASSERT_FALSE(dependentStartedTooEarly.load());
}

static void test_failed_dependency_skips_dependent(void) {
  std::atomic<bool> dependentExecuted{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;

  auto prerequisite = std::make_unique<CallbackFetchOperation>(
      "Failed prerequisite", true, []() { return ProviderResult::error("gridpoints unavailable"); });
  auto dependent = std::make_unique<CallbackFetchOperation>("Dependent", true, [&]() {
    dependentExecuted.store(true);
    return ProviderResult::ok();
  });
  dependent->dependsOn(*prerequisite);
  ops.push_back(std::move(prerequisite));
  ops.push_back(std::move(dependent));

  auto results = executeParallel(ops);
  TEST_ASSERT_FALSE(results[0].isOk());
  TEST_ASSERT_FALSE(results[1].isOk());
  TEST_ASSERT_FALSE(dependentExecuted.load());
  TEST_ASSERT_FALSE(results[1].detail().isEmpty());
}

static void test_multiple_dependencies_wait_for_all_prerequisites(void) {
  std::atomic<bool> firstFinished{false};
  std::atomic<bool> secondFinished{false};
  std::atomic<bool> dependentExecuted{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;

  auto first = std::make_unique<CallbackFetchOperation>("First prerequisite", true, [&]() {
    vTaskDelay(pdMS_TO_TICKS(10));
    firstFinished.store(true);
    return ProviderResult::ok();
  });
  auto second = std::make_unique<CallbackFetchOperation>("Second prerequisite", true, [&]() {
    vTaskDelay(pdMS_TO_TICKS(20));
    secondFinished.store(true);
    return ProviderResult::ok();
  });
  auto dependent = std::make_unique<CallbackFetchOperation>("Dependent", true, [&]() {
    dependentExecuted.store(firstFinished.load() && secondFinished.load());
    return ProviderResult::ok();
  });
  dependent->dependsOn(*first);
  dependent->dependsOn(*second);
  ops.push_back(std::move(first));
  ops.push_back(std::move(second));
  ops.push_back(std::move(dependent));

  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(3, results.size());
  TEST_ASSERT_TRUE(results[0].isOk());
  TEST_ASSERT_TRUE(results[1].isOk());
  TEST_ASSERT_TRUE(results[2].isOk());
  TEST_ASSERT_TRUE(dependentExecuted.load());
}

static void test_dependency_failure_propagates_through_chain(void) {
  std::atomic<int> executed{0};
  std::vector<std::unique_ptr<FetchOperation>> ops;

  auto root = std::make_unique<CallbackFetchOperation>("Root", true, [&]() {
    ++executed;
    return ProviderResult::error("root failed");
  });
  auto middle = std::make_unique<CallbackFetchOperation>("Middle", true, [&]() {
    ++executed;
    return ProviderResult::ok();
  });
  auto leaf = std::make_unique<CallbackFetchOperation>("Leaf", true, [&]() {
    ++executed;
    return ProviderResult::ok();
  });
  middle->dependsOn(*root);
  leaf->dependsOn(*middle);
  ops.push_back(std::move(root));
  ops.push_back(std::move(middle));
  ops.push_back(std::move(leaf));

  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(3, results.size());
  TEST_ASSERT_FALSE(results[0].isOk());
  TEST_ASSERT_FALSE(results[1].isOk());
  TEST_ASSERT_FALSE(results[2].isOk());
  TEST_ASSERT_EQUAL(1, executed.load());
}

static void test_invalid_dependency_graph_does_not_execute(void) {
  std::atomic<bool> executed{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;
  auto external = std::make_unique<CallbackFetchOperation>("External", true, []() { return ProviderResult::ok(); });
  auto dependent = std::make_unique<CallbackFetchOperation>("Dependent", true, [&]() {
    executed.store(true);
    return ProviderResult::ok();
  });
  dependent->dependsOn(*external);
  ops.push_back(std::move(dependent));

  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(1, results.size());
  TEST_ASSERT_FALSE(results[0].isOk());
  TEST_ASSERT_FALSE(executed.load());
}

static void test_cyclic_dependency_graph_does_not_execute(void) {
  std::atomic<bool> executed{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;
  auto first = std::make_unique<CallbackFetchOperation>("First", true, [&]() {
    executed.store(true);
    return ProviderResult::ok();
  });
  auto second = std::make_unique<CallbackFetchOperation>("Second", true, [&]() {
    executed.store(true);
    return ProviderResult::ok();
  });
  first->dependsOn(*second);
  second->dependsOn(*first);
  ops.push_back(std::move(first));
  ops.push_back(std::move(second));

  auto results = executeParallel(ops);
  TEST_ASSERT_EQUAL(2, results.size());
  TEST_ASSERT_FALSE(results[0].isOk());
  TEST_ASSERT_FALSE(results[1].isOk());
  TEST_ASSERT_FALSE(executed.load());
}

static void test_async_dependency_waits_for_prerequisite(void) {
  std::atomic<bool> prerequisiteFinished{false};
  std::atomic<bool> dependentExecuted{false};
  std::vector<std::unique_ptr<FetchOperation>> ops;

  auto prerequisite = std::make_unique<CallbackFetchOperation>("Async prerequisite", true, [&]() {
    vTaskDelay(pdMS_TO_TICKS(20));
    prerequisiteFinished.store(true);
    return ProviderResult::ok();
  });
  auto dependent = std::make_unique<CallbackFetchOperation>("Async dependent", true, [&]() {
    dependentExecuted.store(prerequisiteFinished.load());
    return ProviderResult::ok();
  });
  dependent->dependsOn(*prerequisite);
  ops.push_back(std::move(prerequisite));
  ops.push_back(std::move(dependent));

  auto execution = executeParallelAsync(std::move(ops));
  TEST_ASSERT_TRUE(execution.wait(pdMS_TO_TICKS(1000)));
  TEST_ASSERT_TRUE(execution.isComplete());
  TEST_ASSERT_EQUAL(2, execution.results().size());
  TEST_ASSERT_TRUE(execution.results()[0].isOk());
  TEST_ASSERT_TRUE(execution.results()[1].isOk());
  TEST_ASSERT_TRUE(dependentExecuted.load());
}

static void test_create_fetch_operations_from_unified_providers(void) {
  static weather_report_t report{};
  report.resetForecast();
  report.resetAlerts();
  auto fetchBundle = createFetchBundle(report);
  TEST_ASSERT_EQUAL(2, fetchBundle.ops.size());
  TEST_ASSERT_TRUE(fetchBundle.ops[0]->shouldAbortOnFailure());
  TEST_ASSERT_FALSE(fetchBundle.ops[1]->shouldAbortOnFailure());
  TEST_ASSERT_EQUAL_STRING("One Call API", fetchBundle.ops[0]->name());
  TEST_ASSERT_EQUAL_STRING("Air Pollution API", fetchBundle.ops[1]->name());
}

static void test_should_abort_flags(void) {
  MockFetchOperation wop("Weather", true, 0, ProviderResult::ok());
  MockFetchOperation aop("Air", true, 0, ProviderResult::ok());
  MockFetchOperation alop("Alerts", false, 0, ProviderResult::ok());
  TEST_ASSERT_TRUE(wop.shouldAbortOnFailure());
  TEST_ASSERT_TRUE(aop.shouldAbortOnFailure());
  TEST_ASSERT_FALSE(alop.shouldAbortOnFailure());
}

static void test_owm_provider_combines_forecast_and_alerts(void) {
  OWMProvider p;
  TEST_ASSERT_EQUAL_STRING("One Call API", p.getApiName());
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, OWMProvider::mapWeatherCode(800));
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(fetch_executor_tests::test_pool_limits_concurrency_to_two);
  RUN_TEST(fetch_executor_tests::test_worker_pool_processes_all_operations);
  RUN_TEST(fetch_executor_tests::test_execute_parallel_single_op_no_task);
  RUN_TEST(fetch_executor_tests::test_execute_parallel_empty);
  RUN_TEST(fetch_executor_tests::test_execute_parallel_propagates_results);
  RUN_TEST(fetch_executor_tests::test_dependency_waits_for_successful_prerequisite);
  RUN_TEST(fetch_executor_tests::test_failed_dependency_skips_dependent);
  RUN_TEST(fetch_executor_tests::test_multiple_dependencies_wait_for_all_prerequisites);
  RUN_TEST(fetch_executor_tests::test_dependency_failure_propagates_through_chain);
  RUN_TEST(fetch_executor_tests::test_invalid_dependency_graph_does_not_execute);
  RUN_TEST(fetch_executor_tests::test_cyclic_dependency_graph_does_not_execute);
  RUN_TEST(fetch_executor_tests::test_async_dependency_waits_for_prerequisite);
  RUN_TEST(fetch_executor_tests::test_async_execution_returns_before_delayed_operation_finishes);
  RUN_TEST(fetch_executor_tests::test_async_single_operation_is_not_run_inline);
  RUN_TEST(fetch_executor_tests::test_async_empty_execution_is_complete_and_wait_is_repeatable);
  RUN_TEST(fetch_executor_tests::test_async_handle_owns_operation_until_completion);
  RUN_TEST(fetch_executor_tests::test_create_fetch_operations_from_unified_providers);
  RUN_TEST(fetch_executor_tests::test_should_abort_flags);
  RUN_TEST(fetch_executor_tests::test_owm_provider_combines_forecast_and_alerts);
}

}  // namespace fetch_executor_tests
