/* FetchExecutor — bounded FreeRTOS pool for provider fetches.
 * Copyright (C) 2026  Lumixen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fetch_executor.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "client_utils.h"
#include "logger.h"

namespace {

struct TaskArgs {
  FetchOperation *op;
  ProviderResult *out;
  SemaphoreHandle_t poolSem;
  SemaphoreHandle_t doneSem;
};

void fetchTask(void *pvParameters) {
  TaskArgs *args = static_cast<TaskArgs *>(pvParameters);
  if (xSemaphoreTake(args->poolSem, portMAX_DELAY) == pdTRUE) {
    uint32_t t0 = millis();
    *(args->out) = args->op->execute();
    LOG_DEBUG("FetchTask %s: done in %ums ok=%d", args->op->name(), static_cast<unsigned>(millis() - t0),
              args->out->isOk());
    xSemaphoreGive(args->poolSem);
  } else {
    *(args->out) = ProviderResult::error("pool take failed");
  }
  xSemaphoreGive(args->doneSem);
  delete args;
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

  SemaphoreHandle_t poolSem = xSemaphoreCreateCounting(FETCH_MAX_CONCURRENCY, FETCH_MAX_CONCURRENCY);
  SemaphoreHandle_t doneSem = xSemaphoreCreateCounting(n, 0);

  if (poolSem == nullptr || doneSem == nullptr) {
    LOG_WARNING("FetchExecutor: semaphore create failed, falling back to sequential");
    if (poolSem) vSemaphoreDelete(poolSem);
    if (doneSem) vSemaphoreDelete(doneSem);
    for (size_t i = 0; i < n; ++i) {
      results[i] = ops[i]->execute();
    }
    return results;
  }

  size_t created = 0;
  for (size_t i = 0; i < n; ++i) {
    TaskArgs *args = new TaskArgs{ops[i].get(), &results[i], poolSem, doneSem};
    char taskName[16];
    snprintf(taskName, sizeof(taskName), "Fetch%u", static_cast<unsigned>(i));
    BaseType_t ok = xTaskCreate(fetchTask, taskName, FETCH_STACK_BYTES, args, FETCH_TASK_PRIORITY, nullptr);
    if (ok != pdPASS) {
      LOG_WARNING("FetchExecutor: xTaskCreate failed for %s, running sequentially", ops[i]->name());
      delete args;
      // Fallback: wait indefinitely for already-created tasks to complete before
      // touching results/poolSem/doneSem to avoid use-after-free
      for (size_t j = 0; j < created; ++j) {
        xSemaphoreTake(doneSem, portMAX_DELAY);
      }
      // Run current and remaining sequentially
      for (size_t k = i; k < n; ++k) {
        results[k] = ops[k]->execute();
      }
      vSemaphoreDelete(poolSem);
      vSemaphoreDelete(doneSem);
      return results;
    }
    ++created;
  }

  // Wait indefinitely for all tasks — bounded by HTTP timeouts (3×5000 + 3×30000)
  // to avoid use-after-free on poolSem/doneSem/results
  for (size_t i = 0; i < created; ++i) {
    xSemaphoreTake(doneSem, portMAX_DELAY);
  }

  vSemaphoreDelete(poolSem);
  vSemaphoreDelete(doneSem);
  return results;
}
