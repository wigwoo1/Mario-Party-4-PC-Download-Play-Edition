#include <port/native_room_client.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>
using namespace partyboard::net;
using Json=nlohmann::ordered_json;
void emit(Json value){std::cout<<value.dump()<<std::endl;}
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("Probe mode required.");const std::string mode=argv[1];
    if(mode=="policy"){
        for(const auto* url:{"https://lobby.example/","http://127.0.0.1:8954/","http://[::1]:8954/"})NativeRoomAddress::parse(url);
        unsigned rejected=0;for(const auto* url:{"http://lobby.example/","http://127.0.0.1.evil.example/","https://host.example:0/","https://host.example:65536/","https://user:secret@host.example/","https://host.example/../","https://host.example/?key=private","javascript:alert(1)","https://host.example/#room=short&key=short"})try{NativeRoomAddress::parse(url);}catch(const std::exception&){++rejected;}
        if(rejected!=9)throw std::runtime_error("Invalid lobby address accepted.");emit({{"status","passed"},{"rejected",rejected}});return 0;
    }
    if(argc<3)throw std::runtime_error("Probe URL required.");
    if(mode=="http"){
        const unsigned cancelMs=argc>3?std::stoul(argv[3]):0,timeoutMs=argc>4?std::stoul(argv[4]):1500;
        std::stop_source cancel;std::jthread canceller;if(cancelMs)canceller=std::jthread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(cancelMs));cancel.request_stop();});
        const auto start=std::chrono::steady_clock::now();Json result;
        try{const auto response=nativeRoomHttp({argv[2],"GET",std::string(43,'a'),"",timeoutMs,1024},cancel.get_token());result={{"ok",true},{"status",response.status},{"bytes",response.body.size()}};}
        catch(const std::exception& error){result={{"ok",false},{"error",error.what()}};}
        result["elapsedMs"]=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();emit(result);return 0;
    }
    const bool hosting=mode=="host";if(!hosting&&mode!="join")throw std::runtime_error("Unknown probe mode.");
    NativeRoomClient client(NativeRoomAddress::parse(argv[2],!hosting),hosting);
    bool announced=false,sealed=false;std::string pending;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(std::chrono::steady_clock::now()<deadline){
        if(!client.error().empty()){emit({{"type","error"},{"message",client.error()}});return 2;}
        if(client.ready()&&!announced){Json message{{"type","ready"}};if(hosting)message["link"]=client.joinLink();emit(message);announced=true;}
        if(client.sealed()&&!sealed){emit({{"type","sealed"}});sealed=true;}
        for(unsigned count=0;count<64;++count){auto event=client.take();if(!event)break;emit(*event);}
        DWORD available=0;const HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
        if(PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr)&&available){
            char buffer[4096];DWORD read=0;if(!ReadFile(input,buffer,std::min<DWORD>(sizeof(buffer),available),&read,nullptr))throw std::runtime_error("Probe input failed.");pending.append(buffer,read);if(pending.size()>128000)throw std::runtime_error("Probe input too large.");
            size_t end;while((end=pending.find('\n'))!=std::string::npos){const auto command=Json::parse(pending.substr(0,end));pending.erase(0,end+1);const auto type=command.at("type").get<std::string>();
                if(type=="stop"){client.close();emit({{"type","closed"}});return 0;}
                if(type=="send")client.send(command.at("to"),command.at("kind"),command.at("payload"));
                else if(type=="seal")client.seal();else if(type=="remove")client.remove(command.at("peer"));else throw std::runtime_error("Unknown probe command.");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Native room probe deadline exceeded.");
}catch(const std::exception& error){emit({{"type","error"},{"message",error.what()}});return 1;}}
