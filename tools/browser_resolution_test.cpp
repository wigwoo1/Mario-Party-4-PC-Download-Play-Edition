#include <port/browser_resolution.hpp>

constexpr bool allOutputPresetsSupported() {
    constexpr int widths[]={640,960,1280,1920,2560};
    constexpr int aspects[][2]={{4,3},{16,9},{16,10},{21,9}};
    for(int width:widths)for(const auto& aspect:aspects) {
        const int height=(width*aspect[1]+aspect[0]/2)/aspect[0];
        if(!partyboard::browserResolutionSupported(width,height))return false;
    }
    return true;
}
static_assert(allOutputPresetsSupported());
static_assert(partyboard::browserResolutionSupported(640,480));
static_assert(partyboard::browserResolutionSupported(960,540));
static_assert(partyboard::browserResolutionSupported(1280,800));
static_assert(partyboard::browserResolutionSupported(1920,1080));
static_assert(partyboard::browserResolutionSupported(2560,1600));
static_assert(partyboard::browserResolutionSupported(640,274));
static_assert(partyboard::browserResolutionSupported(960,411));
static_assert(partyboard::browserResolutionSupported(1280,549));
static_assert(partyboard::browserResolutionSupported(1920,823));
static_assert(partyboard::browserResolutionSupported(2560,1097));
static_assert(!partyboard::browserResolutionSupported(1280,400));
static_assert(!partyboard::browserResolutionSupported(1366,768));
static_assert(!partyboard::browserResolutionSupported(2560,2200));

int main() { return 0; }
