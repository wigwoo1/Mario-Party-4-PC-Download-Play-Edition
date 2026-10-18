// Diagnostic only: elapsed host time is never a gameplay clock or assertion.
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

template<class Wait> void measure(const char* phase,const char* name,Wait wait) {
    using Clock=std::chrono::steady_clock;
    std::vector<double> samples;
    for(int i=0;i<120;++i){
        const auto before=Clock::now();wait();
        samples.push_back(std::chrono::duration<double,std::milli>(Clock::now()-before).count());
    }
    double sum=0;for(const auto value:samples)sum+=value;
    std::sort(samples.begin(),samples.end());
    std::printf("{\"phase\":\"%s\",\"wait\":\"%s\",\"samples\":%zu,\"meanMs\":%.6f,\"medianMs\":%.6f,\"p95Ms\":%.6f,\"minMs\":%.6f,\"maxMs\":%.6f}\n",
        phase,name,samples.size(),sum/samples.size(),samples[60],samples[114],samples.front(),samples.back());
    std::fflush(stdout);
}
void measureBoth(const char* phase){
    measure(phase,"std-sleep-1ms",[]{std::this_thread::sleep_for(std::chrono::milliseconds(1));});
    measure(phase,"SDL-Delay-1ms",[]{SDL_Delay(1);});
}
int main(){
    measureBoth("before-video");
    if(!SDL_Init(SDL_INIT_VIDEO)){std::fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 1;}
    auto* window=SDL_CreateWindow("Muted input wait diagnostic",64,64,SDL_WINDOW_HIDDEN);
    if(!window){std::fprintf(stderr,"SDL window: %s\n",SDL_GetError());SDL_Quit();return 1;}
    measureBoth("hidden-window");
    SDL_DestroyWindow(window);SDL_Quit();
}
