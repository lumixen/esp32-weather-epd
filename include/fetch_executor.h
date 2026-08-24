/* FetchExecutor — bounded FreeRTOS pool for provider fetches.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <memory>
#include <vector>
#include <freertos/FreeRTOS.h>
#include "fetch_operation.h"
#include "provider_result.h"

static constexpr size_t FETCH_MAX_CONCURRENCY = 2;
static constexpr size_t FETCH_STACK_BYTES = 8192;
static constexpr uint32_t FETCH_TASK_PRIORITY = 1;

/* Owns an in-flight asynchronous execution and all objects accessed by its
 * worker tasks. The handle must outlive the execution; destruction waits for
 * any remaining workers before releasing the operations. */
class FetchExecution {
 public:
  struct State;

  FetchExecution() = default;
  FetchExecution(FetchExecution &&) noexcept;
  FetchExecution &operator=(FetchExecution &&) noexcept;
  FetchExecution(const FetchExecution &) = delete;
  FetchExecution &operator=(const FetchExecution &) = delete;
  ~FetchExecution();

  bool wait(TickType_t timeout = portMAX_DELAY);
  bool isComplete() const;
  const std::vector<ProviderResult> &results() const;

 private:
  explicit FetchExecution(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;

  friend FetchExecution executeParallelAsync(std::vector<std::unique_ptr<FetchOperation>> operations);
};

FetchExecution executeParallelAsync(std::vector<std::unique_ptr<FetchOperation>> operations);
std::vector<ProviderResult> executeParallel(std::vector<std::unique_ptr<FetchOperation>> &ops);
