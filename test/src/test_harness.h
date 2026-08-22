/* Shared lifecycle dispatch for the configuration-rooted Unity tests.
 *
 * GPL-3.0, see LICENSE.
 */

#pragma once

namespace test_harness {

using Callback = void (*)();

inline Callback activeSetUp = nullptr;
inline Callback activeTearDown = nullptr;

inline void selectCallbacks(Callback setUp, Callback tearDown) {
  activeSetUp = setUp;
  activeTearDown = tearDown;
}

inline void dispatchSetUp() {
  if (activeSetUp != nullptr) {
    activeSetUp();
  }
}

inline void dispatchTearDown() {
  if (activeTearDown != nullptr) {
    activeTearDown();
  }
}

}  // namespace test_harness
