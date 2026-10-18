#include "port/native_router_connection.hpp"
#include "port/native_host_invitation.hpp"
#include "port/native_host_replacement.hpp"
#include "port/recovery_identity.hpp"
#include <source_location>
#include <iostream>
using namespace partyboard::net;
void check(bool ok,std::source_location location=std::source_location::current()){
    if(!ok)throw std::runtime_error("router connection assertion at line "+std::to_string(location.line()));
}
template<class Predicate>void wait(Predicate predicate){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!predicate()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Router connection test timed out");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
}
struct State {std::mutex mutex;std::map<uint16_t,RouterMappingEntry> mappings;unsigned adds=0,removes=0;};
class Backend:public RouterMappingBackend {
    std::shared_ptr<State> state_;
public:
    explicit Backend(std::shared_ptr<State> state):state_(std::move(state)){}
    RouterMappingQuery query(uint16_t port)override{
        std::lock_guard lock(state_->mutex);const auto found=state_->mappings.find(port);
        return found==state_->mappings.end()?RouterMappingQuery{RouterMappingQuery::State::Absent,{},714}:RouterMappingQuery{RouterMappingQuery::State::Found,found->second,0};
    }
    int add(const RouterMappingRequest& request,const std::string& owner,uint32_t lease)override{
        std::lock_guard lock(state_->mutex);++state_->adds;state_->mappings[request.externalPort]={request.internalAddress,owner,request.internalPort,true,lease};return 0;
    }
    int remove(uint16_t port)override{std::lock_guard lock(state_->mutex);++state_->removes;state_->mappings.erase(port);return 0;}
};
int main(){try{
    for(const char* address:{"0.0.0.0","10.0.0.1","100.64.0.1","127.0.0.1","169.254.0.1","172.16.0.1","192.168.0.1","192.0.2.1","198.18.0.1","198.51.100.1","203.0.113.1","224.0.0.1","255.255.255.255","router.local"})check(!routerPublicIpv4(address));
    check(routerPublicIpv4("9.9.9.9")&&routerPublicIpv4("8.8.8.8"));
    rtc::PeerConnection connection;auto channel=connection.createDataChannel("partyboard-router-binding-test");
    wait([&]{return connection.gatheringState()==rtc::PeerConnection::GatheringState::Complete&&connection.localDescription();});
    const auto original=*connection.localDescription();const auto originalText=std::string(original);
    const auto bound=nativeRouterGatheredPorts(original);check(!bound.empty());
    auto state=std::make_shared<State>();
    auto worker=std::make_shared<NativeRouterWorker>([&](std::stop_token)->std::optional<NativeRouterWorker::Backend>{
        return NativeRouterWorker::Backend{{"http://127.0.0.1:1/control","urn:schemas-upnp-org:service:WANIPConnection:1",bound.front().internalAddress,"9.9.9.9"},std::make_unique<Backend>(state)};
    });
    NativeRouterConnectionMapping mapped(worker);std::optional<std::string> text;
    wait([&]{text=mapped.description(original,NativeRouterTicket::NativeRouterWorkerClock());return text.has_value();});
    rtc::Description augmented(*text,"offer");
    check(augmented.candidates().size()>original.candidates().size());check(augmented.icePwd()==original.icePwd()&&augmented.iceUfrag()==original.iceUfrag());
    check(augmented.fingerprint()->value==original.fingerprint()->value);check(std::string(*connection.localDescription())==originalText);
    for(auto candidate:augmented.candidates())if(candidate.type()==rtc::Candidate::Type::ServerReflexive){
        check(candidate.resolve(rtc::Candidate::ResolveMode::Simple));check(candidate.address()=="9.9.9.9");
        check(std::any_of(bound.begin(),bound.end(),[&](const auto& request){return request.externalPort==candidate.port();}));
    }
    check(mapped.description(original,NativeRouterTicket::NativeRouterWorkerClock())==text);
    {
        NativeRtcSessionControl controls;NativeHostInvitation invitation("guest1",controls,{},worker);
        std::optional<std::string> code;wait([&]{code=invitation.invitation();return code.has_value();});
        const auto packet=receiveNativeInvitation(*code,"offer","guest1");
        rtc::Description offered(packet.at("description").at("sdp").get<std::string>(),"offer");
        const auto offeredCandidates=offered.candidates();
        check(std::any_of(offeredCandidates.begin(),offeredCandidates.end(),[](const auto& candidate){return candidate.type()==rtc::Candidate::Type::ServerReflexive;}));
        check(invitation.invitation()==code);invitation.close();
    }
    {
        const nlohmann::ordered_json credential={{"type","recovery-credential"},{"version",1},{"session",std::string(32,'1')},{"assetId",std::string(64,'a')},{"peer","guest2"},{"secret",std::string(64,'b')}};
        RecoveryIdentity host(credential),verify(credential);const std::string nonce(32,'c');
        NativeHostReplacement replacement(60000,[&](const auto& description){return host.sign(description,nonce);},[](const auto&){return false;},{},worker);
        std::optional<nlohmann::ordered_json> offer;wait([&]{offer=replacement.offer(0);return offer.has_value();});
        check(verify.verify(*offer,"offer",nonce));
        rtc::Description signedDescription(offer->at("description").at("sdp").get<std::string>(),"offer");
        const auto candidates=signedDescription.candidates();check(std::any_of(candidates.begin(),candidates.end(),[](const auto& candidate){return candidate.type()==rtc::Candidate::Type::ServerReflexive;}));
        replacement.close();
    }
    mapped.close();check(worker->close());
    {std::lock_guard lock(state->mutex);check(state->mappings.empty()&&state->adds==state->removes);}
    {
        NativeRouterConnectionMapping unavailable;check(unavailable.description(original,0)==originalText);
    }
    {
        std::mutex mutex;std::condition_variable changed;bool release=false;
        auto late=std::make_shared<NativeRouterWorker>([&](std::stop_token stop)->std::optional<NativeRouterWorker::Backend>{
            std::unique_lock lock(mutex);std::stop_callback wake(stop,[&]{changed.notify_all();});changed.wait(lock,[&]{return release||stop.stop_requested();});
            return NativeRouterWorker::Backend{{"http://127.0.0.1:1/control","urn:schemas-upnp-org:service:WANIPConnection:1",bound.front().internalAddress,"9.9.9.9"},std::make_unique<Backend>(state)};
        });
        NativeRouterConnectionMapping timeout(late);check(!timeout.description(original,0));
        check(timeout.description(original,12001)==originalText);
        {std::lock_guard lock(mutex);release=true;}changed.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));check(timeout.description(original,12002)==originalText);
        check(late->close());{std::lock_guard lock(state->mutex);check(state->mappings.empty()&&state->adds==state->removes);}
    }
    connection.close();
    std::cout<<"PASS actual gathered-port invitation and signed replacement SDP enrichment; original ICE credentials/description preserved; stable offers, retirement and timed-out late mapping cleanup. Fake public candidates were decoded only; no router or WAN connectivity claim.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
