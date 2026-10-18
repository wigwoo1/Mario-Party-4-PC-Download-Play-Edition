#pragma once
#include "port/crypto.hpp"
#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>

namespace partyboard::net {
inline bool routerIpv4(std::string_view address){
    for(unsigned part=0;part<4;++part){
        const auto dot=address.find('.');
        const auto token=address.substr(0,dot);
        unsigned value=0;
        if(token.empty()||token.size()>3||(token.size()>1&&token.front()=='0'))return false;
        const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size()||value>255)return false;
        if(part==3)return dot==std::string_view::npos;
        if(dot==std::string_view::npos)return false;
        address.remove_prefix(dot+1);
    }
    return false;
}
inline std::string newRouterMappingOwner(){
    std::array<uint8_t,16> bytes;crypto::secureRandom(bytes);
    std::string owner="Partyboard-";
    for(const auto byte:bytes){owner+="0123456789abcdef"[byte>>4];owner+="0123456789abcdef"[byte&15];}
    return owner;
}
struct RouterMappingRequest {
    std::string internalAddress;
    uint16_t internalPort=0,externalPort=0;
};
struct RouterMappingEntry {
    std::string internalAddress,description;
    uint16_t internalPort=0;
    bool enabled=false;
    // nullopt means missing/malformed, zero means permanent: neither is accepted.
    std::optional<uint32_t> leaseSeconds;
};
struct RouterMappingQuery {
    enum class State { Absent, Found, Unavailable } state=State::Unavailable;
    RouterMappingEntry entry;
    int error=0;
};
// UDP only. Backend operations must run on the router worker, never the game/UI
// thread. An error/timeout is not evidence that a mapping is absent.
class RouterMappingBackend {
public:
    virtual void setCancellation(std::stop_token){}
    virtual RouterMappingQuery query(uint16_t externalPort)=0;
    virtual int add(const RouterMappingRequest&,const std::string& owner,uint32_t leaseSeconds)=0;
    virtual int remove(uint16_t externalPort)=0;
    virtual ~RouterMappingBackend()=default;
};
enum class RouterLeaseState { Idle, Active, Conflict, Unavailable, Lost, UnsupportedLease, CleanupPending, Closed };
class NativeRouterMappingLease {
    RouterMappingBackend& backend_;
    RouterMappingRequest request_;
    std::string owner_;
    RouterLeaseState state_=RouterLeaseState::Idle;
    bool mayOwn_=false,started_=false,closing_=false;
    uint64_t expires_=0,renewAt_=0;
    uint32_t requestedSeconds_;
    bool owns(const RouterMappingEntry& entry)const{
        return entry.internalAddress==request_.internalAddress&&entry.internalPort==request_.internalPort&&entry.description==owner_;
    }
    bool finite(const RouterMappingEntry& entry)const{
        return entry.enabled&&entry.leaseSeconds&&*entry.leaseSeconds>0&&*entry.leaseSeconds<=requestedSeconds_;
    }
    void confirm(const RouterMappingQuery& result,uint64_t now){
        if(result.state==RouterMappingQuery::State::Unavailable){state_=RouterLeaseState::Unavailable;return;}
        if(result.state==RouterMappingQuery::State::Absent){mayOwn_=false;state_=RouterLeaseState::Lost;return;}
        if(!owns(result.entry)){mayOwn_=false;state_=RouterLeaseState::Conflict;return;}
        mayOwn_=true;
        if(!finite(result.entry)){
            state_=RouterLeaseState::UnsupportedLease;
            // Do not knowingly leave a permanent mapping created by a router
            // which ignored our finite duration. Cleanup rechecks ownership.
            close();
            if(state_==RouterLeaseState::Closed)state_=RouterLeaseState::UnsupportedLease;
            return;
        }
        const auto duration=uint64_t(*result.entry.leaseSeconds)*1000;
        expires_=now+duration;renewAt_=now+duration/2;state_=RouterLeaseState::Active;
    }
public:
    NativeRouterMappingLease(RouterMappingBackend& backend,RouterMappingRequest request,
                            std::string owner=newRouterMappingOwner(),uint32_t leaseSeconds=600)
        :backend_(backend),request_(std::move(request)),owner_(std::move(owner)),requestedSeconds_(leaseSeconds){
        if(!routerIpv4(request_.internalAddress)||!request_.internalPort||!request_.externalPort||leaseSeconds<30||leaseSeconds>3600)
            throw std::invalid_argument("Bounded IPv4 UDP mapping and finite lease required");
        if(owner_.size()!=43||owner_.substr(0,11)!="Partyboard-"||owner_.find_first_not_of("0123456789abcdef",11)!=std::string::npos)
            throw std::invalid_argument("Fresh router mapping ownership token required");
    }
    NativeRouterMappingLease(const NativeRouterMappingLease&)=delete;
    NativeRouterMappingLease& operator=(const NativeRouterMappingLease&)=delete;
    RouterLeaseState state()const{return state_;}
    bool active(uint64_t now)const{return !closing_&&state_==RouterLeaseState::Active&&now<expires_;}
    uint64_t expiresAt()const{return expires_;}
    uint64_t renewAt()const{return renewAt_;}
    const std::string& owner()const{return owner_;}
    void acquire(uint64_t now){
        if(started_||closing_)throw std::logic_error("Mapping acquisition is single-use");
        started_=true;
        const auto before=backend_.query(request_.externalPort);
        if(before.state==RouterMappingQuery::State::Unavailable){state_=RouterLeaseState::Unavailable;return;}
        if(before.state==RouterMappingQuery::State::Found){state_=RouterLeaseState::Conflict;return;}
        // UPnP has no atomic compare-and-set. Query immediately before every
        // mutation; never adopt/replace a known preexisting mapping, even one
        // with this application's name or this same internal address and port.
        mayOwn_=true;
        (void)backend_.add(request_,owner_,requestedSeconds_);
        // Verify even after HTTP errors: the request may have reached the router.
        confirm(backend_.query(request_.externalPort),now);
    }
    void tick(uint64_t now){
        if(closing_||!mayOwn_)return;
        if(expires_&&now>=expires_){state_=RouterLeaseState::Lost;return;}
        if(now<renewAt_)return;
        const auto before=backend_.query(request_.externalPort);
        if(before.state==RouterMappingQuery::State::Unavailable){state_=RouterLeaseState::Unavailable;renewAt_=now+1000;return;}
        if(before.state==RouterMappingQuery::State::Absent){mayOwn_=false;state_=RouterLeaseState::Lost;return;}
        if(!owns(before.entry)){mayOwn_=false;state_=RouterLeaseState::Conflict;return;}
        if(!finite(before.entry)){state_=RouterLeaseState::UnsupportedLease;close();return;}
        (void)backend_.add(request_,owner_,requestedSeconds_);
        confirm(backend_.query(request_.externalPort),now);
        if(state_==RouterLeaseState::Unavailable)renewAt_=now+1000;
    }
    // False means cleanup could not be verified; a worker may retry. Never
    // blindly delete after a failed query, and never delete another owner.
    bool close(){
        closing_=true;
        if(!mayOwn_){state_=RouterLeaseState::Closed;return true;}
        const auto before=backend_.query(request_.externalPort);
        if(before.state==RouterMappingQuery::State::Unavailable){state_=RouterLeaseState::CleanupPending;return false;}
        if(before.state==RouterMappingQuery::State::Absent||!owns(before.entry)){mayOwn_=false;state_=RouterLeaseState::Closed;return true;}
        (void)backend_.remove(request_.externalPort);
        const auto after=backend_.query(request_.externalPort);
        if(after.state==RouterMappingQuery::State::Absent||(after.state==RouterMappingQuery::State::Found&&!owns(after.entry))){mayOwn_=false;state_=RouterLeaseState::Closed;return true;}
        state_=RouterLeaseState::CleanupPending;return false;
    }
    ~NativeRouterMappingLease(){try{close();}catch(...){/* finite lease remains the crash/failure fallback */}}
};
}
