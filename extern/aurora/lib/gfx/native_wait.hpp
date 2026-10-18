#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace aurora::gfx {

inline void service_native_render_wait() {
#ifdef _WIN32
  // DXGI may synchronously send to the window-owning thread while rendering.
  // Service only nonqueued messages: SDL must retain posted input/window events
  // until the next regular event poll, outside frame construction and locks.
  MSG message{};
  PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
#endif
}

template <typename Predicate>
void wait_for_render_condition(std::condition_variable& cv, std::unique_lock<std::mutex>& lock,
                               Predicate ready) {
#ifdef _WIN32
  while (!ready()) {
    // A window procedure must never run while holding the render queue lock.
    lock.unlock();
    service_native_render_wait();
    lock.lock();
    cv.wait_for(lock, std::chrono::milliseconds{1}, ready);
  }
#else
  cv.wait(lock, ready);
#endif
}

} // namespace aurora::gfx
