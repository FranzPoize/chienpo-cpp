#pragma once
#include "chienpo/game.hpp"
namespace chienpo {
void build_draw_lists(Game& game);
Frames load_ilda(const std::filesystem::path& path);
const Frames& get_from_frame_cache(Game& game,const std::string& path);
void load_animation_list(Game& game,const AnimationPathMap& paths);
void add_animation_to_thing(Game& game,Thing& thing,const std::string& state,const std::string& path,double ticks,bool loop);
void change_animation_on_thing(Game& game,Thing& thing,const std::string& state,std::size_t index,const std::string& path,double ticks,bool loop);
void remove_animation_from_thing(Thing& thing,const std::string& state,std::size_t index);
void advance_animation(Game& game,Thing& thing,double tick_advance=1);
void change_animation_state(Thing& thing,const std::string& state);
double get_animation_state_duration(Game& game,Thing& thing,const std::string& state);
Frame get_current_frame(Game& game,Thing& thing);
TransformMatrix multiply_matrix(const TransformMatrix& a,const TransformMatrix& b);
Vec2 apply_projection_matrix(const TransformMatrix& matrix,Vec2 point);
void apply_perspective_transform(std::vector<DacPoint>& points,const TransformMatrix& matrix);
TransformMatrix compute_projection(const std::array<Vec2,4>& cursors);
void update_calibration(Game& game,Thing& scene,const CommandState& commands);
} // namespace chienpo
