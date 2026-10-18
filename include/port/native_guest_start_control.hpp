#pragma once
#include "port/native_guest_join_session.hpp"
#include "port/native_guest_start_agreement.hpp"

namespace partyboard::net {
// Called by the game thread after engine/GPU preparation and lobby readiness.
// Only the original invitation's assigned Start channel supplies these packets.
class NativeGuestStartControl {
    NativeGuestJoinSession& session_;
    NativeGuestStartAgreement agreement_;
    std::optional<nlohmann::ordered_json> commit_;
public:
    explicit NativeGuestStartControl(NativeGuestJoinSession& session)
        :session_(session),agreement_(session.transport().assignedPeer(),session.configuration()){
        (void)session.recoveryIdentity();
        if(!session.assetsComplete())throw std::runtime_error("Verified Join assets required for Start");
    }
    void receive(const NativeGuestInvitation::Message& message,
            const PreparedStartLobby& lobby,const PreparedStartRuntime& captured){
        (void)session_.configuration();
        if(!session_.assetsComplete()||message.label!="partyboard-start-v1")
            throw std::invalid_argument("Original assigned Join Start packet required");
        const auto* text=std::get_if<std::string>(&message.payload);
        if(!text||text->size()>65536)throw std::invalid_argument("Bounded text Join Start packet required");
        const auto channel=session_.transport().channel(message.label);
        if(!channel||!channel->isOpen())throw std::runtime_error("Join Start channel unavailable");
        const auto type=nlohmann::ordered_json::parse(*text).at("type");
        if(type=="session-start")channel->send(agreement_.acknowledge(*text,lobby,captured).dump());
        else if(type=="session-start-commit")commit_=agreement_.acceptCommit(*text,lobby,captured);
        else throw std::invalid_argument("Unexpected Join Start packet");
    }
    bool committed()const{return session_.assetsComplete()&&agreement_.committed();}
    const nlohmann::ordered_json& commit()const{if(!committed()||!commit_)throw std::runtime_error("Accepted Join commit unavailable");return *commit_;}
};
}
