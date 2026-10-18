#include "port/asset_transfer.hpp"
#include "port/native_asset_receiver.hpp"
#include <sstream>
#include <iostream>
int main(int argc,char** argv){
    if(argc==4&&std::string(argv[1])=="--manifest"){
        partyboard::net::NativeAsset asset(argv[2],"Mario Party 4.ciso");
        std::ofstream file(argv[3]);file<<asset.manifest().dump(2)<<'\n';
        if(!file)return 5;std::cout<<"PASS private native asset preparation: "<<asset.manifest()["id"]<<'\n';return 0;
    }
    if(argc!=2)return 2;
    const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
    const auto path=directory/"fixture.bin";
    {std::ofstream file(path,std::ios::binary);for(unsigned i=0;i<50000;++i)file.put(char((i*23)&255));}
    partyboard::net::NativeAsset asset(path,"fixture.bin");
    {
        std::ostringstream staging;
        partyboard::net::NativeAssetReceiver receiver(asset.manifest(),staging);
        partyboard::net::NativeAssetHost source(asset);
        for(const auto& packet:source.control(receiver.request()))receiver.accept(packet);
        const auto received=staging.str();
        if(!receiver.complete()||receiver.receivedBytes()!=50000||received.size()!=50000)return 12;
        for(unsigned i=0;i<50000;++i)if(uint8_t(received[i])!=uint8_t((i*23)&255))return 13;
        source.control(receiver.completion());if(!source.complete())return 14;
    }
    for(int scenario=0;scenario<4;++scenario){
        auto manifest=asset.manifest();if(scenario==1)manifest["id"]=std::string(64,'0');
        std::ostringstream staging;partyboard::net::NativeAssetReceiver receiver(manifest,staging);
        receiver.request();bool rejected=false;
        try{
            for(unsigned i=0;i<4;++i){auto packet=asset.packet(scenario==2?1:i);
                if(scenario==0)packet.back()^=1;
                if(scenario==3)staging.setstate(std::ios::badbit);
                receiver.accept(packet);
            }
        }catch(const std::exception&){rejected=true;}
        if(!rejected||receiver.complete())return 15;
        rejected=false;try{receiver.completion();}catch(const std::exception&){rejected=true;}if(!rejected)return 16;
    }
    {std::ofstream file(directory/"manifest.json");file<<asset.manifest().dump();}
    partyboard::net::NativeAssetHost sender(asset);
    const auto window=sender.control(R"({"type":"get","start":0,"count":4})");
    if(window.size()!=4)return 6;
    sender.control(nlohmann::ordered_json{{"type","complete"},{"id",asset.manifest()["id"]}}.dump());
    if(!sender.complete())return 7;
    for(const auto* bad:{R"({"type":"get","start":0,"count":17})",R"({"type":"get","start":-1,"count":1})"}){
        partyboard::net::NativeAssetHost invalid(asset);bool rejected=false;
        try{invalid.control(bad);}catch(const std::exception&){rejected=true;}if(!rejected)return 8;
    }
    partyboard::net::NativeAssetHost resumed(asset);
    if(resumed.control(R"({"type":"get","start":2,"count":2})").size()!=2)return 9;
    bool outOfOrder=false;try{resumed.control(R"({"type":"get","start":0,"count":1})");}catch(const std::exception&){outOfOrder=true;}
    if(!outOfOrder)return 10;
    partyboard::net::NativeAssetHost cancelled(asset);
    cancelled.control(R"({"type":"get","start":0,"count":1})");
    cancelled.cancel();cancelled.cancel();
    for(const auto& pending:{std::string(R"({"type":"get","start":1,"count":1})"),
        nlohmann::ordered_json{{"type","complete"},{"id",asset.manifest()["id"]}}.dump()}){
        bool rejected=false;try{cancelled.control(pending);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||cancelled.complete())return 11;
    }
    for(unsigned index=0;index<asset.manifest()["count"].get<unsigned>();++index){
        auto packet=asset.packet(index);std::ofstream file(directory/(std::to_string(index)+".bin"),std::ios::binary);
        file.write(reinterpret_cast<const char*>(packet.data()),packet.size());
    }
    bool rejected=false;try{asset.packet(4);}catch(const std::out_of_range&){rejected=true;}
    if(!rejected)return 3;
    {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.put(char(99));}
    rejected=false;try{asset.packet(0);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)return 4;
    std::cout<<"PASS native asset sender and receiver: exact payload, chunk/root integrity, order, staging failure and changed-file rejection\n";
}
