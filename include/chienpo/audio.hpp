#pragma once
#include "chienpo/game.hpp"
#include <map>
#include <string>
namespace chienpo {
struct Audio {
 void* implementation=nullptr;
 bool available=false, muted=false;
 std::string error;
 std::map<std::string,float> volumes;
 std::map<std::string,bool> loops;
};
bool audio_open(Audio& audio);
void audio_update(Audio& audio,Game& game);
void audio_stop(Audio& audio,const std::string& name={});
void audio_close(Audio& audio);
std::string sound_asset_name(const std::string& asset);
} // namespace chienpo
