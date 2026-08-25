/* FetchExecutor — bounded FreeRTOS pool for provider fetches.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fetch_executor.h"

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "logger.h"

namespace {

class DependencyScheduler {
  struct SkippedOperation {
    size_t operationIndex;
    size_t failedDependencyIndex;
  };

 public:
  enum class InitializeResult { Ready, InvalidGraph, ResourcesUnavailable };

  DependencyScheduler(std::vector<std::unique_ptr<FetchOperation>> &ops, std::vector<ProviderResult> &results)
      : ops_(ops), results_(results), operationCount_(ops.size()) {}

  ~DependencyScheduler() {
    if (readySem_ != nullptr) {
      vSemaphoreDelete(readySem_);
      readySem_ = nullptr;
    }
    if (doneSem_ != nullptr) {
      vSemaphoreDelete(doneSem_);
      doneSem_ = nullptr;
    }
    if (scratchMutex_ != nullptr) {
      vSemaphoreDelete(scratchMutex_);
      scratchMutex_ = nullptr;
    }
    if (mutex_ != nullptr) {
      vSemaphoreDelete(mutex_);
      mutex_ = nullptr;
    }
  }

  InitializeResult initialize() {
    if (operationCount_ == 0) {
      complete_.store(true, std::memory_order_release);
      return InitializeResult::Ready;
    }

    if (!buildGraph()) {
      markAllFailed("Invalid fetch dependency graph");
      return InitializeResult::InvalidGraph;
    }

    workerCount_ = operationCount_ < FETCH_MAX_CONCURRENCY ? operationCount_ : FETCH_MAX_CONCURRENCY;
    // Every operation can become ready at most once. Reserve the complete
    // queue before workers start so readiness propagation never allocates
    // while holding the scheduler mutex.
    ready_.reserve(operationCount_);
    for (size_t index = 0; index < operationCount_; ++index) {
      if (remainingDependencies_[index] == 0) {
        ready_.push_back(index);
      }
    }
    completedWithoutExecution_.reserve(operationCount_);
    skippedOperations_.reserve(operationCount_);

    mutex_ = xSemaphoreCreateMutex();
    scratchMutex_ = xSemaphoreCreateMutex();
    readySem_ = xSemaphoreCreateCounting(static_cast<UBaseType_t>(operationCount_), 0);
    doneSem_ = xSemaphoreCreateCounting(static_cast<UBaseType_t>(workerCount_), 0);
    if (mutex_ == nullptr || scratchMutex_ == nullptr || readySem_ == nullptr || doneSem_ == nullptr) {
      LOG_WARNING("FetchExecutor: scheduler synchronization creation failed");
      return InitializeResult::ResourcesUnavailable;
    }

    for (size_t i = 0; i < ready_.size(); ++i) {
      xSemaphoreGive(readySem_);
    }
    return InitializeResult::Ready;
  }

  bool isComplete() const { return complete_.load(std::memory_order_acquire); }
  size_t workerCount() const { return workerCount_; }
  SemaphoreHandle_t doneSemaphore() const { return doneSem_; }

  bool takeReadyBlocking(size_t &index) {
    while (true) {
      if (xSemaphoreTake(readySem_, portMAX_DELAY) != pdTRUE) {
        return false;
      }

      lock();
      if (finishedCount_ == operationCount_) {
        unlock();
        return false;
      }
      if (readyHead_ < ready_.size()) {
        index = ready_[readyHead_++];
        ++activeCount_;
        unlock();
        return true;
      }
      unlock();
    }
  }

  bool takeReadyNow(size_t &index) {
    lock();
    if (finishedCount_ == operationCount_ || readyHead_ >= ready_.size()) {
      unlock();
      return false;
    }
    index = ready_[readyHead_++];
    ++activeCount_;
    unlock();
    return true;
  }

  void executeClaimed(size_t index) {
    FetchOperation *op = ops_[index].get();
    const uint32_t start = millis();
    ProviderResult result = op->execute();
    LOG_DEBUG("FetchWorker %s: done in %ums ok=%d", op->name(), static_cast<unsigned>(millis() - start), result.isOk());
    completeOperation(index, result);
  }

  // Used by synchronous fallback paths after all worker tasks have stopped.
  void runSequentially() {
    while (!isComplete()) {
      size_t index = 0;
      if (!takeReadyNow(index)) {
        markUnscheduledFailed();
        return;
      }
      executeClaimed(index);
    }
  }

  // Mark work that cannot be scheduled because there are no workers left.
  void markUnscheduledFailed() {
    lock();
    for (size_t index = 0; index < operationCount_; ++index) {
      if (!completed_[index]) {
        results_[index] = ProviderResult::error("Failed to schedule fetch operation");
        completed_[index] = true;
        ++finishedCount_;
      }
    }
    complete_.store(true, std::memory_order_release);
    unlock();
  }

  void markAllFailed(const char *detail) {
    lock();
    for (size_t index = 0; index < operationCount_; ++index) {
      results_[index] = ProviderResult::error(detail);
      completed_[index] = true;
    }
    finishedCount_ = operationCount_;
    complete_.store(true, std::memory_order_release);
    unlock();
  }

 private:
  bool buildGraph() {
    dependents_.resize(operationCount_);
    dependencyIndexes_.resize(operationCount_);
    remainingDependencies_.assign(operationCount_, 0);
    dependencyFailed_.assign(operationCount_, false);
    completed_.assign(operationCount_, false);

    // The number of provider operations is intentionally small. A linear
    // lookup avoids imposing another container allocation on the ESP32.
    for (size_t index = 0; index < operationCount_; ++index) {
      for (const FetchOperation *dependency : ops_[index]->dependencies()) {
        size_t dependencyIndex = operationCount_;
        for (size_t candidate = 0; candidate < operationCount_; ++candidate) {
          if (ops_[candidate].get() == dependency) {
            dependencyIndex = candidate;
            break;
          }
        }
        if (dependencyIndex == operationCount_) {
          return false;
        }

        dependencyIndexes_[index].push_back(dependencyIndex);
        dependents_[dependencyIndex].push_back(index);
        ++remainingDependencies_[index];
      }
    }

    // Kahn's algorithm detects cycles without starting any worker tasks.
    std::vector<size_t> pending;
    pending.reserve(operationCount_);
    std::vector<size_t> unresolved = remainingDependencies_;
    for (size_t index = 0; index < operationCount_; ++index) {
      if (unresolved[index] == 0) {
        pending.push_back(index);
      }
    }

    size_t processed = 0;
    size_t cursor = 0;
    while (cursor < pending.size()) {
      const size_t index = pending[cursor++];
      ++processed;
      for (size_t dependent : dependents_[index]) {
        if (--unresolved[dependent] == 0) {
          pending.push_back(dependent);
        }
      }
    }
    return processed == operationCount_;
  }

  void completeOperation(size_t index, const ProviderResult &result) {
    size_t readySignals = 0;
    bool wakeWorkers = false;

    // Completion callbacks can run concurrently. Serialize access to the
    // preallocated scratch buffers, but keep logging outside the scheduler
    // mutex so another worker can continue resolving independent work.
    lockScratch();
    completedWithoutExecution_.clear();
    skippedOperations_.clear();

    lock();
    results_[index] = result;
    completed_[index] = true;
    --activeCount_;
    ++finishedCount_;
    completedWithoutExecution_.push_back(index);

    // A failed operation may make a whole dependent branch ineligible. Those
    // skipped nodes are resolved here so their own dependents can be released.
    size_t cursor = 0;
    while (cursor < completedWithoutExecution_.size()) {
      const size_t completedIndex = completedWithoutExecution_[cursor++];
      for (size_t dependent : dependents_[completedIndex]) {
        if (!results_[completedIndex].isOk()) {
          dependencyFailed_[dependent] = true;
        }
        if (--remainingDependencies_[dependent] != 0) {
          continue;
        }

        if (dependencyFailed_[dependent]) {
          size_t failedDependencyIndex = operationCount_;
          for (size_t dependency : dependencyIndexes_[dependent]) {
            if (!results_[dependency].isOk()) {
              failedDependencyIndex = dependency;
              break;
            }
          }
          // Set the terminal failure state while holding the lock. The
          // warning is produced after the lock is released.
          results_[dependent] = ProviderResult::error("Required fetch dependency failed");
          completed_[dependent] = true;
          ++finishedCount_;
          completedWithoutExecution_.push_back(dependent);
          skippedOperations_.push_back({dependent, failedDependencyIndex});
        } else {
          ready_.push_back(dependent);
          ++readySignals;
        }
      }
    }

    if (finishedCount_ == operationCount_) {
      complete_.store(true, std::memory_order_release);
      wakeWorkers = true;
    }
    unlock();

    if (readySem_ != nullptr) {
      for (size_t i = 0; i < readySignals; ++i) {
        xSemaphoreGive(readySem_);
      }
    }
    if (wakeWorkers && readySem_ != nullptr) {
      // Wake workers which are blocked waiting for a newly released task.
      // The semaphore capacity is at least the configured worker count.
      for (size_t i = 0; i < workerCount_; ++i) {
        xSemaphoreGive(readySem_);
      }
    }

    for (const SkippedOperation &skipped : skippedOperations_) {
      if (skipped.failedDependencyIndex < operationCount_) {
        LOG_WARNING("FetchWorker %s skipped: Required fetch dependency failed: %s",
                    ops_[skipped.operationIndex]->name(), ops_[skipped.failedDependencyIndex]->name());
      } else {
        LOG_WARNING("FetchWorker %s skipped: Required fetch dependency failed", ops_[skipped.operationIndex]->name());
      }
    }

    unlockScratch();
  }

  void lock() {
    if (mutex_ != nullptr) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
    }
  }

  void unlock() {
    if (mutex_ != nullptr) {
      xSemaphoreGive(mutex_);
    }
  }

  void lockScratch() {
    if (scratchMutex_ != nullptr) {
      xSemaphoreTake(scratchMutex_, portMAX_DELAY);
    }
  }

  void unlockScratch() {
    if (scratchMutex_ != nullptr) {
      xSemaphoreGive(scratchMutex_);
    }
  }

  std::vector<std::unique_ptr<FetchOperation>> &ops_;
  std::vector<ProviderResult> &results_;
  const size_t operationCount_;

  std::vector<std::vector<size_t>> dependents_;
  std::vector<std::vector<size_t>> dependencyIndexes_;
  std::vector<size_t> remainingDependencies_;
  std::vector<bool> dependencyFailed_;
  std::vector<bool> completed_;
  std::vector<size_t> ready_;
  std::vector<size_t> completedWithoutExecution_;
  std::vector<SkippedOperation> skippedOperations_;
  size_t readyHead_ = 0;
  size_t finishedCount_ = 0;
  size_t activeCount_ = 0;
  size_t workerCount_ = 0;

  SemaphoreHandle_t mutex_ = nullptr;
  SemaphoreHandle_t scratchMutex_ = nullptr;
  SemaphoreHandle_t readySem_ = nullptr;
  SemaphoreHandle_t doneSem_ = nullptr;
  std::atomic<bool> complete_{false};
};

void fetchWorker(void *pvParameters) {
  auto *scheduler = static_cast<DependencyScheduler *>(pvParameters);
  size_t index = 0;
  while (scheduler->takeReadyBlocking(index)) {
    scheduler->executeClaimed(index);
  }

  SemaphoreHandle_t doneSem = scheduler->doneSemaphore();
  xSemaphoreGive(doneSem);
  vTaskDelete(nullptr);
}

}  // namespace

struct FetchExecution::State {
  explicit State(std::vector<std::unique_ptr<FetchOperation>> &&operations)
      : ops(std::move(operations)), results(ops.size()), scheduler(ops, results) {}

  std::vector<std::unique_ptr<FetchOperation>> ops;
  std::vector<ProviderResult> results;
  DependencyScheduler scheduler;
  std::atomic<size_t> createdWorkers{0};
  size_t consumedWorkers = 0;
  bool runUnclaimedSequentially = false;
};

namespace {

bool runSequentialFallback(FetchExecution::State &state, TickType_t timeout, TickType_t start) {
  bool withinDeadline = true;
  while (!state.scheduler.isComplete()) {
    if (timeout != portMAX_DELAY && xTaskGetTickCount() - start >= timeout) {
      return false;
    }

    size_t index = 0;
    if (!state.scheduler.takeReadyNow(index)) {
      state.scheduler.markUnscheduledFailed();
      break;
    }
    state.scheduler.executeClaimed(index);
    if (timeout != portMAX_DELAY && xTaskGetTickCount() - start >= timeout) {
      withinDeadline = false;
    }
  }
  return withinDeadline;
}

bool waitForWorkers(FetchExecution::State &state, TickType_t timeout) {
  const size_t createdWorkers = state.createdWorkers.load(std::memory_order_acquire);
  if (state.scheduler.isComplete() && state.consumedWorkers == createdWorkers) {
    return true;
  }

  const TickType_t start = xTaskGetTickCount();
  while (state.consumedWorkers < createdWorkers) {
    TickType_t remaining = timeout;
    if (timeout != portMAX_DELAY) {
      const TickType_t elapsed = xTaskGetTickCount() - start;
      if (elapsed >= timeout) {
        return false;
      }
      remaining = timeout - elapsed;
    }
    if (xSemaphoreTake(state.scheduler.doneSemaphore(), remaining) != pdTRUE) {
      return false;
    }
    ++state.consumedWorkers;
  }

  bool withinDeadline = true;
  if (timeout != portMAX_DELAY && xTaskGetTickCount() - start >= timeout) {
    withinDeadline = false;
  }

  // A task-creation failure can leave ready operations behind. All workers
  // have stopped now, so it is safe to drain the graph on this task.
  if (state.runUnclaimedSequentially) {
    withinDeadline = runSequentialFallback(state, timeout, start) && withinDeadline;
    if (state.scheduler.isComplete()) {
      state.runUnclaimedSequentially = false;
    }
  }
  return withinDeadline;
}

void createWorkers(DependencyScheduler &scheduler, size_t &created) {
  for (size_t i = 0; i < scheduler.workerCount(); ++i) {
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "FW%u", static_cast<unsigned>(i));
    BaseType_t ok = xTaskCreate(fetchWorker, taskName, FETCH_STACK_BYTES, &scheduler, FETCH_TASK_PRIORITY, nullptr);
    if (ok != pdPASS) {
      break;
    }
    ++created;
  }
}

}  // namespace

FetchExecution::FetchExecution() = default;

FetchExecution::FetchExecution(std::unique_ptr<State> state) : state_(std::move(state)) {}

FetchExecution::FetchExecution(FetchExecution &&other) noexcept : state_(std::move(other.state_)) {}

FetchExecution &FetchExecution::operator=(FetchExecution &&other) noexcept {
  if (this != &other) {
    close();
    state_ = std::move(other.state_);
  }
  return *this;
}

FetchExecution::~FetchExecution() { close(); }

bool FetchExecution::wait(TickType_t timeout) {
  if (!state_) {
    return true;
  }
  return waitForWorkers(*state_, timeout);
}

void FetchExecution::close() {
  if (state_) {
    wait(portMAX_DELAY);
    state_.reset();
  }
}

bool FetchExecution::isComplete() const { return !state_ || state_->scheduler.isComplete(); }

const std::vector<ProviderResult> &FetchExecution::results() const {
  static const std::vector<ProviderResult> emptyResults;
  return state_ ? state_->results : emptyResults;
}

FetchExecution executeParallelAsync(std::vector<std::unique_ptr<FetchOperation>> operations) {
  auto state = std::make_unique<FetchExecution::State>(std::move(operations));
  LOG_DEBUG("FetchExecutor: async n=%u pool=%u", static_cast<unsigned>(state->ops.size()),
            static_cast<unsigned>(FETCH_MAX_CONCURRENCY));

  const auto initializeResult = state->scheduler.initialize();
  if (initializeResult == DependencyScheduler::InitializeResult::InvalidGraph) {
    return FetchExecution(std::move(state));
  }
  if (initializeResult == DependencyScheduler::InitializeResult::ResourcesUnavailable) {
    state->scheduler.markAllFailed("Failed to schedule asynchronous fetch operation");
    return FetchExecution(std::move(state));
  }
  if (state->ops.empty()) {
    return FetchExecution(std::move(state));
  }

  size_t created = 0;
  createWorkers(state->scheduler, created);
  state->createdWorkers.store(created, std::memory_order_release);

  if (created < state->scheduler.workerCount()) {
    LOG_WARNING("FetchExecutor: async worker task creation failed, running remainder on wait");
    state->runUnclaimedSequentially = true;
    if (created == 0) {
      // Do not execute operations inline: the async API must return promptly
      // even when task creation is unavailable.
      state->scheduler.markAllFailed("Failed to schedule asynchronous fetch operation");
      state->runUnclaimedSequentially = false;
    }
  }
  return FetchExecution(std::move(state));
}

std::vector<ProviderResult> executeParallel(std::vector<std::unique_ptr<FetchOperation>> &ops) {
  const size_t n = ops.size();
  LOG_DEBUG("FetchExecutor: n=%u pool=%u", static_cast<unsigned>(n), static_cast<unsigned>(FETCH_MAX_CONCURRENCY));
  std::vector<ProviderResult> results(n);
  if (n == 0) {
    return results;
  }

  // Preserve the existing no-task fast path for the common single-operation
  // provider while still validating graph metadata when it is present.
  if (n == 1 && ops[0]->dependencies().empty()) {
    results[0] = ops[0]->execute();
    return results;
  }

  DependencyScheduler scheduler(ops, results);
  const auto initializeResult = scheduler.initialize();
  if (initializeResult == DependencyScheduler::InitializeResult::InvalidGraph) {
    return results;
  }
  if (initializeResult == DependencyScheduler::InitializeResult::ResourcesUnavailable) {
    scheduler.runSequentially();
    return results;
  }
  if (n == 1) {
    scheduler.runSequentially();
    return results;
  }

  size_t created = 0;
  createWorkers(scheduler, created);
  if (created < scheduler.workerCount()) {
    LOG_WARNING("FetchExecutor: worker task creation failed, running remainder sequentially");
    for (size_t i = 0; i < created; ++i) {
      xSemaphoreTake(scheduler.doneSemaphore(), portMAX_DELAY);
    }
    scheduler.runSequentially();
    return results;
  }

  // Provider-level HTTP timeouts bound execution time.
  for (size_t i = 0; i < created; ++i) {
    xSemaphoreTake(scheduler.doneSemaphore(), portMAX_DELAY);
  }
  return results;
}
