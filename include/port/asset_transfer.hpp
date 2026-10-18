#pragma once
#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <span>
#include <stdexcept>
#include <optional>
#include <stop_token>

namespace partyboard::net {
// transfer.mjs wire format. Prepare/read on an asset worker, not RTC callbacks
// or the simulation thread. No local pathname is exposed in the manifest.
class NativeAsset {
public:
    static constexpr uint32_t Chunk=16*1024-40;
    using Hash=std::array<uint8_t,32>;
    NativeAsset(const std::filesystem::path& path,const std::string& publicName,std::stop_token stop={}):file_(path,std::ios::binary) {
        size_=std::filesystem::file_size(path);
        if(!file_||!size_||size_>2147483648ULL||publicName.size()>160)throw std::invalid_argument("Bounded private asset required");
        count_=uint32_t((size_+Chunk-1)/Chunk);hashes_.reserve(size_t(count_)*32);
        for(uint32_t index=0;index<count_;++index){if(stop.stop_requested())throw std::runtime_error("Native asset preparation cancelled");auto bytes=read(index);auto hash=digest(bytes);hashes_.insert(hashes_.end(),hash.begin(),hash.end());}
        id_=hex(digest(hashes_));
        manifest_={{"type","manifest"},{"version",1},{"id",id_},{"size",size_},{"chunk",Chunk},{"count",count_},{"name",publicName}};
    }
    nlohmann::ordered_json manifest() const {return manifest_;}
    std::vector<uint8_t> packet(uint32_t index) {
        auto bytes=read(index);const auto hash=digest(bytes);
        if(!std::equal(hash.begin(),hash.end(),hashes_.begin()+size_t(index)*32))throw std::runtime_error("Private asset changed after preparation");
        std::vector<uint8_t> result(40+bytes.size());
        put32(result.data(),index);put32(result.data()+4,uint32_t(bytes.size()));
        std::copy(hash.begin(),hash.end(),result.begin()+8);std::copy(bytes.begin(),bytes.end(),result.begin()+40);
        return result;
    }
private:
    friend class NativeAssetReceiver;
    static Hash digest(std::span<const uint8_t> bytes) {
        return crypto::sha256(bytes);
    }
    static std::string hex(const Hash& hash){std::string text;constexpr char d[]="0123456789abcdef";for(auto b:hash){text.push_back(d[b>>4]);text.push_back(d[b&15]);}return text;}
    static void put32(uint8_t* out,uint32_t value){for(unsigned i=0;i<4;++i)out[i]=uint8_t(value>>(24-8*i));}
    std::vector<uint8_t> read(uint32_t index) {
        if(index>=count_)throw std::out_of_range("Asset chunk out of range");
        const auto offset=uint64_t(index)*Chunk;
        std::vector<uint8_t> bytes(size_t(std::min<uint64_t>(Chunk,size_-offset)));
        file_.clear();file_.seekg(std::streamoff(offset));file_.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
        if(!file_||file_.gcount()!=std::streamsize(bytes.size()))throw std::runtime_error("Private asset read failed");
        return bytes;
    }
    std::ifstream file_;
    uint64_t size_{};
    uint32_t count_{};
    std::vector<uint8_t> hashes_;
    std::string id_;
    nlohmann::ordered_json manifest_;
};
// One assigned guest, worker-thread only. First get may resume a persisted
// prefix; subsequent requests acknowledge consecutive verified windows.
class NativeAssetHost {
public:
    explicit NativeAssetHost(NativeAsset& asset):asset_(asset),manifest_(asset.manifest()) {}
    std::vector<std::vector<uint8_t>> control(std::string_view data) {
        if(failed_||complete_||cancelled_)throw std::runtime_error("Asset sender stopped");
        try {
            if(data.size()>2048)throw std::runtime_error("Oversized asset control");
            const auto message=nlohmann::ordered_json::parse(data);
            const auto count=manifest_["count"].get<uint32_t>();
            if(message.value("type","")=="complete") {
                if(message.value("id","")!=manifest_["id"].get<std::string>()||(next_&&*next_!=count))
                    throw std::runtime_error("Invalid asset completion");
                complete_=true;return {};
            }
            if(message.value("type","")!="get"||!message.contains("start")||!message.contains("count")||
               !message["start"].is_number_integer()||!message["count"].is_number_integer())
                throw std::runtime_error("Invalid asset request");
            const auto start=message["start"].get<int64_t>(),window=message["count"].get<int64_t>();
            if(start<0||start>=count||window<1||window>16||start+window>count||(next_&&start!=*next_))
                throw std::runtime_error("Invalid asset request window");
            std::vector<std::vector<uint8_t>> packets;packets.reserve(size_t(window));
            for(auto index=start;index<start+window;++index)packets.push_back(asset_.packet(uint32_t(index)));
            next_=uint32_t(start+window);return packets;
        }catch(...){failed_=true;throw;}
    }
    // Deliver transport closure to the owning worker before processing queued
    // requests. This does not authorize resuming a replacement connection.
    void cancel() {cancelled_=true;}
    bool complete() const {return complete_;}
private:
    NativeAsset& asset_;
    nlohmann::ordered_json manifest_;
    std::optional<uint32_t> next_;
    bool failed_{},complete_{},cancelled_{};
};
}
