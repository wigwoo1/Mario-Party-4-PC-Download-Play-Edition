#pragma once
#include "port/native_invitation.hpp"
#include "port/native_connection_route.hpp"
#include "port/native_rtc_session_control.hpp"
#include "port/native_router_connection.hpp"
#include <optional>

namespace partyboard::net {
// Game-thread owner for one initial invitation. Retain until its input/asset
// owners stop; private reply assignment is separate from reconnect signatures.
class NativeHostInvitation {
    std::string peer_;
    std::shared_ptr<rtc::PeerConnection> connection_;
    mutable NativeRouterConnectionMapping router_;
    bool answered_=false,closed_=false;
public:
    NativeHostInvitation(std::string assignedPeer,NativeRtcSessionControl& controls,rtc::Configuration configuration={},std::shared_ptr<NativeRouterWorker> router={})
        :peer_(std::move(assignedPeer)),router_(std::move(router)){
        validateNativeInvitation({{"version",1},{"playerId",peer_},{"description",{{"type","offer"},{"sdp",""}}}},"offer");
        if(controls.hasPeer(peer_))throw std::invalid_argument("Native invitation peer already assigned");
        connection_=std::make_shared<rtc::PeerConnection>(std::move(configuration));
        try{
            for(auto purpose:{NativeRtcControlChannel::Purpose::Preparation,NativeRtcControlChannel::Purpose::Start,NativeRtcControlChannel::Purpose::Lobby})
                controls.attachAuthenticated(peer_,purpose,connection_->createDataChannel(NativeRtcControlChannel::label(purpose)));
        }catch(...){controls.retireAuthenticated(peer_);connection_->close();throw;}
    }
    NativeHostInvitation(const NativeHostInvitation&)=delete;
    NativeHostInvitation& operator=(const NativeHostInvitation&)=delete;
    std::shared_ptr<rtc::DataChannel> createChannel(const std::string& label){
        if(closed_||answered_)throw std::runtime_error("Invitation channel creation ended");
        return connection_->createDataChannel(label);
    }
    std::optional<std::string> invitation()const{
        if(closed_||answered_||connection_->gatheringState()!=rtc::PeerConnection::GatheringState::Complete)return std::nullopt;
        const auto description=connection_->localDescription();
        if(!description)return std::nullopt;
        const auto mapped=router_.description(*description,NativeRouterTicket::NativeRouterWorkerClock());
        if(!mapped)return {};
        return encodeNativeInvitation({{"version",1},{"playerId",peer_},{"description",{{"type","offer"},{"sdp",*mapped}}}});
    }
    void acceptReply(const std::string& code){
        if(closed_||answered_||!invitation())throw std::runtime_error("No pending native invitation");
        const auto packet=receiveNativeInvitation(code,"answer",peer_);
        connection_->setRemoteDescription(rtc::Description(packet.at("description").at("sdp").get<std::string>(),"answer"));
        answered_=true;
    }
    void retireRouterMapping(){router_.close();}
    const char* routerPreparation()const{return router_.status();}
    void close(){if(closed_)return;closed_=true;router_.close();connection_->resetCallbacks();connection_->close();}
    NativeConnectionRoute connectionRoute()const{return closed_?NativeConnectionRoute::Closed:nativeConnectionRoute(*connection_);}
    ~NativeHostInvitation(){close();}
};
}
