#pragma once
#include "port/asset_transfer.hpp"
#include <ostream>

namespace partyboard::net {
// Worker-thread receiver. The caller owns a private staging stream and must
// not mount or publish it until complete(). Manifest names never select paths.
class NativeAssetReceiver {
    nlohmann::ordered_json manifest_;
    std::ostream& output_;
    uint64_t size_=0,received_=0;
    uint32_t count_=0,next_=0,requestedEnd_=0;
    std::vector<uint8_t> hashes_;
    bool failed_=false,complete_=false;
    static uint32_t read32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
public:
    NativeAssetReceiver(nlohmann::ordered_json manifest,std::ostream& output):manifest_(std::move(manifest)),output_(output){
        if(manifest_.at("type")!="manifest"||manifest_.at("version")!=1||manifest_.at("chunk")!=NativeAsset::Chunk||
           !manifest_.at("size").is_number_integer()||!manifest_.at("count").is_number_integer())throw std::invalid_argument("Invalid native asset manifest");
        const auto size=manifest_.at("size").get<int64_t>(),count=manifest_.at("count").get<int64_t>();
        const auto id=manifest_.at("id").get<std::string>(),name=manifest_.at("name").get<std::string>();
        if(size<1||size>2147483648LL||count!=(size+NativeAsset::Chunk-1)/NativeAsset::Chunk||name.size()>160||id.size()!=64||
           !std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})||!output_)
            throw std::invalid_argument("Bounded native asset manifest required");
        size_=uint64_t(size);count_=uint32_t(count);hashes_.reserve(size_t(count_)*32);
    }
    std::string request(){
        if(failed_||complete_||next_!=requestedEnd_)throw std::runtime_error("No native asset request available");
        requestedEnd_=(std::min)(count_,next_+16);
        return nlohmann::ordered_json{{"type","get"},{"start",next_},{"count",requestedEnd_-next_}}.dump();
    }
    void accept(std::span<const uint8_t> packet){
        if(failed_||complete_)throw std::runtime_error("Native asset receiver stopped");
        try{
            if(packet.size()<40||packet.size()>NativeAsset::Chunk+40||next_>=requestedEnd_||read32(packet.data())!=next_||
               read32(packet.data()+4)!=packet.size()-40||packet.size()-40!=std::min<uint64_t>(NativeAsset::Chunk,size_-received_))
                throw std::runtime_error("Unexpected native asset chunk");
            const auto bytes=packet.subspan(40);const auto hash=NativeAsset::digest(bytes);
            if(!std::equal(hash.begin(),hash.end(),packet.begin()+8))throw std::runtime_error("Native asset chunk integrity failed");
            output_.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
            if(!output_)throw std::runtime_error("Native asset staging write failed");
            hashes_.insert(hashes_.end(),hash.begin(),hash.end());received_+=bytes.size();++next_;
            if(next_==count_){
                if(NativeAsset::hex(NativeAsset::digest(hashes_))!=manifest_.at("id").get<std::string>())throw std::runtime_error("Native asset identity mismatch");
                output_.flush();if(!output_)throw std::runtime_error("Native asset staging flush failed");complete_=true;
            }
        }catch(...){failed_=true;throw;}
    }
    std::string completion()const{
        if(!complete_||failed_)throw std::runtime_error("Verified complete native asset required");
        return nlohmann::ordered_json{{"type","complete"},{"id",manifest_.at("id")}}.dump();
    }
    void cancel(){failed_=true;complete_=false;}
    bool complete()const{return complete_&&!failed_;}
    bool needsRequest()const{return !failed_&&!complete_&&next_==requestedEnd_;}
    uint64_t receivedBytes()const{return received_;}
};
}
