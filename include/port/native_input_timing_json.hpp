#pragma once
#include "native_input_timing.hpp"
#include <nlohmann/json.hpp>
namespace partyboard::net {
inline nlohmann::ordered_json nativeInputTimingJson(const NativeInputTiming& value){
    const auto span=[](const NativeInputTiming::Span& item){return nlohmann::ordered_json{
        {"calls",item.calls},{"totalMs",item.totalMs},{"maxMs",item.maxMs},{"buckets",item.buckets}};};
    return {{"enabled",value.enabled},{"startedPolls",value.startedPolls},{"completedPolls",value.completedPolls},
        {"pump",span(value.pump)},{"send",span(value.send)},{"wait",span(value.wait)},{"poll",span(value.poll)},
        {"bucketUpperBoundsMs",{2,8,20,nullptr}}};
}
}
