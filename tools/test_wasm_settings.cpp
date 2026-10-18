#include <port/settings.h>
#include <port/config.hpp>
#include <port/io.hpp>
#include <cassert>
#include <cstdio>
#include <filesystem>

std::filesystem::path PartyBoard_ConfigPath = "/";

int main() {
    partyboard::registerSettings();
    partyboard::config::FinishRegistration();
    auto& settings = partyboard::getSettings();
    assert(!partyboard_settings_enableTurboKeybind());
    assert(partyboard_settings_card_file_type() == CARD_RAWIMAGE);
    assert(partyboard_settings_audio_channels()==2);
    assert(partyboard_settings_audio_gain()==0.8f);
    settings.audio.masterVolume.setValue(-10);
    assert(partyboard_settings_audio_gain()==0.0f);
    settings.audio.masterVolume.setValue(150);
    assert(partyboard_settings_audio_gain()==1.0f);
    settings.audio.masterVolume.setValue(37);
    assert(partyboard_settings_audio_gain()==0.37f);
    settings.audio.muted.setValue(true);
    assert(partyboard_settings_audio_gain()==0.0f);
    assert(settings.audio.masterVolume.getValue()==37);
    for(int invalid : {-1,0,1,3,5,7,2147483647}) {
        settings.audio.outputChannels.setValue(invalid);
        assert(partyboard_settings_audio_channels()==2);
    }
    settings.audio.outputChannels.setValue(6);
    assert(partyboard_settings_audio_channels()==6);
    settings.game.enableTurboKeybind.setValue(true);
    settings.audio.masterVolume.setValue(37);
    partyboard::config::Save();
    settings.game.enableTurboKeybind.setValue(false);
    settings.audio.masterVolume.setValue(80);
    settings.audio.muted.setValue(false);
    settings.audio.outputChannels.setValue(2);
    partyboard::config::LoadFromUserPreferences();
    assert(partyboard_settings_enableTurboKeybind());
    assert(settings.audio.masterVolume.getValue() == 37);
    assert(settings.audio.muted.getValue());
    assert(partyboard_settings_audio_gain()==0.0f);
    settings.audio.muted.setValue(false);
    assert(partyboard_settings_audio_gain()==0.37f);
    assert(partyboard_settings_audio_channels()==6);

    bool rejected = false;
    try {
        settings.audio.masterVolume.getImpl()->loadFromJson(
            settings.audio.masterVolume, nlohmann::json::array({1, 2}));
    } catch (const std::exception&) { rejected = true; }
    assert(rejected && settings.audio.masterVolume.getValue() == 37);
    partyboard::config::LoadFromFileName("/missing-config.json");
    assert(settings.audio.masterVolume.getValue() == 37);
    std::remove("/config.json");
    partyboard::io::FileStream::WriteAllText("/broken-config.json", "{\"audio.masterVolume\":12,");
    partyboard::config::LoadFromFileName("/broken-config.json");
    assert(settings.audio.masterVolume.getValue() == 37);
    assert(partyboard_settings_enableTurboKeybind());
    partyboard::io::FileStream::WriteAllText("/broken-config.json", "{\"audio.masterVolume\":24}");
    partyboard::config::LoadFromFileName("/broken-config.json");
    assert(settings.audio.masterVolume.getValue() == 24);
    std::remove("/broken-config.json");
    puts("Browser malformed config passed: partial JSON preserves state and corrected JSON loads afterward.");
    puts("Browser settings passed: defaults, C accessors, temporary file roundtrip and validation exceptions.");
}
