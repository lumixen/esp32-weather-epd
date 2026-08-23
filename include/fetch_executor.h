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
#include "fetch_operation.h"
#include "provider_result.h"

static constexpr size_t FETCH_MAX_CONCURRENCY = 2;
static constexpr size_t FETCH_STACK_BYTES = 8192;
static constexpr uint32_t FETCH_TASK_PRIORITY = 1;

std::vector<ProviderResult> executeParallel(std::vector<std::unique_ptr<FetchOperation>> &ops);
