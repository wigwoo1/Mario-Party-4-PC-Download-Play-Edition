#include "../src/port/native_lobby_music.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=3)throw std::runtime_error("Packaged runtime path required");
 const bool physical=argc==3&&std::string(argv[2]).starts_with("device");
 const unsigned channels=argc==3&&(std::string(argv[2])=="device6"||std::string(argv[2])=="surround")?6:2;
 if(!physical)SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");partyboard::NativeLobbyMusic music;
 if(argc==3&&std::string(argv[2])=="playlist"){
  if(!music.start(argv[1],channels))throw std::runtime_error("Playlist audio open failed");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
  while(music.completedTracks()<2){
   if(music.error()||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Full playlist failed");
   if(music.queuedBytes()>int(48000*channels*2*12/100))throw std::runtime_error("Audio queue exceeded 120ms bound");
   std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  while(music.decodedFrames(0)<=5664000&&!music.error()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if(music.error()||music.decodedFrames(0)<=5664000||music.decodedFrames(1)!=4709376)throw std::runtime_error("Second track incomplete");
  music.stop();if(music.queuedBytes())throw std::runtime_error("Stop left queued audio");
  std::cout<<"PASS native full two-song playlist and loop back; SDL dummy device, no audible acceptance\n";return 0;
 }
 for(int i=0;i<3;i++){
  const float initialGain=i==0?0.0f:i==1?0.8f:2.0f;
  if(!music.start(argv[1],channels,initialGain))throw std::runtime_error("Dummy audio open failed");
  if(std::fabs(music.gain()-std::clamp(initialGain,0.0f,1.0f))>0.0001f)throw std::runtime_error("Initial gain not applied before decoding");
  if(!music.setGain(0.0f)||music.gain()!=0.0f)throw std::runtime_error("Mute gain failed");
  if(!music.setGain(0.8f)||std::fabs(music.gain()-0.8f)>0.0001f)throw std::runtime_error("Master/music gain failed");
  if(!music.setGain(2.0f)||music.gain()!=1.0f)throw std::runtime_error("Upper gain clamp failed");
  if(!music.setGain(-1.0f)||music.gain()!=0.0f)throw std::runtime_error("Lower gain clamp failed");
  if(music.setGain(NAN)||music.gain()!=0.0f)throw std::runtime_error("Nonfinite gain accepted");
  music.setGain(0.8f);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  if(music.error()||!music.decodedFrames(0)||music.queuedBytes()<0||music.queuedBytes()>int(48000*channels*2*12/100))throw std::runtime_error("Decode/queue failed");
  if(physical){const char* driver=SDL_GetCurrentAudioDriver();if(!driver||std::string(driver)=="dummy")throw std::runtime_error("Physical backend required");std::cout<<"Device cycle "<<i<<" driver="<<driver<<" sourceChannels="<<music.sourceChannels()<<" deviceChannels="<<music.deviceChannels()<<" decodedFrames="<<music.decodedFrames(0)<<" queuedBytes="<<music.queuedBytes()<<"\n";}
  music.stop();music.stop();if(music.active()||music.queuedBytes())throw std::runtime_error("Stop retained audio device");
 }
 if(music.start(argv[1],channels,NAN)||music.active()||music.decodedFrames(0))throw std::runtime_error("Invalid initial gain started output");
 if(!music.start("missing-lobby-path/",channels))throw std::runtime_error("Dummy audio restart failed");
 for(int i=0;i<100&&!music.error();i++)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 if(!music.error())throw std::runtime_error("Missing track did not fail");
 music.stop();std::cout<<(physical?"PASS native Opus physical SDL device queue/restart/cancel/missing-track cleanup; no loopback/audible proof\n":"PASS native Opus SDL dummy queue/restart/cancel/missing-track cleanup; no audible acceptance\n");
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
