# Fetch executor dependency scheduling plan

## Goal

Allow fetch operations to declare prerequisites while preserving the existing bounded FreeRTOS execution model. A dependent operation must not start until its prerequisites have completed successfully.

The primary use case is a NOAA provider with two operations:

1. Fetch gridpoints from `/points/{latitude},{longitude}`.
2. Fetch the forecast URL returned by the gridpoints request.

The existing independent providers must continue to run in parallel, subject to `FETCH_MAX_CONCURRENCY`.

## Current implementation constraints

- `FetchOperation` currently exposes only `execute()`, `name()`, and `shouldAbortOnFailure()`.
- `fetch_executor.cpp` assigns work by atomically claiming the next vector index, which assumes all operations are independent.
- Both synchronous (`executeParallel`) and asynchronous (`executeParallelAsync`) execution paths have their own worker logic.
- Results are indexed in the same order as the submitted operations.
- `shouldAbortOnFailure()` is evaluated by `main.cpp` after execution and represents display/network failure policy. It must remain separate from dependency scheduling.
- Provider instances are retained by `FetchBundle`, allowing operations from one provider to share provider-owned state safely until execution completes.

## Design

### 1. Add dependency metadata to `FetchOperation`

Add a non-owning list of prerequisite operation pointers to the base class:

```cpp
void dependsOn(const FetchOperation &operation);
const std::vector<const FetchOperation *> &dependencies() const;
```

`dependsOn()` is called while a provider builds its operation vector, before the vector is submitted to the executor. The dependency pointers are non-owning; the existing `std::unique_ptr<FetchOperation>` objects remain the owners.

Example:

```cpp
auto gridpoints = std::make_unique<CallbackFetchOperation>(...);
auto forecast = std::make_unique<CallbackFetchOperation>(...);

forecast->dependsOn(*gridpoints);

operations.push_back(std::move(gridpoints));
operations.push_back(std::move(forecast));
```

This keeps the existing provider interface and leaves independent operations unchanged.

### 2. Define dependency behavior

A dependency is a successful prerequisite, not merely an ordering hint:

- An operation becomes eligible only after all dependencies have completed.
- If all dependencies succeed, the operation is queued for execution.
- If any dependency fails, the operation is not executed.
- The skipped operation receives a failed `ProviderResult` describing the failed prerequisite.
- Failure propagates through subsequent dependency levels.
- `shouldAbortOnFailure()` continues to control what the application does with a result after execution; it does not determine whether dependents are scheduled.

This prevents a NOAA forecast request from using an absent or stale gridpoints URL.

### 3. Build and validate a task graph

Before starting workers, the executor will:

- Map every submitted operation pointer to its input index.
- Verify that every declared dependency is part of the same execution.
- Build reverse edges from each operation to its dependents.
- Count unresolved dependencies for every operation.
- Detect cycles and reject invalid graphs before launching worker tasks.

Invalid dependency references and cycles must produce failed results and return without deadlocking.

The graph is intentionally built per execution. It does not require global task IDs or persistent scheduler state.

### 4. Implement a ready-operation scheduler

Replace the current unconditional index claiming with a scheduler shared by both execution APIs.

Scheduler state will track:

- A queue of ready operation indexes.
- The number of unresolved dependencies per operation.
- Whether an operation has a failed prerequisite.
- Completion state and results.
- Reverse dependency edges.
- Number of active, completed, and created workers.

Worker behavior:

1. Wait for a ready operation.
2. Execute it outside the scheduler lock.
3. Publish its result.
4. Resolve its dependents.
5. Queue dependents whose prerequisites are all successful.
6. Mark dependent operations failed/skipped when a prerequisite failed.
7. Exit once the whole graph has reached a terminal state.

FreeRTOS semaphores or equivalent synchronization will be used for the ready queue. Workers must block while waiting for prerequisites rather than busy-looping.

The maximum number of worker task stacks remains `FETCH_MAX_CONCURRENCY`.

### 5. Unify synchronous and asynchronous execution

Refactor the duplicated scheduling logic so `executeParallel()` and `executeParallelAsync()` use the same dependency-aware scheduler.

Preserve the current public behavior:

- Empty executions complete immediately.
- Results retain input-vector ordering.
- `FetchExecution::wait()`, `close()`, `isComplete()`, and `results()` keep their existing semantics.
- The asynchronous handle continues owning operations until completion/close.
- Worker/task and semaphore allocation failures never cause a hang.
- Provider-level HTTP timeouts continue to bound normal operation duration.

Sequential fallback after partial worker creation must drain ready operations according to dependency state. It must not execute an arbitrary remaining suffix of the input vector.

### 6. Add executor tests

Extend `test/src/test_owm/fetch_executor.inc` with dependency-focused tests:

- A dependent operation never starts before its prerequisite.
- A successful prerequisite allows the dependent operation to run.
- A failed prerequisite prevents the dependent operation from running.
- Failure propagates through a dependency chain.
- Independent operations continue to run concurrently.
- Multiple dependencies are all required before execution.
- Missing dependency references fail without hanging.
- Cyclic dependencies fail without hanging.
- Synchronous and asynchronous execution have identical dependency behavior.
- Existing pool-limit, result-order, result-propagation, lifetime, and fallback tests remain passing.

Use mock operations with shared event/order state rather than timing-only assertions wherever possible.

### 7. NOAA provider integration preparation

When the NOAA provider is implemented:

- Store the parsed gridpoint response and forecast URL in the provider instance.
- Create a critical gridpoints operation.
- Create a critical forecast operation that reads the provider-owned forecast URL.
- Declare the forecast operation dependent on the gridpoints operation.
- Clear provider/report data on failed requests as appropriate for the existing provider conventions.
- Keep the provider alive through `FetchBundle.providers` until all operations complete.

NOAA provider/configuration work is separate from the executor change unless needed to prove the integration.

## Implementation checklist

### Dependency API

- [x] Add dependency storage and declaration methods to `FetchOperation`.
- [x] Document that dependencies are non-owning and must belong to the submitted execution.
- [x] Keep independent operations and existing provider interfaces source-compatible.

### Dependency graph

- [x] Build an operation-pointer-to-index map before starting workers.
- [x] Validate dependency references.
- [x] Build reverse dependent edges.
- [x] Detect cycles and return failed results without launching a deadlocked worker set.
- [x] Define consistent failed-result details for skipped dependent operations.

### Executor scheduler

- [x] Replace unconditional atomic index claiming with a ready-operation queue.
- [x] Track unresolved dependency counts and failed prerequisites.
- [x] Queue operations only after all dependencies succeed.
- [x] Propagate prerequisite failures through dependency chains.
- [x] Preserve the `FETCH_MAX_CONCURRENCY` worker limit.
- [x] Use FreeRTOS synchronization so workers block instead of busy-looping.
- [x] Share dependency-aware scheduling between synchronous and asynchronous execution.
- [x] Preserve result ordering and `FetchExecution` lifecycle semantics.
- [x] Make task-creation and semaphore-allocation fallbacks dependency-aware.

### Tests

- [x] Test that dependents never start before prerequisites.
- [x] Test successful prerequisite execution.
- [x] Test failed prerequisite skipping.
- [x] Test failure propagation through multiple dependency levels.
- [x] Test multiple prerequisites.
- [x] Test that independent operations still execute concurrently.
- [x] Test invalid dependency references.
- [x] Test cyclic dependencies.
- [x] Test both synchronous and asynchronous execution paths.
- [x] Keep all existing executor, provider, and lifetime tests passing.

### NOAA integration preparation

- [ ] Document the gridpoints operation and forecast operation relationship.
- [ ] Ensure provider-owned gridpoint state remains alive through forecast execution.
- [ ] Verify the forecast operation is not attempted when gridpoints fail.

### Verification

- [x] Run `./scripts/format.sh --check`.
- [x] Run `bash test/run_tests_docker.sh`.
- [x] Run `~/.platformio/penv/bin/pio run -e lolin_d32`.

## Verification commands

```sh
./scripts/format.sh --check
bash test/run_tests_docker.sh
~/.platformio/penv/bin/pio run -e lolin_d32
```

## Completion criteria

- Existing providers require no dependency-related changes.
- A provider can express a prerequisite without changing the executor call sites.
- No dependent operation starts before all prerequisites complete successfully.
- Invalid graphs terminate with failed results instead of deadlocking.
- The two-worker concurrency bound remains enforced.
- All configured QEMU tests and the normal PlatformIO build finish successfully.
