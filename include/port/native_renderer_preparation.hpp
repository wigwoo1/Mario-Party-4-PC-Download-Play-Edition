#pragma once
#include "port/native_prepared_start.hpp"
#include <aurora/aurora.h>
#include <SDL3/SDL_timer.h>
#include <chrono>
#include <cstdlib>
#include <charconv>
#include <string_view>

namespace partyboard::net {
// Game-thread GPU preparation. Pump caller-owned UI/transport events between
// frames; keep the actual seeded engine stopped throughout queue draining.
template<class Pump> void prepareNativeRenderer(const PreparedStartRuntime& prepared,Pump&& pump){
    uint32_t expectedSeedRows=0;
    if(std::getenv("PARTYBOARD_NATIVE_PIPELINE_SEED")){
        const char* count=std::getenv("PARTYBOARD_NATIVE_PIPELINE_SEED_COUNT");
        if(!count)throw std::runtime_error("Native seeded preparation requires expected row count");
        const std::string_view text(count);
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),expectedSeedRows);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!expectedSeedRows)
            throw std::runtime_error("Valid native pipeline seed row count required");
        const auto* stats=aurora_get_stats();
        if(!stats||stats->loadedPipelineConfigurations!=expectedSeedRows)
            throw std::runtime_error("Native renderer did not accept all seeded pipeline configurations");
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(std::chrono::steady_clock::now()<deadline){
        if(!pump())throw std::runtime_error("Native renderer preparation cancelled");
        if(aurora_begin_frame()){
            aurora_end_frame();SDL_Delay(20);
            const auto captured=captureNativePreparedStart(prepared.assetId,prepared.buildId);
            if(captured.checkpoint!=prepared.checkpoint||captured.seed!=prepared.seed)
                throw std::runtime_error("Native renderer changed prepared engine state");
            const auto* stats=aurora_get_stats();
            if(stats&&stats->queuedPipelines==0)return;
        }else SDL_Delay(20);
    }
    throw std::runtime_error("Native discovered pipeline preparation did not finish");
}
}

