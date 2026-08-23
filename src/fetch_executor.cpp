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

namespace {

struct WorkerContext {
  std::vector<std::unique_ptr<FetchOperation>> *ops;
  std::vector<ProviderResult> *results;
  size_t operationCount;
  std::atomic<size_t> nextIndex{0};
  SemaphoreHandle_t doneSem;
};

bool claimNext(WorkerContext &context, size_t &index) {
  size_t candidate = context.nextIndex.load(std::memory_order_relaxed);
  while (candidate < context.operationCount &&
         !context.nextIndex.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed,
                                                  std::memory_order_relaxed)) {
  }
  if (candidate >= context.operationCount) {
    return false;
  }
  index = candidate;
  return true;
}

void fetchWorker(void *pvParameters) {
  WorkerContext *context = static_cast<WorkerContext *>(pvParameters);
  size_t index = 0;
  while (claimNext(*context, index)) {
    FetchOperation *op = (*context->ops)[index].get();
    uint32_t t0 = millis();
    (*context->results)[index] = op->execute();
    LOG_DEBUG("FetchWorker %s: done in %ums ok=%d", op->name(), static_cast<unsigned>(millis() - t0),
              (*context->results)[index].isOk());
  }
  xSemaphoreGive(context->doneSem);
  vTaskDelete(nullptr);
}

}  // namespace

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
  WorkerContext context{&ops, &results, n, 0, doneSem};
  size_t created = 0;
  for (size_t i = 0; i < workerCount; ++i) {
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "FetchWorker%u", static_cast<unsigned>(i));
    BaseType_t ok = xTaskCreate(fetchWorker, taskName, FETCH_STACK_BYTES, &context, FETCH_TASK_PRIORITY, nullptr);
    if (ok != pdPASS) {
      LOG_WARNING("FetchExecutor: worker task creation failed, running remainder sequentially");
      // Wait for existing workers before accessing results or destroying doneSem.
      for (size_t j = 0; j < created; ++j) {
        xSemaphoreTake(doneSem, portMAX_DELAY);
      }
      // Workers claim indices monotonically; after they finish, all claimed operations
      // are complete and the unclaimed suffix can run sequentially.
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
