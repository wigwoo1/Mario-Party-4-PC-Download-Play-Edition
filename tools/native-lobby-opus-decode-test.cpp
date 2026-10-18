#include <opusfile.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::runtime_error("Two lobby tracks required");
  for(int i=1;i<3;i++){
   int error=0;OggOpusFile* file=op_open_file(argv[i],&error);
   if(!file)throw std::runtime_error("Opus open rejected: "+std::to_string(error));
   const auto expected=op_pcm_total(file,-1);int16_t pcm[11520];int64_t frames=0;uint64_t energy=0;int samples;
   while((samples=op_read_stereo(file,pcm,11520))>0){frames+=samples;for(int j=0;j<samples*2;j++){const int64_t value=pcm[j];energy+=uint64_t(value*value);}}
   op_free(file);
   if(samples<0||expected<=0||frames!=expected||!energy)throw std::runtime_error("Incomplete/silent Opus decode");
   std::cout<<"PASS track "<<i<<" frames="<<frames<<" sampleRate=48000 stereo energy="<<energy<<"\n";
  }
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
