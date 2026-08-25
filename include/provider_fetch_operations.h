/* Generic provider-owned fetch operations.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "fetch_operation.h"

/* Small adapter used by providers to describe arbitrary operations while
 * retaining optional dependency metadata from FetchOperation. */
class CallbackFetchOperation : public FetchOperation {
 public:
  CallbackFetchOperation(const char *name, bool abortOnFailure, std::function<ProviderResult()> callback)
      : name_(name), abortOnFailure_(abortOnFailure), callback_(std::move(callback)) {}

  ProviderResult execute() override { return callback_(); }
  const char *name() const override { return name_; }
  bool shouldAbortOnFailure() const override { return abortOnFailure_; }

 private:
  const char *name_;
  bool abortOnFailure_;
  std::function<ProviderResult()> callback_;
};
