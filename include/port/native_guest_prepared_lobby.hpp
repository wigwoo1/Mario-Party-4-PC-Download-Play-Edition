#pragma once
#include "port/native_guest_engine_preparation.hpp"
#include "port/native_guest_lobby_snapshot.hpp"
#include "port/native_guest_start_control.hpp"

namespace partyboard::net {
// Game-thread owner after verified disc/engine/GPU preparation. All control
// state comes from the original invitation; no method authorizes game resume.
class NativeGuestPreparedLobby {
    NativeGuestJoinSession& session_;
    nlohmann::ordered_json receipt_;
    NativeGuestLobbySnapshot lobby_;
    NativeGuestStartControl start_;
    std::deque<NativeGuestInvitation::Message> pendingStart_;
    bool snapshotAvailable_=false;
    uint64_t sequence_=0;
public:
    NativeGuestPreparedLobby(NativeGuestJoinSession& session,nlohmann::ordered_json receipt,uint64_t initialSequence=0)
        :session_(session),receipt_(receiveNativePreparedRuntimeReport(receipt.dump(),session.configuration())),
         lobby_(session.configuration().at("assetId"),session.receivedBytes(),session.transport().assignedPeer()),start_(session),sequence_(initialSequence){}
    void receiveSnapshot(const NativeGuestInvitation::Message& message){
        if(message.label!="partyboard-lobby-v1")throw std::invalid_argument("Original Lobby snapshot required");
        const auto* text=std::get_if<std::string>(&message.payload);
        if(!text)throw std::invalid_argument("Text host lobby snapshot required");
        lobby_.receive(*text);snapshotAvailable_=true;
    }
    void pump(){
        session_.pump();
        if(!session_.error().empty())throw std::runtime_error(session_.error());
        // The host can begin sending PAD immediately after committing Start.
        // Independent RTC channels may deliver that PAD before the commit;
        // preserve it in the bounded session queue for the installed input owner.
        while(auto message=session_.takeControl("partyboard-lobby-v1")){
            receiveSnapshot(*message);
        }
        while(auto message=session_.takeControl("partyboard-start-v1")){
            if(pendingStart_.size()>=4)throw std::runtime_error("Join pending Start bounds exceeded");
            pendingStart_.push_back(std::move(*message));
        }
        while(snapshotAvailable_&&!pendingStart_.empty()){
            const auto* text=std::get_if<std::string>(&pendingStart_.front().payload);
            if(!text||text->size()>65536)throw std::invalid_argument("Bounded Join Start text required");
            const auto packet=nlohmann::ordered_json::parse(*text);
            const auto& revision=packet.at("revision");
            if(!revision.is_number_integer()||revision.get<int64_t>()<0||revision.get<uint64_t>()>9007199254740991ULL)
                throw std::invalid_argument("Safe Join Start revision required");
            // RTC channels are ordered individually, not relative to each other.
            if(revision.get<uint64_t>()>lobby_.current().revision)break;
            const auto& configuration=session_.configuration();
            start_.receive(pendingStart_.front(),lobby_.current(),
                captureNativePreparedStart(configuration.at("assetId"),configuration.at("buildId")));
            pendingStart_.pop_front();
        }
    }
    void setReady(bool ready){
        if(start_.committed()||!pendingStart_.empty())throw std::runtime_error("Join Start agreement already pending");
        publishNativeGuestReady(session_,receipt_,lobby_.current(),sequence_,ready);
    }
    bool hasSnapshot()const{return snapshotAvailable_;}
    const PreparedStartLobby& snapshot()const{return lobby_.current();}
    bool committed()const{return start_.committed();}
    bool ownsSession(const NativeGuestJoinSession& session)const{return &session_==&session;}
    const nlohmann::ordered_json& commit()const{return start_.commit();}
};
}
