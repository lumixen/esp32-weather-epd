/* FetchOperation — generic scheduling handle for parallel provider fetches.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <vector>
#include "provider_result.h"

class FetchOperation {
 public:
  virtual ~FetchOperation() = default;
  virtual ProviderResult execute() = 0;
  virtual const char *name() const = 0;
  virtual bool shouldAbortOnFailure() const = 0;

  // Dependencies are non-owning references to operations submitted in the
  // same execution. They must be declared before the operation is submitted
  // to the executor. Repeating a dependency declaration is harmless.
  void dependsOn(const FetchOperation &operation) {
    const FetchOperation *dependency = &operation;
    for (const FetchOperation *existing : dependencies_) {
      if (existing == dependency) {
        return;
      }
    }
    dependencies_.push_back(dependency);
  }
  const std::vector<const FetchOperation *> &dependencies() const { return dependencies_; }

 private:
  std::vector<const FetchOperation *> dependencies_;
};
