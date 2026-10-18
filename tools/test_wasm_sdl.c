#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    SDL_Window *window=SDL_CreateWindow("PartyBoard browser platform probe",640,480,0);
    assert(window);
    int width=0,height=0;
    assert(SDL_GetWindowSize(window,&width,&height));
    assert(width==640 && height==480);
    int count=0;
    SDL_JoystickID *pads=SDL_GetGamepads(&count);
    assert(count>=0);
    SDL_free(pads);
    printf("SDL browser probe passed: window %dx%d, gamepad subsystem initialized, devices=%d.\n",width,height,count);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
