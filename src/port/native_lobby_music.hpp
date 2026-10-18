#pragma once
#include "pcm_output.h"
#include <opusfile.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace partyboard {
// Local lobby audio never participates in synchronized game timing.
class NativeLobbyMusic {
    PartyboardPcmOutput output_{};
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::atomic<int> error_{0};
    std::atomic<unsigned> completedTracks_{0};
    std::array<std::atomic<uint64_t>,2> decodedFrames_{};
    void run(std::array<std::string,2> tracks) noexcept {
        OggOpusFile* file=nullptr;
        try {
            unsigned track=0;
            while(!stopping_.load()) {
                int error=0;file=op_open_file(tracks[track].c_str(),&error);
                if(!file){error_=error?error:-1;break;}
                int16_t pcm[1920];
                while(!stopping_.load()) {
                    // At most 100ms queued plus one 20ms decode block.
                    const int queued=SDL_GetAudioStreamQueued(output_.stream);
                    if(queued<0){error_=-1000;break;}
                    if(queued>=48000*output_.channels*2/10){std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
                    const int frames=op_read_stereo(file,pcm,1920);
                    if(frames<0){error_=frames;break;}
                    if(!frames)break;
                    if(PartyboardPcmOutputSubmitStereo(&output_,pcm,frames)!=1){error_=-1001;break;}
                    decodedFrames_[track]+=frames;
                }
                op_free(file);file=nullptr;
                if(error_.load()||stopping_.load())break;
                completedTracks_++;
                track=(track+1)%tracks.size();
            }
        }catch(...){error_=-1002;if(file)op_free(file);}
    }
public:
    NativeLobbyMusic()=default;
    NativeLobbyMusic(const NativeLobbyMusic&)=delete;
    NativeLobbyMusic& operator=(const NativeLobbyMusic&)=delete;
    ~NativeLobbyMusic(){stop();}
    bool start(const std::string& base,unsigned channels=2,float initialGain=0.5f) {
        stop();error_=0;completedTracks_=0;for(auto& count:decodedFrames_)count=0;stopping_=false;
        if(!std::isfinite(initialGain)){error_=-1004;return false;}
        if(!PartyboardPcmOutputOpenChannels(&output_,48000,channels)){error_=-1003;return false;}
        if(!setGain(initialGain)){PartyboardPcmOutputClose(&output_);error_=-1004;return false;}
        try {worker_=std::thread([this,base]{run({base+"audio/wifi-menu.opus",base+"audio/currently-spectating.opus"});});}
        catch(...){PartyboardPcmOutputClose(&output_);error_=-1002;return false;}
        return true;
    }
    void stop() {
        stopping_=true;
        if(worker_.joinable())worker_.join();
        PartyboardPcmOutputClose(&output_);
    }
    // Called by the UI owner; SDL synchronizes stream gain with audio output.
    bool setGain(float gain) {
        return output_.stream && std::isfinite(gain)
            && SDL_SetAudioStreamGain(output_.stream,std::clamp(gain,0.0f,1.0f));
    }
    float gain() const {return output_.stream?SDL_GetAudioStreamGain(output_.stream):0.0f;}
    unsigned sourceChannels() const {return output_.channels;}
    unsigned deviceChannels() const {return PartyboardPcmOutputDeviceChannels(&output_);}
    bool active() const {return output_.stream!=nullptr;}
    int error() const {return error_.load();}
    unsigned completedTracks() const {return completedTracks_.load();}
    uint64_t decodedFrames(unsigned track) const {return decodedFrames_.at(track).load();}
    int queuedBytes() const {return output_.stream?SDL_GetAudioStreamQueued(output_.stream):0;}
};
}
