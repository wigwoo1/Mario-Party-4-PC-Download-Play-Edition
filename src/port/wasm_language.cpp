#include <dolphin/os.h>
#include <port/port_version.h>
#include <port/settings.h>
#include <port/wasm_language.h>

// Matches native startup: English by default, PAL selection cached after
// settings registration and disc version initialization.
static u8 selectedLanguage;
extern "C" void PartyBoardBrowserLanguageInit() {
    selectedLanguage = partyboard::version::isRegionPal()
        ? static_cast<u8>(partyboard::getSettings().game.language.getValue()) : 0;
}
u8 OSGetLanguage() { return selectedLanguage; }
