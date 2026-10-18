#pragma once
#include "crypto.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <utility>

namespace partyboard::net {
// Call before Aurora opens the profile database. Measure and install the same
// immutable snapshot; a peer receipt must never supply local provenance.
struct NativePipelineSeed {
    std::vector<unsigned char> bytes;
    std::string sha256;
};
inline std::shared_ptr<const NativePipelineSeed> readNativePipelineSeed(
        const std::filesystem::path& source,const std::string& expectedHash) {
    const auto size=std::filesystem::file_size(source);
    if(size<100 || size>32*1024*1024) throw std::invalid_argument("Bounded pipeline seed required");
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    std::ifstream input(source,std::ios::binary);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!input || input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Pipeline seed changed while reading");
    if(std::memcmp(bytes.data(),"SQLite format 3\0",16)) throw std::invalid_argument("SQLite pipeline seed required");
    const auto digest=crypto::sha256(bytes);
    constexpr char hex[]="0123456789abcdef";
    std::string hash;
    for(auto value:digest){hash+=hex[value>>4];hash+=hex[value&15];}
    if(hash!=expectedHash) throw std::runtime_error("Pipeline seed identity mismatch");
    return std::make_shared<const NativePipelineSeed>(NativePipelineSeed{std::move(bytes),std::move(hash)});
}
inline std::string installNativePipelineSeed(const std::filesystem::path& source,
        const std::filesystem::path& profile, const std::string& expectedHash) {
    const auto seed=readNativePipelineSeed(source,expectedHash);
    if(profile.empty()) throw std::invalid_argument("Private pipeline profile required");
    std::filesystem::create_directories(profile);
    const auto destination=profile/"pipeline_cache.db";
    if(std::filesystem::exists(destination) || std::filesystem::exists(profile/"pipeline_cache.db-wal") ||
            std::filesystem::exists(profile/"pipeline_cache.db-shm"))
        throw std::runtime_error("Pipeline seed requires a fresh private profile");
    std::ofstream output(destination,std::ios::binary);
    output.write(reinterpret_cast<const char*>(seed->bytes.data()),static_cast<std::streamsize>(seed->bytes.size()));
    output.close();
    if(!output) throw std::runtime_error("Pipeline seed installation failed");
    return seed->sha256;
}
}
