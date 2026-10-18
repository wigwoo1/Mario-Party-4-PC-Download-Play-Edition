#include <SDL3/SDL.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <array>

int main() {
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.name="PartyBoard isolated virtual input probe";
    descriptor.naxes=SDL_GAMEPAD_AXIS_COUNT;
    descriptor.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    descriptor.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    descriptor.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    const auto id=SDL_AttachVirtualJoystick(&descriptor);
    assert(id && SDL_IsGamepad(id));
    auto* joystick=SDL_OpenJoystick(id);
    auto* gamepad=SDL_OpenGamepad(id);
    assert(joystick && gamepad);
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true));
    assert(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
    assert(SDL_GetGamepadButton(gamepad,SDL_GAMEPAD_BUTTON_SOUTH));
    assert(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>30000);
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,false));
    SDL_UpdateJoysticks();SDL_UpdateGamepads();
    assert(!SDL_GetGamepadButton(gamepad,SDL_GAMEPAD_BUTTON_SOUTH));
    char* rawMapping=SDL_GetGamepadMapping(gamepad);
    assert(rawMapping);
    const std::string original(rawMapping);SDL_free(rawMapping);
    for(const auto type:std::array{SDL_GAMEPAD_TYPE_XBOXONE,SDL_GAMEPAD_TYPE_PS5,SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO}) {
        auto mapping=original;
        // Replace any existing explicit type field before adding the test type.
        const auto start=mapping.find(",type:");
        if(start!=std::string::npos) {
            const auto end=mapping.find(',',start+1);
            mapping.erase(start,end==std::string::npos?std::string::npos:end-start);
        }
        if(mapping.back()!=',')mapping+=',';
        mapping+="type:";mapping+=SDL_GetGamepadStringForType(type);mapping+=',';
        assert(SDL_SetGamepadMapping(id,mapping.c_str()));
        SDL_UpdateGamepads();
        assert(SDL_GetGamepadType(gamepad)==type);
        printf("Virtual mapped family: %s\n",SDL_GetGamepadStringForType(type));
    }
    SDL_CloseGamepad(gamepad);SDL_CloseJoystick(joystick);
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    puts("PASS virtual gamepad input, three explicit family mappings and cleanup; no physical device used.");
}
