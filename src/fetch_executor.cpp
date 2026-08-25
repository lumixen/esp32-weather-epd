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

#include "client_utils.h"
#include "logger.h"

struct FetchExecution::State {
  explicit State(std::vector<std::unique_ptr<FetchOperation>> &&operations)
      : ops(std::move(operations)), results(ops.size()) {}

  std::vector<std::unique_ptr<FetchOperation>> ops;
  std::vector<ProviderResult> results;
  std::atomic<size_t> nextIndex{0};
  std::atomic<size_t> finishedWorkers{0};
  std::atomic<size_t> createdWorkers{0};
  SemaphoreHandle_t doneSem = nullptr;
  size_t operationCount = 0;
  // Immutable after task creation begins. Workers use this value to detect
  // normal completion; failed creation is tracked separately in
  // createdWorkers so it never races with a worker.
  size_t workerCount = 0;
  size_t consumedWorkers = 0;
  size_t fallbackIndex = 0;
  bool fallbackIndexInitialized = false;
  std::atomic<bool> runUnclaimedSequentially{false};
  std::atomic<bool> complete{false};

  ~State() {
    if (doneSem != nullptr) {
      vSemaphoreDelete(doneSem);
      doneSem = nullptr;
    }
  }
};

namespace {

bool claimNext(FetchExecution::State &state, size_t &index) {
  size_t candidate = state.nextIndex.load(std::memory_order_relaxed);
  while (candidate < state.operationCount &&
         !state.nextIndex.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
  }
  if (candidate >= state.operationCount) {
    return false;
  }
  index = candidate;
  return true;
}

void fetchWorker(void *pvParameters) {
  auto *state = static_cast<FetchExecution::State *>(pvParameters);
  size_t index = 0;
  while (claimNext(*state, index)) {
    FetchOperation *op = state->ops[index].get();
    uint32_t t0 = millis();
    state->results[index] = op->execute();
    LOG_DEBUG("FetchWorker %s: done in %ums ok=%d", op->name(), static_cast<unsigned>(millis() - t0),
              state->results[index].isOk());
  }
  // Finish every access to State before signaling the handle. The semaphore
  // handle is copied locally because the token is the final operation before
  // this task deletes itself; no State member may be read after it is given.
  SemaphoreHandle_t doneSem = state->doneSem;
  const bool runUnclaimedSequentially = state->runUnclaimedSequentially.load(std::memory_order_acquire);
  const size_t workerCount = state->workerCount;
  if (!runUnclaimedSequentially && state->finishedWorkers.fetch_add(1, std::memory_order_acq_rel) + 1 == workerCount) {
    state->complete.store(true, std::memory_order_release);
  }
  xSemaphoreGive(doneSem);
  vTaskDelete(nullptr);
}

bool waitForWorkers(FetchExecution::State &state, TickType_t timeout) {
  // A worker sets complete immediately before giving its token. Do not take
  // this fast path until every completion token has also been accounted for;
  // otherwise the handle could destroy State while the worker is signaling.
  const size_t createdWorkers = state.createdWorkers.load(std::memory_order_acquire);
  if (state.complete.load(std::memory_order_acquire) && state.consumedWorkers == createdWorkers) {
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
    if (xSemaphoreTake(state.doneSem, remaining) != pdTRUE) {
      return false;
    }
    ++state.consumedWorkers;
  }

  bool withinDeadline = true;
  if (timeout != portMAX_DELAY && xTaskGetTickCount() - start >= timeout) {
    withinDeadline = false;
  }

  // A task-creation failure can leave an unclaimed suffix. It is important to
  // run that suffix only after all workers have stopped touching the state.
  if (state.runUnclaimedSequentially) {
    if (!state.fallbackIndexInitialized) {
      state.fallbackIndex = state.nextIndex.load(std::memory_order_acquire);
      state.fallbackIndexInitialized = true;
    }

    while (state.fallbackIndex < state.operationCount) {
      if (timeout != portMAX_DELAY) {
        const TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout) {
          return false;
        }
      }
      state.results[state.fallbackIndex] = state.ops[state.fallbackIndex]->execute();
      ++state.fallbackIndex;
      if (timeout != portMAX_DELAY && xTaskGetTickCount() - start >= timeout) {
        withinDeadline = false;
      }
    }
    state.runUnclaimedSequentially = false;
  }
  state.complete.store(true, std::memory_order_release);
  return withinDeadline;
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

bool FetchExecution::isComplete() const { return !state_ || state_->complete.load(std::memory_order_acquire); }

const std::vector<ProviderResult> &FetchExecution::results() const {
  static const std::vector<ProviderResult> emptyResults;
  return state_ ? state_->results : emptyResults;
}

FetchExecution executeParallelAsync(std::vector<std::unique_ptr<FetchOperation>> operations) {
  auto state = std::make_unique<FetchExecution::State>(std::move(operations));
  state->operationCount = state->ops.size();
  LOG_DEBUG("FetchExecutor: async n=%u pool=%u", static_cast<unsigned>(state->operationCount),
            static_cast<unsigned>(FETCH_MAX_CONCURRENCY));

  if (state->operationCount == 0) {
    state->complete.store(true, std::memory_order_release);
    return FetchExecution(std::move(state));
  }

  state->workerCount = state->operationCount < FETCH_MAX_CONCURRENCY ? state->operationCount : FETCH_MAX_CONCURRENCY;
  state->doneSem = xSemaphoreCreateCounting(state->workerCount, 0);
  if (state->doneSem == nullptr) {
    LOG_WARNING("FetchExecutor: async semaphore create failed, returning failed execution");
    for (size_t i = 0; i < state->operationCount; ++i) {
      state->results[i] = ProviderResult::error("Failed to schedule asynchronous fetch operation");
    }
    state->complete.store(true, std::memory_order_release);
    state->workerCount = 0;
    return FetchExecution(std::move(state));
  }

  size_t created = 0;
  for (size_t i = 0; i < state->workerCount; ++i) {
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "FA%u", static_cast<unsigned>(i));
    BaseType_t ok = xTaskCreate(fetchWorker, taskName, FETCH_STACK_BYTES, state.get(), FETCH_TASK_PRIORITY, nullptr);
    if (ok != pdPASS) {
      LOG_WARNING("FetchExecutor: async worker task creation failed, running remainder on wait");
      state->runUnclaimedSequentially = true;
      if (created == 0) {
        // Do not run operations inline: the async API must return promptly
        // even when task creation is unavailable.
        for (size_t index = 0; index < state->operationCount; ++index) {
          state->results[index] = ProviderResult::error("Failed to schedule asynchronous fetch operation");
        }
        state->runUnclaimedSequentially = false;
        state->complete.store(true, std::memory_order_release);
      }
      return FetchExecution(std::move(state));
    }
    ++created;
    state->createdWorkers.store(created, std::memory_order_release);
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
  if (n == 1) {
    results[0] = ops[0]->execute();
    return results;
  }

  const size_t workerCount = n < FETCH_MAX_CONCURRENCY ? n : FETCH_MAX_CONCURRENCY;
  SemaphoreHandle_t doneSem = xSemaphoreCreateCounting(workerCount, 0);

  if (doneSem == nullptr) {
    LOG_WARNING("FetchExecutor: semaphore create failed, falling back to sequential");
    for (size_t i = 0; i < n; ++i) {
      results[i] = ops[i]->execute();
    }
    return results;
  }

  // Keep the number of task stacks bounded: workers claim operations until none remain.
  struct WorkerContext {
    std::vector<std::unique_ptr<FetchOperation>> *ops;
    std::vector<ProviderResult> *results;
    size_t operationCount;
    std::atomic<size_t> nextIndex{0};
    SemaphoreHandle_t doneSem;
  };

  auto blockingWorker = [](void *pvParameters) {
    auto *context = static_cast<WorkerContext *>(pvParameters);
    size_t index = 0;
    while (true) {
      size_t candidate = context->nextIndex.load(std::memory_order_relaxed);
      while (candidate < context->operationCount &&
             !context->nextIndex.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed,
                                                       std::memory_order_relaxed)) {
      }
      if (candidate >= context->operationCount) {
        break;
      }
      index = candidate;
      FetchOperation *op = (*context->ops)[index].get();
      uint32_t t0 = millis();
      (*context->results)[index] = op->execute();
      LOG_DEBUG("FetchWorker %s: done in %ums ok=%d", op->name(), static_cast<unsigned>(millis() - t0),
                (*context->results)[index].isOk());
    }
    xSemaphoreGive(context->doneSem);
    vTaskDelete(nullptr);
  };

  WorkerContext context{&ops, &results, n, 0, doneSem};
  size_t created = 0;
  for (size_t i = 0; i < workerCount; ++i) {
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "FetchWorker%u", static_cast<unsigned>(i));
    BaseType_t ok = xTaskCreate(blockingWorker, taskName, FETCH_STACK_BYTES, &context, FETCH_TASK_PRIORITY, nullptr);
    if (ok != pdPASS) {
      LOG_WARNING("FetchExecutor: worker task creation failed, running remainder sequentially");
      // Wait for existing workers before accessing results or destroying doneSem.
      for (size_t j = 0; j < created; ++j) {
        xSemaphoreTake(doneSem, portMAX_DELAY);
      }
      const size_t firstUnclaimed = context.nextIndex.load(std::memory_order_acquire);
      for (size_t k = firstUnclaimed; k < n; ++k) {
        results[k] = ops[k]->execute();
      }
      vSemaphoreDelete(doneSem);
      return results;
    }
    ++created;
  }

  // Wait indefinitely for all workers. Provider-level HTTP timeouts bound execution time.
  for (size_t i = 0; i < created; ++i) {
    xSemaphoreTake(doneSem, portMAX_DELAY);
  }

  vSemaphoreDelete(doneSem);
  return results;
}
