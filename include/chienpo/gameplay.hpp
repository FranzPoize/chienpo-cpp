#pragma once
#include "chienpo/game.hpp"
namespace chienpo {
Thing_idx make_entity(Game& game,EntityType kind,Thing_idx parent=0,double x=.5,double y=.5);
Thing_idx spawn_player(Game& game,Thing_idx scene,double x,double y);
void update_gameplay(Game& game,Thing& scene,const CommandState& commands);
void update_editor(Game& game,Thing& scene,const CommandState& commands);
} // namespace chienpo
