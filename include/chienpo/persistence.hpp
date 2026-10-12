#pragma once
#include "chienpo/game.hpp"
namespace chienpo {
bool load_level(Game& game,const std::filesystem::path& path);
bool save_level(Game& game,const std::filesystem::path& path);
Thing_idx load_blueprint(Game& game,const std::filesystem::path& path,Thing_idx parent=0);
bool save_blueprint(Game& game,Thing_idx entity,const std::filesystem::path& path);
void refresh_resources(Game& game);
std::string serialize_world(const Game& game);
bool deserialize_world(Game& game,const std::string& json);
} // namespace chienpo
