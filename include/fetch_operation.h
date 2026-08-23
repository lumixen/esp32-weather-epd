/* FetchOperation — generic scheduling handle for parallel provider fetches.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "provider_result.h"

class FetchOperation {
 public:
  virtual ~FetchOperation() = default;
  virtual ProviderResult execute() = 0;
  virtual const char *name() const = 0;
  virtual bool shouldAbortOnFailure() const = 0;
};
