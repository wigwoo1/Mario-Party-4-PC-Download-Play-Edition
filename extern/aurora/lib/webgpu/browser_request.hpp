#pragma once
// Browser-only startup wait. Requires Asyncify; yields to browser callbacks.
#include <emscripten.h>
#include <memory>
#include <utility>
#include <webgpu/webgpu_cpp.h>
namespace aurora::webgpu {
template<class T, class Start>
T browser_request(Start start) {
  struct State { bool done=false; T value{}; };
  auto state=std::make_shared<State>();
  start([state](auto, T value, wgpu::StringView) {
    state->value=std::move(value);
    state->done=true;
  });
  const double deadline=emscripten_get_now()+15000;
  while(!state->done && emscripten_get_now()<deadline) emscripten_sleep(10);
  // Late callbacks retain only this request's state, never renderer globals.
  return state->done ? std::move(state->value) : T{};
}
}
