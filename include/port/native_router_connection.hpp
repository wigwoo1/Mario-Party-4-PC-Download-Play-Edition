#pragma once
#include "port/native_router_candidates.hpp"
#include "port/native_router_worker.hpp"

namespace partyboard::net {
inline bool routerPublicIpv4(const std::string& address){
    if(!routerIpv4(address))return false;
    unsigned octets[4]{};size_t begin=0;
    for(unsigned index=0;index<4;++index){const auto end=address.find('.',begin);const auto token=std::string_view(address).substr(begin,end-begin);std::from_chars(token.data(),token.data()+token.size(),octets[index]);begin=end+1;}
    const auto a=octets[0],b=octets[1],c=octets[2];
    return a!=0&&a!=10&&a!=127&&a<224&&!(a==100&&b>=64&&b<=127)&&!(a==169&&b==254)&&
        !(a==172&&b>=16&&b<=31)&&!(a==192&&(b==168||b==0||(b==88&&c==99)))&&
        !(a==198&&(b==18||b==19||(b==51&&c==100)))&&!(a==203&&b==0&&c==113);
}
// One RTC owner, one published SDP. Mapping observations never touch the local
// ICE agent, game RNG or recovery signatures; only the copied description gains
// a candidate after owned finite readback. Hold the ticket until route retirement.
class NativeRouterConnectionMapping {
    std::shared_ptr<NativeRouterWorker> worker_;
    std::optional<NativeRouterTicket> ticket_;
    std::optional<std::string> published_;
    uint64_t deadline_=0;
    size_t mappedCount_=0;
    bool closed_=false,started_=false;
public:
    explicit NativeRouterConnectionMapping(std::shared_ptr<NativeRouterWorker> worker={}):worker_(std::move(worker)){}
    NativeRouterConnectionMapping(const NativeRouterConnectionMapping&)=delete;
    std::optional<std::string> description(const rtc::Description& original,uint64_t now){
        if(closed_)return {};
        if(published_)return published_;
        if(!started_){
            started_=true;deadline_=now+12000;
            if(worker_)try{ticket_.emplace(worker_->retain(nativeRouterGatheredPorts(original)));}catch(...){ticket_.reset();}
        }
        rtc::Description copy=original;
        if(ticket_){
            const auto snapshot=ticket_->snapshot();
            if(!snapshot.complete&&!snapshot.closed&&now<deadline_)return {};
            if(snapshot.complete&&!snapshot.closed){
                const auto bound=nativeRouterGatheredPorts(original);
                for(const auto& mapped:snapshot.mapped){
                    if(!routerPublicIpv4(mapped.externalAddress)||mapped.expiresAt<=NativeRouterTicket::NativeRouterWorkerClock())continue;
                    const auto& request=mapped.request;
                    if(std::none_of(bound.begin(),bound.end(),[&](const auto& actual){return actual.internalAddress==request.internalAddress&&actual.internalPort==request.internalPort&&actual.externalPort==request.externalPort;}))continue;
                    rtc::Candidate candidate("candidate:pbupnp"+std::to_string(request.internalPort)+" 1 UDP 1694498815 "+mapped.externalAddress+" "+std::to_string(request.externalPort)+" typ srflx raddr "+request.internalAddress+" rport "+std::to_string(request.internalPort),original.bundleMid());
                    if(!copy.hasCandidate(candidate))copy.addCandidate(std::move(candidate));
                }
                // A rejected/nonpublic mapping must not retain an unused lease.
                if(copy.candidates().size()==original.candidates().size())ticket_->close();
            }else ticket_->close(); // Never leave a late, unpublished mapping.
        }
        mappedCount_=copy.candidates().size()-original.candidates().size();
        published_=std::string(copy);return published_;
    }
    const char* status()const{return closed_?"Closed":published_?(mappedCount_?"Mapped":"ICE fallback"):started_?"Preparing":"Not requested";}
    size_t mappedCount()const{return closed_?0:mappedCount_;}
    void close(){closed_=true;if(ticket_)ticket_->close();worker_.reset();}
    ~NativeRouterConnectionMapping(){close();}
};
}
