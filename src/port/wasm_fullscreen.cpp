#include <emscripten.h>

extern "C" int __real_emscripten_exit_fullscreen(void);

// SDL applies its windowed flag when showing the canvas. The page may already
// own fullscreen on HTML/game-view while waiting for assets. That ancestor is
// not SDL's fullscreen window; only the page's own exit control should end it.
extern "C" int __wrap_emscripten_exit_fullscreen(void) {
    const bool pageOwnsFullscreen=EM_ASM_INT({
        if(typeof document==='undefined')return 0;
        const target=document.fullscreenElement||document.webkitFullscreenElement;
        const canvas=Module.canvas;
        return target&&canvas&&target!==canvas&&target.contains(canvas)?1:0;
    });
    if(pageOwnsFullscreen)return 0; // EMSCRIPTEN_RESULT_SUCCESS; SDL stays windowed.
    return __real_emscripten_exit_fullscreen();
}
