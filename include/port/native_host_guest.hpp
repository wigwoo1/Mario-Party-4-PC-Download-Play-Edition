#pragma once
#include "port/native_host_invitation.hpp"
#include "port/native_rtc_asset_host.hpp"
#include "port/native_pipeline_seed.hpp"
#include <algorithm>

namespace partyboard::net {
// Own the guest's initial connection and asset worker together. Session control
// must outlive this owner; retire its routes only after the worker has stopped.
// Initial invitation assignment does not establish signed recovery credentials.
class NativeHostGuest {
    NativeRtcSessionControl& controls_;
    std::string peer_;
    NativeHostInvitation invitation_;
    std::shared_ptr<rtc::DataChannel> input_;
    std::shared_ptr<rtc::DataChannel> recovery_;
    std::unique_ptr<NativeRtcAssetHost> assets_;
    std::shared_ptr<const NativePipelineSeed> pipelineSeed_;
    std::shared_ptr<rtc::DataChannel> pipelines_;
    size_t pipelineOffset_=0;
    bool pipelineHeaderSent_=false;
    bool closed_=false;
public:
    NativeHostGuest(std::string peer,NativeRtcSessionControl& controls,
                    std::shared_ptr<NativeSharedAsset> asset,rtc::Configuration configuration={},
                    std::shared_ptr<const NativePipelineSeed> pipelineSeed={},std::shared_ptr<NativeRouterWorker> router={})
        :controls_(controls),peer_(std::move(peer)),invitation_(peer_,controls,std::move(configuration),std::move(router)),pipelineSeed_(std::move(pipelineSeed)) {
        try {
            input_=invitation_.createChannel("partyboard-input-v1");
            recovery_=invitation_.createChannel("partyboard-recovery-v1");
            if(pipelineSeed_)pipelines_=invitation_.createChannel("partyboard-pipelines-v1");
            assets_=std::make_unique<NativeRtcAssetHost>(std::move(asset),invitation_.createChannel("partyboard-assets-v1"));
        } catch (...) {
            controls_.retireAuthenticated(peer_);
            invitation_.close();
            throw;
        }
    }
    NativeHostGuest(const NativeHostGuest&)=delete;
    NativeHostGuest& operator=(const NativeHostGuest&)=delete;
    std::optional<std::string> invitation()const{return invitation_.invitation();}
    void acceptReply(const std::string& code){if(closed_)throw std::runtime_error("Native guest closed");invitation_.acceptReply(code);}
    bool assetsComplete()const{return !closed_&&assets_->complete();}
    uint64_t assetSentBytes()const{return assets_->sentBytes();}
    NativeConnectionRoute connectionRoute()const{return invitation_.connectionRoute();}
    std::string assetError()const{return assets_?assets_->error():std::string{};}
    // Same bounded header/chunks as the browser sender. Pump only from the
    // lobby thread; readiness still requires a matching verified seed receipt.
    void pumpPipelineSeed(){
        if(!pipelineSeed_||!pipelines_||!pipelines_->isOpen())return;
        if(!pipelineHeaderSent_){
            pipelines_->send(nlohmann::ordered_json{{"type","pipeline-seed"},{"version",1},
                {"size",pipelineSeed_->bytes.size()},{"sha256",pipelineSeed_->sha256}}.dump());
            pipelineHeaderSent_=true;
        }
        for(unsigned count=0;count<16&&pipelineOffset_<pipelineSeed_->bytes.size()&&pipelines_->bufferedAmount()<262144;++count){
            const auto length=std::min(size_t(16384),pipelineSeed_->bytes.size()-pipelineOffset_);
            pipelines_->send(reinterpret_cast<const rtc::byte*>(pipelineSeed_->bytes.data()+pipelineOffset_),length);
            pipelineOffset_+=length;
        }
        if(pipelineOffset_==pipelineSeed_->bytes.size())pipelineSeed_.reset();
    }
    // Coordinator attaches this dedicated channel only after validating the
    // retained start identity. Merely negotiating it never resumes simulation.
    std::shared_ptr<rtc::DataChannel> inputChannel()const{
        if(closed_)throw std::runtime_error("Native guest closed");
        return input_;
    }
    std::shared_ptr<rtc::DataChannel> recoveryChannel()const{
        if(closed_)throw std::runtime_error("Native guest closed");return recovery_;
    }
    // Any installed engine input binding must be stopped/detached first.
    void retireRouterMapping(){invitation_.retireRouterMapping();}
    const char* routerPreparation()const{return invitation_.routerPreparation();}
    void close(){
        if(closed_)return;
        closed_=true;
        assets_->close();
        if(pipelines_)pipelines_->close();pipelineSeed_.reset();
        input_->close();
        recovery_->close();
        controls_.retireAuthenticated(peer_);
        invitation_.close();
    }
    ~NativeHostGuest(){close();}
};
}
