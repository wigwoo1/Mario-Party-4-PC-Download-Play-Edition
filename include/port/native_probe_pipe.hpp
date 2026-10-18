#pragma once
#ifdef _WIN32
#include <windows.h>
#include <string>
#include <vector>
#include <stdexcept>
namespace partyboard::net {
// Local diagnostic only: bounded nonblocking stdin pipe, never a gameplay or
// network authorization channel. UI/game thread never waits for a writer.
class NativeProbePipe {
    std::string pending_;
public:
    std::vector<std::string> poll(){
        const auto handle=GetStdHandle(STD_INPUT_HANDLE);DWORD available=0;
        if(!PeekNamedPipe(handle,nullptr,0,nullptr,&available,nullptr))throw std::runtime_error("Native probe stdin pipe unavailable");
        if(available){
            char chunk[4096];DWORD read=0;
            if(!ReadFile(handle,chunk,available>sizeof(chunk)?sizeof(chunk):available,&read,nullptr))throw std::runtime_error("Native probe stdin read failed");
            pending_.append(chunk,read);
            if(pending_.size()>100000)throw std::runtime_error("Native probe command exceeds bound");
        }
        std::vector<std::string> lines;
        for(size_t end;(end=pending_.find('\n'))!=std::string::npos;){
            auto line=pending_.substr(0,end);pending_.erase(0,end+1);
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            lines.push_back(std::move(line));
        }
        return lines;
    }
};
}
#endif
