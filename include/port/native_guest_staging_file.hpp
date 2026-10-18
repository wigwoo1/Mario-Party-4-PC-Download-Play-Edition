#pragma once
#include "port/native_guest_join_session.hpp"
#include "port/recovery_credentials.hpp"

namespace partyboard::net {
// Caller chooses the private base directory. The peer never supplies a path.
// Destroy Join and detach/close the engine before disposing this file owner.
class NativeGuestStagingFile {
    std::filesystem::path directory_,path_;
    std::ofstream stream_;
    bool sealed_=false;
public:
    explicit NativeGuestStagingFile(const std::filesystem::path& privateBase){
        const auto base=std::filesystem::absolute(privateBase);
        std::filesystem::create_directories(base);
        for(unsigned attempt=0;attempt<8;++attempt){
            const auto candidate=base/("join-"+createRecoveryNonce());
            if(std::filesystem::create_directory(candidate)){directory_=candidate;break;}
        }
        if(directory_.empty())throw std::runtime_error("Could not reserve private Join staging directory");
        path_=directory_/"game.ciso";
        stream_.open(path_,std::ios::binary|std::ios::out);
        if(!stream_){std::error_code ignored;std::filesystem::remove(path_,ignored);std::filesystem::remove(directory_,ignored);throw std::runtime_error("Could not open private Join staging file");}
    }
    NativeGuestStagingFile(const NativeGuestStagingFile&)=delete;
    NativeGuestStagingFile& operator=(const NativeGuestStagingFile&)=delete;
    std::ostream& stream(){if(sealed_)throw std::runtime_error("Join staging file is sealed");return stream_;}
    const std::filesystem::path& verifiedPath(const NativeGuestJoinSession& session)const{
        if(!session.usesStaging(stream_)||!session.assetsComplete()||!stream_)throw std::runtime_error("Verified staged Join asset required");
        return path_;
    }
    const std::filesystem::path& sealVerified(const NativeGuestJoinSession& session){
        (void)verifiedPath(session);
        if(!sealed_){stream_.close();if(stream_.fail())throw std::runtime_error("Could not close verified Join staging file");sealed_=true;}
        return path_;
    }
    ~NativeGuestStagingFile(){
        stream_.close();std::error_code ignored;
        // Exact owned files only; never recursively delete the caller's base.
        std::filesystem::remove(path_,ignored);std::filesystem::remove(directory_,ignored);
    }
};
}
