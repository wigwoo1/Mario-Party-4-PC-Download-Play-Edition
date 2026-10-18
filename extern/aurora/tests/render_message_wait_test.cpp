#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../lib/gfx/render_worker.hpp"
#include "../lib/gfx/native_wait.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

namespace rw = aurora::gfx::render_worker;
constexpr UINT Sent = WM_APP + 40;
constexpr UINT Posted = WM_APP + 41;
static unsigned dispatchedPosted;
static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
  if (message == Sent) return 123;
  if (message == Posted) { ++dispatchedPosted; return 0; }
  return DefWindowProcW(hwnd, message, w, l);
}
int main(int argc, char** argv) {
  WNDCLASSW cls{};
  cls.lpfnWndProc = procedure; cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"AuroraOwnedWaitRegression";
  if (!RegisterClassW(&cls)) return 2;
  HWND window = CreateWindowW(cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                              nullptr, cls.hInstance, nullptr);
  if (!window) return 3;
  PostMessageW(window, Posted, 0, 0);
  std::atomic_bool delivered = false;
  auto send = [&] {
    DWORD_PTR result = 0;
    delivered = SendMessageTimeoutW(window, Sent, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                    1500, &result) != 0 && result == 123;
  };
  const char* mode = argc > 1 ? argv[1] : "sync";
  if (!strcmp(mode, "queue")) {
    rw::BoundedQueue queue{1};
    queue.push({});
    std::thread consumer([&] { send(); bool closed; queue.pop_for(std::chrono::milliseconds(0), closed); });
    queue.push({});
    consumer.join();
  } else if (!strcmp(mode, "slot")) {
    rw::FrameSlotPool slots{1};
    const auto slot = slots.acquire();
    std::thread releaser([&] { send(); slots.release(slot); });
    slots.acquire();
    releaser.join();
  } else if (!strcmp(mode, "staging")) {
    std::atomic_bool complete = false;
    std::thread producer([&] { send(); complete = true; });
    while (!complete) {
      aurora::gfx::service_native_render_wait();
      std::this_thread::sleep_for(std::chrono::microseconds{100});
    }
    producer.join();
  } else {
    rw::initialize();
    rw::enqueue_work(send);
    if (!strcmp(mode, "shutdown")) rw::shutdown();
    else { rw::synchronize(); rw::shutdown(); }
  }
  MSG msg{};
  bool preserved = dispatchedPosted == 0 && PeekMessageW(&msg, window, Posted, Posted, PM_REMOVE);
  DestroyWindow(window);
  printf("%s: sent message completed=%d; posted event preserved=%d\n", mode, int(delivered.load()), int(preserved));
  return delivered && preserved ? 0 : 1;
}
