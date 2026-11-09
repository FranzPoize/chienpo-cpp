#include "chienpo/gameplay.hpp"
#include "chienpo/persistence.hpp"
#include "chienpo/render.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace chienpo;
namespace {
void check(bool value, const std::string & message)
{
    if (!value) throw std::runtime_error(message);
}
bool close(double a, double b)
{
    return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) < 1e-9;
}
bool same(double a, double b) { return close(a, b); }
bool same(const Vec2 & a, const Vec2 & b)
{
    return close(a.x, b.x) && close(a.y, b.y);
}
bool same(const Color & a, const Color & b)
{
    return close(a.r, b.r) && close(a.g, b.g) && close(a.b, b.b);
}
bool same(const DacPoint & a, const DacPoint & b)
{
    return close(a.x, b.x) && close(a.y, b.y) && close(a.r, b.r)
           && close(a.g, b.g) && close(a.b, b.b);
}
bool same(const Animation & a, const Animation & b)
{
    return a.path == b.path && a.m_loop == b.m_loop
           && close(a.m_tick_per_frame, b.m_tick_per_frame);
}
bool same(const Trigger & a, const Trigger & b)
{
    return a.m_trigger_type == b.m_trigger_type
           && a.m_trigger_target == b.m_trigger_target
           && close(a.m_trigger_count_target, b.m_trigger_count_target)
           && a.m_trigger_action == b.m_trigger_action
           && close(a.m_trigger_percentage, b.m_trigger_percentage)
           && close(a.m_trigger_delay, b.m_trigger_delay)
           && close(a.m_trigger_elapsed, b.m_trigger_elapsed)
           && a.m_triggered == b.m_triggered;
}
template <class T>
bool same(const T & a, const T & b)
{
    return a == b;
}
template <class T, std::size_t N>
bool same(const std::array<T, N> & a, const std::array<T, N> & b)
{
    for (std::size_t i = 0; i < N; ++i)
        if (!same(a[i], b[i])) return false;
    return true;
}
template <class T>
bool same(const std::vector<T> & a, const std::vector<T> & b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!same(a[i], b[i])) return false;
    return true;
}
bool same(const AnimationPathMap & a, const AnimationPathMap & b)
{
    if (a.size() != b.size()) return false;
    for (const auto & [key, value] : a)
    {
        auto it = b.find(key);
        if (it == b.end() || !same(value, it->second)) return false;
    }
    return true;
}
struct Scratch
{
    std::filesystem::path path;
    Scratch()
    {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i < 100; ++i)
        {
            path = std::filesystem::temp_directory_path()
                   / ("chienpo-tests-" + std::to_string(stamp) + "-"
                      + std::to_string(i));
            if (std::filesystem::create_directory(path)) return;
        }
        throw std::runtime_error("Cannot create unique scratch directory");
    }
    ~Scratch()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
void pool_and_hierarchy()
{
    auto pool = std::make_unique<Things>();
    check(!ent_valid(pool->get(0)), "slot zero is a sentinel");
    for (std::size_t i = 1; i < MAX_THINGS; ++i)
        check(pool->add(EntityType::decor) == i,
              "allocation preserves stable consecutive indices");
    check(pool->add(EntityType::decor) == 0,
          "pool capacity leaves sentinel reserved");
    pool->get(51).m_name = "old data";
    pool->remove(51);
    check(pool->add(EntityType::platform) == 51,
          "removed slot is reused when full");
    check(pool->get(51).m_name != "old data",
          "reused slot resets previous entity data");
    check(pool->validate(), "full pool with reused slot validates");
    pool->reset();
    const auto root = pool->add(EntityType::scene_root_node);
    const auto a = pool->add(EntityType::decor),
               b = pool->add(EntityType::decor),
               c = pool->add(EntityType::decor);
    check(add_child(*pool, root, a) && add_child(*pool, root, b)
              && add_child(*pool, root, c),
          "attach siblings");
    check(pool->children(root) == std::vector<Thing_idx>{a, b, c},
          "children retain insertion order");
    check(pool->get(a).prev_child_idx == c && pool->get(c).next_child_idx == a,
          "sibling list is circular");
    pool->get(b).m_active = false;
    check(pool->children(root, true) == std::vector<Thing_idx>{a, c},
          "inactive children excluded only when requested");
    remove_child(*pool, b);
    check(pool->get(a).next_child_idx == c && pool->get(c).prev_child_idx == a,
          "middle removal repairs both links");
    check(add_child(*pool, a, b), "reparent detached node");
    check(!add_child(*pool, b, root) && !add_child(*pool, a, a),
          "ancestor and self cycles are rejected");
    check(pool->get(root).parent_idx == 0 && pool->get(b).parent_idx == a,
          "rejected cycle preserves hierarchy");
    auto copy = std::make_unique<Things>(*pool);
    check(
        copy->get(b).m_things == copy.get() && copy->get(b).parent().m_idx == a,
        "copy rebinds parent accessor owner");
    auto assigned = std::make_unique<Things>();
    *assigned = *copy;
    check(assigned->get(a).m_things == assigned.get(),
          "assignment rebinds entity owners");
    auto & linked = pool->get(c);
    linked.m_portal_destination_idx = a;
    linked.m_moving_platform_idx = b;
    linked.new_parent_after_transaction = a;
    linked.m_triggers.emplace_back();
    linked.m_triggers.back().m_trigger_target = {a, b, c};
    remove_hierarchy(*pool, a);
    check(!ent_valid(pool->get(a)) && !ent_valid(pool->get(b)),
          "recursive removal removes descendants including inactive ones");
    check(pool->children(root) == std::vector<Thing_idx>{c},
          "recursive removal preserves unrelated sibling");
    check(linked.m_portal_destination_idx == 0
              && linked.m_moving_platform_idx == 0
              && linked.new_parent_after_transaction == 0,
          "deletion clears incoming links before slot reuse");
    check(
        linked.m_triggers.front().m_trigger_target == std::vector<Thing_idx>{c},
        "deletion removes only deleted trigger targets");
    remove_child(*pool, c);
    check(pool->get(root).first_child_idx == 0 && pool->validate(),
          "removing final child empties valid hierarchy");
    check(copy->validate() && ent_valid(copy->get(b)),
          "copy remains independent after original removal");
}
void input_edges()
{
    CommandState state;
    Input input;
    input.jump = true;
    input.add_portal = true;
    input.delete_key = true;
    transform_command_state(state, input);
    check(state.jump.pressed && state.jump.positive_edge
              && !state.jump.negative_edge,
          "first press raises positive edge");
    check(state.add_portal.positive_edge && state.delete_key.positive_edge,
          "editor actions have press edges");
    transform_command_state(state, input);
    check(state.jump.pressed && !state.jump.positive_edge
              && !state.jump.negative_edge,
          "held key does not retrigger");
    transform_command_state(state, Input{});
    check(!state.jump.pressed && state.jump.negative_edge,
          "release raises negative edge");
    remove_command_edges(state);
    check(!state.jump.negative_edge && !state.add_portal.negative_edge,
          "edge cleanup clears all command edges");
}

// Explicit field lists keep serialization coverage independent of its
// implementation.
void fill_fields(Thing & entity, GameContext & context)
{
    entity.m_active = !entity.m_active;
    entity.m_name = "fixture-state";
    entity.m_show = !entity.m_show;
    entity.m_selection_sub_index = 1;
    entity.m_debug_moving_platform = !entity.m_debug_moving_platform;
    entity.m_debug_start_tick_count = 0.75;
    entity.m_game_over = !entity.m_game_over;
    entity.m_game_win = !entity.m_game_win;
    entity.m_tick_count_game_over = 1.125;
    entity.m_tick_count_win = 1.25;
    entity.m_animation_state = "fixture-state";
    entity.m_animation_tick_count = 1.5;
    entity.m_animation_playing = !entity.m_animation_playing;
    entity.m_animation_sprite_sequence = 2;
    entity.m_animation_ended = !entity.m_animation_ended;
    entity.m_playing = !entity.m_playing;
    entity.m_sprite_offset_x = 2.125;
    entity.m_sprite_offset_y = 2.25;
    entity.m_accel_x = 2.375;
    entity.m_accel_y = 2.5;
    entity.m_speed_x = 2.625;
    entity.m_speed_y = 2.75;
    entity.m_grounded = 2.875;
    entity.m_collision = !entity.m_collision;
    entity.m_jumps = 1;
    entity.m_facing = 2;
    entity.m_inside_of_portal = 3;
    entity.m_climbing = 1;
    entity.m_should_start_climbing = 2;
    entity.m_should_stop_climbing = 3;
    entity.m_coyote_frame = 1;
    entity.m_was_climbing = 2;
    entity.m_attack_tick = 4.125;
    entity.m_next_attack = 4.25;
    entity.m_boss_avg_attack_delay = 4.375;
    entity.m_boss_attack_variance = 4.5;
    entity.m_tick_duration = 4.625;
    entity.m_shot_fired = !entity.m_shot_fired;
    entity.m_trail_index = 3;
    entity.m_should_rebound = 1;
    entity.m_no_masking = !entity.m_no_masking;
    entity.m_current_node = 3;
    entity.m_direction = 1;
    entity.m_loop = !entity.m_loop;
    entity.m_path_point_size = 2;
    entity.m_angle = 5.75;
    entity.m_laser_delay_attack = 5.875;
    entity.m_laser_delta = 6.0;
    entity.m_laser_delay_alarm = 6.125;
    entity.m_laser_proj_speed = 6.25;
    entity.m_laser_proj_fold = 6.375;
    entity.m_laser_proj_size = 6.5;
    entity.m_laser_proj_scale_y = 6.625;
    entity.m_laser_ondulating = !entity.m_laser_ondulating;
    entity.m_laser_ondulating_color = !entity.m_laser_ondulating_color;
    entity.m_cursor_idx = 2;
    entity.m_horiz_scaling = 7.125;
    entity.m_vert_scaling = 7.25;
    entity.x = 7.375;
    entity.y = 7.5;
    entity.xmin = 7.625;
    entity.xmax = 7.75;
    entity.ymin = 7.875;
    entity.ymax = 8.0;
    entity.m_scale = 8.125;
    entity.m_tightness = 8.25;
    entity.m_fps = 8.375;
    entity.m_device = 2;
    entity.m_base_scale_x = 8.625;
    entity.m_base_scale_y = 8.75;
    entity.m_intensity = 8.875;
    entity.m_device_on = !entity.m_device_on;
    entity.m_point_count = 1;
    entity.m_invert_x = !entity.m_invert_x;
    entity.m_show_boundary = !entity.m_show_boundary;
    entity.m_helios_index = 1;
    entity.m_cancel_projection = !entity.m_cancel_projection;
    entity.m_count = 9.75;
    entity.m_dead = !entity.m_dead;
    entity.m_intensity_modifier = 10.0;
    context.m_jump_max_height = 0.125;
    context.m_max_player_horizontal_speed = 0.25;
    context.m_half_jump_length = 0.375;
    context.m_climbing_speed = 0.5;
    context.m_climbing_jump_boost = 0.625;
    context.m_climbing_jump_division = 0.75;
    context.m_ground_accel = 0.875;
    context.m_ground_accel_max_ratio = 1.0;
    context.m_air_accel = 1.125;
    context.m_air_accel_max_ratio = 1.25;
    context.m_ground_friction = 1.375;
    context.m_ground_friction_max_ratio = 1.5;
    context.m_air_friction = 1.625;
    context.m_air_friction_max_ratio = 1.75;
    context.m_boss_avg_attack_delay = 1.875;
    context.m_boss_attack_variance = 2.0;
    context.m_boss_proj_speed = 2.125;
    context.m_boss_proj_gravity = 2.25;
    context.m_boss_proj_spring = 2.375;
    context.m_boss_proj_split_count = 2.5;
    context.m_laser_delay_attack = 2.625;
    context.m_base_player_scale = 2.75;
    context.m_coyote_frame_amount = 2.875;
    context.m_moving_platform_speed = 3.0;
    context.m_intensity = 3.125;
    context.m_scale = 3.25;
    context.m_fps = 3.375;
    context.m_pps = 3.5;
    context.m_unselected_intensity = 3.625;
    context.m_selected_intensity = 3.75;
    context.m_base_intensity_modifier = 3.875;
    context.m_cursor_speed = 4.0;
    context.m_draw_bounds = !context.m_draw_bounds;
    context.m_game_over_pos_x = 4.25;
    context.m_game_over_pos_y = 4.375;
    context.m_game_over_scale = 4.5;
    context.m_game_over_proj_index = 4.625;
    context.m_game_win_pos_x = 4.75;
    context.m_game_win_pos_y = 4.875;
    context.m_game_win_scale = 5.0;
    context.m_game_win_proj_index = 5.125;
    context.m_smoothing_step_number_long = 5.25;
    context.m_smoothing_step_number_short = 5.375;
    context.m_stop_on_end_point = 5.5;
}
void check_fields(const Thing & a,
                  const Thing & b,
                  const GameContext & ca,
                  const GameContext & cb)
{
    check(same(a.m_active, b.m_active), "round trip Thing.m_active");
    check(same(a.m_name, b.m_name), "round trip Thing.m_name");
    check(same(a.m_show, b.m_show), "round trip Thing.m_show");
    check(same(a.m_selection_sub_index, b.m_selection_sub_index),
          "round trip Thing.m_selection_sub_index");
    check(same(a.m_debug_moving_platform, b.m_debug_moving_platform),
          "round trip Thing.m_debug_moving_platform");
    check(same(a.m_debug_start_tick_count, b.m_debug_start_tick_count),
          "round trip Thing.m_debug_start_tick_count");
    check(same(a.m_game_over, b.m_game_over), "round trip Thing.m_game_over");
    check(same(a.m_game_win, b.m_game_win), "round trip Thing.m_game_win");
    check(same(a.m_tick_count_game_over, b.m_tick_count_game_over),
          "round trip Thing.m_tick_count_game_over");
    check(same(a.m_tick_count_win, b.m_tick_count_win),
          "round trip Thing.m_tick_count_win");
    check(same(a.m_animation_state, b.m_animation_state),
          "round trip Thing.m_animation_state");
    check(same(a.m_animation_tick_count, b.m_animation_tick_count),
          "round trip Thing.m_animation_tick_count");
    check(same(a.m_animation_playing, b.m_animation_playing),
          "round trip Thing.m_animation_playing");
    check(same(a.m_animation_sprite_sequence, b.m_animation_sprite_sequence),
          "round trip Thing.m_animation_sprite_sequence");
    check(same(a.m_animation_ended, b.m_animation_ended),
          "round trip Thing.m_animation_ended");
    check(same(a.m_playing, b.m_playing), "round trip Thing.m_playing");
    check(same(a.m_sprite_offset_x, b.m_sprite_offset_x),
          "round trip Thing.m_sprite_offset_x");
    check(same(a.m_sprite_offset_y, b.m_sprite_offset_y),
          "round trip Thing.m_sprite_offset_y");
    check(same(a.m_accel_x, b.m_accel_x), "round trip Thing.m_accel_x");
    check(same(a.m_accel_y, b.m_accel_y), "round trip Thing.m_accel_y");
    check(same(a.m_speed_x, b.m_speed_x), "round trip Thing.m_speed_x");
    check(same(a.m_speed_y, b.m_speed_y), "round trip Thing.m_speed_y");
    check(same(a.m_grounded, b.m_grounded), "round trip Thing.m_grounded");
    check(same(a.m_collision, b.m_collision), "round trip Thing.m_collision");
    check(same(a.m_jumps, b.m_jumps), "round trip Thing.m_jumps");
    check(same(a.m_facing, b.m_facing), "round trip Thing.m_facing");
    check(same(a.m_inside_of_portal, b.m_inside_of_portal),
          "round trip Thing.m_inside_of_portal");
    check(same(a.m_climbing, b.m_climbing), "round trip Thing.m_climbing");
    check(same(a.m_should_start_climbing, b.m_should_start_climbing),
          "round trip Thing.m_should_start_climbing");
    check(same(a.m_should_stop_climbing, b.m_should_stop_climbing),
          "round trip Thing.m_should_stop_climbing");
    check(same(a.m_coyote_frame, b.m_coyote_frame),
          "round trip Thing.m_coyote_frame");
    check(same(a.m_was_climbing, b.m_was_climbing),
          "round trip Thing.m_was_climbing");
    check(same(a.m_attack_tick, b.m_attack_tick),
          "round trip Thing.m_attack_tick");
    check(same(a.m_next_attack, b.m_next_attack),
          "round trip Thing.m_next_attack");
    check(same(a.m_boss_avg_attack_delay, b.m_boss_avg_attack_delay),
          "round trip Thing.m_boss_avg_attack_delay");
    check(same(a.m_boss_attack_variance, b.m_boss_attack_variance),
          "round trip Thing.m_boss_attack_variance");
    check(same(a.m_tick_duration, b.m_tick_duration),
          "round trip Thing.m_tick_duration");
    check(same(a.m_shot_fired, b.m_shot_fired),
          "round trip Thing.m_shot_fired");
    check(same(a.m_trail_index, b.m_trail_index),
          "round trip Thing.m_trail_index");
    check(same(a.m_should_rebound, b.m_should_rebound),
          "round trip Thing.m_should_rebound");
    check(same(a.m_no_masking, b.m_no_masking),
          "round trip Thing.m_no_masking");
    check(same(a.m_current_node, b.m_current_node),
          "round trip Thing.m_current_node");
    check(same(a.m_direction, b.m_direction), "round trip Thing.m_direction");
    check(same(a.m_loop, b.m_loop), "round trip Thing.m_loop");
    check(same(a.m_path_point_size, b.m_path_point_size),
          "round trip Thing.m_path_point_size");
    check(same(a.m_angle, b.m_angle), "round trip Thing.m_angle");
    check(same(a.m_laser_delay_attack, b.m_laser_delay_attack),
          "round trip Thing.m_laser_delay_attack");
    check(same(a.m_laser_delta, b.m_laser_delta),
          "round trip Thing.m_laser_delta");
    check(same(a.m_laser_delay_alarm, b.m_laser_delay_alarm),
          "round trip Thing.m_laser_delay_alarm");
    check(same(a.m_laser_proj_speed, b.m_laser_proj_speed),
          "round trip Thing.m_laser_proj_speed");
    check(same(a.m_laser_proj_fold, b.m_laser_proj_fold),
          "round trip Thing.m_laser_proj_fold");
    check(same(a.m_laser_proj_size, b.m_laser_proj_size),
          "round trip Thing.m_laser_proj_size");
    check(same(a.m_laser_proj_scale_y, b.m_laser_proj_scale_y),
          "round trip Thing.m_laser_proj_scale_y");
    check(same(a.m_laser_ondulating, b.m_laser_ondulating),
          "round trip Thing.m_laser_ondulating");
    check(same(a.m_laser_ondulating_color, b.m_laser_ondulating_color),
          "round trip Thing.m_laser_ondulating_color");
    check(same(a.m_cursor_idx, b.m_cursor_idx),
          "round trip Thing.m_cursor_idx");
    check(same(a.m_horiz_scaling, b.m_horiz_scaling),
          "round trip Thing.m_horiz_scaling");
    check(same(a.m_vert_scaling, b.m_vert_scaling),
          "round trip Thing.m_vert_scaling");
    check(same(a.x, b.x), "round trip Thing.x");
    check(same(a.y, b.y), "round trip Thing.y");
    check(same(a.xmin, b.xmin), "round trip Thing.xmin");
    check(same(a.xmax, b.xmax), "round trip Thing.xmax");
    check(same(a.ymin, b.ymin), "round trip Thing.ymin");
    check(same(a.ymax, b.ymax), "round trip Thing.ymax");
    check(same(a.m_scale, b.m_scale), "round trip Thing.m_scale");
    check(same(a.m_tightness, b.m_tightness), "round trip Thing.m_tightness");
    check(same(a.m_fps, b.m_fps), "round trip Thing.m_fps");
    check(same(a.m_device, b.m_device), "round trip Thing.m_device");
    check(same(a.m_base_scale_x, b.m_base_scale_x),
          "round trip Thing.m_base_scale_x");
    check(same(a.m_base_scale_y, b.m_base_scale_y),
          "round trip Thing.m_base_scale_y");
    check(same(a.m_intensity, b.m_intensity), "round trip Thing.m_intensity");
    check(same(a.m_device_on, b.m_device_on), "round trip Thing.m_device_on");
    check(same(a.m_point_count, b.m_point_count),
          "round trip Thing.m_point_count");
    check(same(a.m_invert_x, b.m_invert_x), "round trip Thing.m_invert_x");
    check(same(a.m_show_boundary, b.m_show_boundary),
          "round trip Thing.m_show_boundary");
    check(same(a.m_helios_index, b.m_helios_index),
          "round trip Thing.m_helios_index");
    check(same(a.m_cancel_projection, b.m_cancel_projection),
          "round trip Thing.m_cancel_projection");
    check(same(a.m_count, b.m_count), "round trip Thing.m_count");
    check(same(a.m_dead, b.m_dead), "round trip Thing.m_dead");
    check(same(a.m_intensity_modifier, b.m_intensity_modifier),
          "round trip Thing.m_intensity_modifier");
    check(same(ca.m_jump_max_height, cb.m_jump_max_height),
          "round trip GameContext.m_jump_max_height");
    check(same(ca.m_max_player_horizontal_speed,
               cb.m_max_player_horizontal_speed),
          "round trip GameContext.m_max_player_horizontal_speed");
    check(same(ca.m_half_jump_length, cb.m_half_jump_length),
          "round trip GameContext.m_half_jump_length");
    check(same(ca.m_climbing_speed, cb.m_climbing_speed),
          "round trip GameContext.m_climbing_speed");
    check(same(ca.m_climbing_jump_boost, cb.m_climbing_jump_boost),
          "round trip GameContext.m_climbing_jump_boost");
    check(same(ca.m_climbing_jump_division, cb.m_climbing_jump_division),
          "round trip GameContext.m_climbing_jump_division");
    check(same(ca.m_ground_accel, cb.m_ground_accel),
          "round trip GameContext.m_ground_accel");
    check(same(ca.m_ground_accel_max_ratio, cb.m_ground_accel_max_ratio),
          "round trip GameContext.m_ground_accel_max_ratio");
    check(same(ca.m_air_accel, cb.m_air_accel),
          "round trip GameContext.m_air_accel");
    check(same(ca.m_air_accel_max_ratio, cb.m_air_accel_max_ratio),
          "round trip GameContext.m_air_accel_max_ratio");
    check(same(ca.m_ground_friction, cb.m_ground_friction),
          "round trip GameContext.m_ground_friction");
    check(same(ca.m_ground_friction_max_ratio, cb.m_ground_friction_max_ratio),
          "round trip GameContext.m_ground_friction_max_ratio");
    check(same(ca.m_air_friction, cb.m_air_friction),
          "round trip GameContext.m_air_friction");
    check(same(ca.m_air_friction_max_ratio, cb.m_air_friction_max_ratio),
          "round trip GameContext.m_air_friction_max_ratio");
    check(same(ca.m_boss_avg_attack_delay, cb.m_boss_avg_attack_delay),
          "round trip GameContext.m_boss_avg_attack_delay");
    check(same(ca.m_boss_attack_variance, cb.m_boss_attack_variance),
          "round trip GameContext.m_boss_attack_variance");
    check(same(ca.m_boss_proj_speed, cb.m_boss_proj_speed),
          "round trip GameContext.m_boss_proj_speed");
    check(same(ca.m_boss_proj_gravity, cb.m_boss_proj_gravity),
          "round trip GameContext.m_boss_proj_gravity");
    check(same(ca.m_boss_proj_spring, cb.m_boss_proj_spring),
          "round trip GameContext.m_boss_proj_spring");
    check(same(ca.m_boss_proj_split_count, cb.m_boss_proj_split_count),
          "round trip GameContext.m_boss_proj_split_count");
    check(same(ca.m_laser_delay_attack, cb.m_laser_delay_attack),
          "round trip GameContext.m_laser_delay_attack");
    check(same(ca.m_base_player_scale, cb.m_base_player_scale),
          "round trip GameContext.m_base_player_scale");
    check(same(ca.m_coyote_frame_amount, cb.m_coyote_frame_amount),
          "round trip GameContext.m_coyote_frame_amount");
    check(same(ca.m_moving_platform_speed, cb.m_moving_platform_speed),
          "round trip GameContext.m_moving_platform_speed");
    check(same(ca.m_intensity, cb.m_intensity),
          "round trip GameContext.m_intensity");
    check(same(ca.m_scale, cb.m_scale), "round trip GameContext.m_scale");
    check(same(ca.m_fps, cb.m_fps), "round trip GameContext.m_fps");
    check(same(ca.m_pps, cb.m_pps), "round trip GameContext.m_pps");
    check(same(ca.m_unselected_intensity, cb.m_unselected_intensity),
          "round trip GameContext.m_unselected_intensity");
    check(same(ca.m_selected_intensity, cb.m_selected_intensity),
          "round trip GameContext.m_selected_intensity");
    check(same(ca.m_base_intensity_modifier, cb.m_base_intensity_modifier),
          "round trip GameContext.m_base_intensity_modifier");
    check(same(ca.m_cursor_speed, cb.m_cursor_speed),
          "round trip GameContext.m_cursor_speed");
    check(same(ca.m_draw_bounds, cb.m_draw_bounds),
          "round trip GameContext.m_draw_bounds");
    check(same(ca.m_game_over_pos_x, cb.m_game_over_pos_x),
          "round trip GameContext.m_game_over_pos_x");
    check(same(ca.m_game_over_pos_y, cb.m_game_over_pos_y),
          "round trip GameContext.m_game_over_pos_y");
    check(same(ca.m_game_over_scale, cb.m_game_over_scale),
          "round trip GameContext.m_game_over_scale");
    check(same(ca.m_game_over_proj_index, cb.m_game_over_proj_index),
          "round trip GameContext.m_game_over_proj_index");
    check(same(ca.m_game_win_pos_x, cb.m_game_win_pos_x),
          "round trip GameContext.m_game_win_pos_x");
    check(same(ca.m_game_win_pos_y, cb.m_game_win_pos_y),
          "round trip GameContext.m_game_win_pos_y");
    check(same(ca.m_game_win_scale, cb.m_game_win_scale),
          "round trip GameContext.m_game_win_scale");
    check(same(ca.m_game_win_proj_index, cb.m_game_win_proj_index),
          "round trip GameContext.m_game_win_proj_index");
    check(
        same(ca.m_smoothing_step_number_long, cb.m_smoothing_step_number_long),
        "round trip GameContext.m_smoothing_step_number_long");
    check(same(ca.m_smoothing_step_number_short,
               cb.m_smoothing_step_number_short),
          "round trip GameContext.m_smoothing_step_number_short");
    check(same(ca.m_stop_on_end_point, cb.m_stop_on_end_point),
          "round trip GameContext.m_stop_on_end_point");
}

void persistence(const std::filesystem::path & root)
{
    auto source = std::make_unique<Game>(root);
    const auto scene =
        get_current_scene(source->m_things, source->m_selected_device_idx)
            .m_idx;
    const auto idx = source->m_things.add(EntityType::decor);
    check(add_child(source->m_things, scene, idx),
          "serialization fixture attached");
    auto & a = source->m_things.get(idx);
    fill_fields(a, source->m_context);
    a.m_animation_path_map = {
        {"fixture",
         {{"two-frames.ild", true, 2.25}, {"second.ild", false, 1.5}}}};
    a.m_path_point = {{.1, .2}, {.4, .8}};
    a.m_cursors = {{{.1, .2}, {.7, .1}, {.8, .9}, {.2, .8}}};
    a.m_laser_proj_start_pos = {.21, .32};
    for (std::size_t i = 0; i < a.m_trail.size(); ++i)
        a.m_trail[i] = static_cast<double>(i) / 7;
    a.m_distortion_matrix = {{{1.1, .2, .3}, {.4, 1.5, .6}, {.01, .02, 1}}};
    a.m_scaling_matrix = {{{2, 0, 0}, {0, 3, 0}, {0, 0, 1}}};
    a.m_world_to_view_matrix =
        multiply_matrix(a.m_distortion_matrix, a.m_scaling_matrix);
    a.m_scene_stack = {SceneType::projection, SceneType::scaling,
                       SceneType::game};
    a.m_scene_type = SceneType::game_win;
    a.m_draw_list = {{.1, .2, .3, .4, .5}, {.6, .7, .8, .9, 1}};
    a.m_blanking_color = {.12, .23, .34};
    a.m_trigger_events = {TriggerActionType::shot, TriggerActionType::on_count};
    a.m_triggers = {{"",
                     TriggerType::activate,
                     {idx},
                     3,
                     TriggerActionType::on_count,
                     .75,
                     .5,
                     .25,
                     true}};
    a.m_moving_platform_idx = idx;
    a.m_portal_destination_idx = idx;
    a.m_proj_thing_idx = scene;
    a.m_scale_thing_idx = scene;
    a.new_parent_after_transaction = scene;
    source->m_context.m_center_bounds = {.37, .61};
    source->m_current_game = "fresh-test-world";
    const auto text = serialize_world(*source);
    auto restored = std::make_unique<Game>(root);
    const auto revision = restored->m_world_revision;
    check(deserialize_world(*restored, text), "deserialize generated world");
    check(restored->m_world_revision > revision,
          "successful load invalidates old tick snapshots");
    const auto & b = restored->m_things.get(idx);
    check_fields(a, b, source->m_context, restored->m_context);
#define SAME_FIELD(field) check(same(a.field, b.field), "round trip " #field)
    SAME_FIELD(m_idx);
    SAME_FIELD(m_kind);
    SAME_FIELD(parent_idx);
    SAME_FIELD(first_child_idx);
    SAME_FIELD(next_child_idx);
    SAME_FIELD(prev_child_idx);
    SAME_FIELD(new_parent_after_transaction);
    SAME_FIELD(m_moving_platform_idx);
    SAME_FIELD(m_portal_destination_idx);
    SAME_FIELD(m_proj_thing_idx);
    SAME_FIELD(m_scale_thing_idx);
    SAME_FIELD(m_animation_path_map);
    SAME_FIELD(m_path_point);
    SAME_FIELD(m_cursors);
    SAME_FIELD(m_laser_proj_start_pos);
    SAME_FIELD(m_trail);
    SAME_FIELD(m_distortion_matrix);
    SAME_FIELD(m_scaling_matrix);
    SAME_FIELD(m_world_to_view_matrix);
    SAME_FIELD(m_scene_stack);
    SAME_FIELD(m_scene_type);
    SAME_FIELD(m_draw_list);
    SAME_FIELD(m_blanking_color);
    SAME_FIELD(m_trigger_events);
    SAME_FIELD(m_triggers);
#undef SAME_FIELD
    check(same(source->m_context.m_center_bounds,
               restored->m_context.m_center_bounds),
          "round trip context bounds");
    check(b.m_things == &restored->m_things && restored->m_things.validate(),
          "load rebinds owners and validates hierarchy");
    const auto before = serialize_world(*restored);
    for (const auto * bad :
         {"{", "null", "[]", "{}", "{\"m_things\":{\"m_things\":[]}}"})
    {
        check(!deserialize_world(*restored, bad),
              "reject malformed or incomplete world");
        check(serialize_world(*restored) == before,
              "failed load is transactional");
    }
    const auto path = root / "saved_levels" / "roundtrip.cpl";
    check(save_level(*source, path),
          "save creates fresh level directory and file");
    check(load_level(*restored, path), "reload saved level");
    check_fields(a, restored->m_things.get(idx), source->m_context,
                 restored->m_context);
}
void blueprints(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    const auto scene =
        get_current_scene(game->m_things, game->m_selected_device_idx).m_idx;
    const auto original = game->m_things.add(EntityType::portal),
               child = game->m_things.add(EntityType::decor);
    check(add_child(game->m_things, scene, original)
              && add_child(game->m_things, original, child),
          "blueprint hierarchy setup");
    auto & portal = game->m_things.get(original);
    portal.m_portal_destination_idx = original;
    portal.m_triggers = {{"",
                          TriggerType::activate,
                          {original, child},
                          1,
                          TriggerActionType::shot,
                          1,
                          0,
                          0,
                          false}};
    game->m_things.get(child).m_portal_destination_idx = original;
    const auto path = root / "blueprints" / "new-blueprint.cpl";
    check(save_blueprint(*game, original, path), "save new blueprint");
    const auto imported = load_blueprint(*game, path, scene);
    check(imported && imported != original,
          "blueprint imports with distinct root index");
    const auto kids = game->m_things.children(imported);
    check(kids.size() == 1 && kids[0] != child,
          "blueprint copies subtree with fresh indices");
    const auto & clone = game->m_things.get(imported);
    check(clone.m_portal_destination_idx == imported,
          "self portal reference remaps to clone");
    check(game->m_things.get(kids[0]).m_portal_destination_idx == imported,
          "descendant reference remaps to copied parent");
    check(clone.m_triggers.size() == 1
              && clone.m_triggers[0].m_trigger_target
                     == std::vector<Thing_idx>{imported, kids[0]},
          "trigger targets remap within copied subtree");
    check(game->m_things.validate(),
          "blueprint insertion preserves circular hierarchy");
}

void write_ilda(const std::filesystem::path & path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    auto header = [&](unsigned count, unsigned frame) {
        std::array<unsigned char, 32> bytes{};
        bytes[0] = 'I';
        bytes[1] = 'L';
        bytes[2] = 'D';
        bytes[3] = 'A';
        bytes[7] = 5;
        bytes[24] = static_cast<unsigned char>(count >> 8);
        bytes[25] = static_cast<unsigned char>(count);
        bytes[26] = static_cast<unsigned char>(frame >> 8);
        bytes[27] = static_cast<unsigned char>(frame);
        bytes[29] = 2;
        out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    };
    header(2, 0);
    const unsigned char first[]{0x40, 0, 0x20, 0, 0,    0x20, 0x40, 0x80,
                                0xC0, 0, 0xE0, 0, 0xC0, 0xFF, 0xFF, 0xFF};
    out.write(reinterpret_cast<const char *>(first), sizeof(first));
    header(1, 1);
    const unsigned char second[]{0, 0, 0, 0, 0x80, 0, 0, 0xFF};
    out.write(reinterpret_cast<const char *>(second), sizeof(second));
    header(0, 0);
    check(static_cast<bool>(out), "write generated ILDA fixture");
}
void render_and_animation(const std::filesystem::path & root)
{
    write_ilda(root / "animations" / "two-frames.ild");
    const auto frames = load_ilda(root / "animations" / "two-frames.ild");
    check(frames.size() == 2 && frames[0].size() == 2 && frames[1].size() == 1,
          "ILDA format 5 frame and point counts");
    check(close(frames[0][0].x, .5) && close(frames[0][0].y, -.25),
          "ILDA signed coordinates and Y flip");
    check(frames[0][0].r > frames[0][0].g && frames[0][0].g > frames[0][0].b,
          "ILDA BGR ordering");
    check(close(frames[0][1].x, -.5) && close(frames[0][1].y, .25),
          "ILDA negative coordinate sign extension");
    check(frames[0][1].r == 0 && frames[0][1].g == 0 && frames[0][1].b == 0,
          "ILDA blanking bit suppresses color");
    const std::array<Vec2, 4> corners{{{.1, .2}, {.9, .1}, {.8, .9}, {.2, .8}}};
    const std::array<Vec2, 4> square{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    const auto matrix = compute_projection(corners);
    for (std::size_t i = 0; i < 4; ++i)
        check(same(apply_projection_matrix(matrix, square[i]), corners[i]),
              "homography maps known corner " + std::to_string(i));
    check(same(multiply_matrix(matrix, g_identity_matrix), matrix),
          "matrix composition preserves identity");
    auto game = std::make_unique<Game>(root);
    auto idx = game->m_things.add(EntityType::decor);
    auto & entity = game->m_things.get(idx);
    add_animation_to_thing(*game, entity, "loop", "two-frames.ild", 2, true);
    change_animation_state(entity, "loop");
    game->m_delta_time = 1.0 / 60;
    check(same(get_current_frame(*game, entity), frames[0]),
          "animation begins at first frame");
    advance_animation(*game, entity, 2);
    check(same(get_current_frame(*game, entity), frames[1]),
          "animation advances by ticks per frame");
    advance_animation(*game, entity, 2);
    check(same(get_current_frame(*game, entity), frames[0])
              && !entity.m_animation_ended,
          "loop wraps without finishing");
    add_animation_to_thing(*game, entity, "once", "two-frames.ild", 1, false);
    change_animation_state(entity, "once");
    advance_animation(*game, entity, 5);
    check(same(get_current_frame(*game, entity), frames[1])
              && entity.m_animation_ended,
          "nonloop animation clamps and signals end");
}

Thing_idx empty_game_scene(Game & game)
{
    switch_game_to_scene(game, SceneType::game);
    const auto scene =
        get_current_scene(game.m_things, game.m_selected_device_idx).m_idx;
    for (auto child : game.m_things.children(scene))
        remove_hierarchy(game.m_things, child);
    game.m_editor_mode = false;
    game.m_delta_time = .01;
    return scene;
}
void drawing_bounds_and_masks(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    const auto scene = empty_game_scene(*game);
    auto & platform = game->m_things.get(
        make_entity(*game, EntityType::platform, scene, .2, .5));
    platform.m_scale = 2;
    auto & mask = game->m_things.get(
        make_entity(*game, EntityType::masking_box, scene, .4, .45));
    mask.xmax = .1;
    mask.ymax = .05;
    mask.m_scale = 2;
    mask.m_active = false;
    auto visible = [](const DacPoint & p) { return p.r + p.g + p.b > 0; };
    build_draw_lists(*game);
    const auto & points = game->m_draw_lists[game->m_selected_device_idx];
    check(std::any_of(
              points.begin(), points.end(),
              [&](const auto & p) { return visible(p) && close(p.x, .6); }),
          "scaled platform drawing reaches collision boundary");
    check(std::any_of(points.begin(), points.end(),
                      [&](const auto & p) {
                          return visible(p) && p.x > .45 && p.x < .55;
                      }),
          "inactive mask does not blank platform");
    mask.m_active = true;
    build_draw_lists(*game);
    check(std::none_of(points.begin(), points.end(),
                       [&](const auto & p) {
                           return visible(p) && p.x > .40001 && p.x < .59999;
                       }),
          "active mask uses scaled collision bounds for blanking");
}
void outside_drawing_detours(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    const auto scene = empty_game_scene(*game);
    auto & device = game->m_things.get(game->m_selected_device_idx);
    device.m_tightness = 4;
    auto & first = game->m_things.get(
        make_entity(*game, EntityType::platform, scene, .1, .2));
    first.xmin = first.ymin = 0;
    first.xmax = .1;
    first.m_scale = 1;
    auto & outside = game->m_things.get(
        make_entity(*game, EntityType::platform, scene, 1.2, .7));
    outside.xmin = outside.ymin = 0;
    outside.xmax = .3;
    outside.m_scale = 1;
    auto & last = game->m_things.get(
        make_entity(*game, EntityType::platform, scene, .7, .8));
    last.xmin = last.ymin = 0;
    last.xmax = .1;
    last.m_scale = 1;
    auto draw = [&] {
        build_draw_lists(*game);
        return game->m_draw_lists.at(device.m_idx);
    };
    auto lit = [](const DacPoint & p) {
        return p.r != 0 || p.g != 0 || p.b != 0;
    };
    outside.m_show = last.m_show = false;
    const auto first_only = draw();
    first.m_show = false;
    last.m_show = true;
    const auto last_only = draw();
    const auto first_lit =
        std::count_if(first_only.begin(), first_only.end(), lit);
    const auto last_lit =
        std::count_if(last_only.begin(), last_only.end(), lit);
    first.m_show = outside.m_show = true;
    auto points = draw();
    check(points.size() == first_only.size() + 2 + last_lit,
          "outside sampled stroke and its blank travel leave only retained "
          "strokes and blank connection anchors");
    check(std::all_of(points.begin(), points.end(),
                      [](const auto & p) { return p.x >= .1 && p.x <= .8; }),
          "outside sampled stroke and dwells leave no outside or border "
          "placeholders");
    auto next = std::find_if(points.begin(), points.end(), [&](const auto & p) {
        return lit(p) && close(p.y, .8);
    });
    check(next != points.end() && next != points.begin()
              && !lit(*std::prev(next)) && close(std::prev(next)->x, .7)
              && close(std::prev(next)->y, .8),
          "next retained stroke starts at its own blank anchor");
    check(
        std::count_if(points.begin(), points.end(),
                      [&](const auto & p) { return lit(p) && close(p.y, .2); })
                == first_lit
            && std::count_if(
                   points.begin(), points.end(),
                   [&](const auto & p) { return lit(p) && close(p.y, .8); })
                   == last_lit,
        "adjacent retained strokes keep all lit samples and dwells");
    last.m_show = false;
    points = draw();
    check(points.size() == first_only.size() + 1
              && same(points.back(), DacPoint{.2, .2, 0, 0, 0}),
          "removed final stroke preserves blank shutoff at the last retained "
          "endpoint");
    first.m_show = false;
    last.m_show = true;
    points = draw();
    check(points.size() == 1 + std::size_t(last_lit)
              && same(points.front(), DacPoint{.7, .8, 0, 0, 0}),
          "removed leading stroke reconnects at the retained stroke's blank "
          "start");
    first.m_show = last.m_show = false;
    points = draw();
    check(points.size() == 1 && same(points[0], DacPoint{.5, .5, 0, 0, 0}),
          "fully rejected frame uses black center fallback");
    outside.m_show = false;
    auto & sprite =
        game->m_things.get(make_entity(*game, EntityType::decor, scene, 0, 0));
    sprite.xmin = sprite.xmax = sprite.ymin = sprite.ymax = 0;
    sprite.m_scale = 1;
    game->m_frame_cache.m_frame_cache["clipping-fixture"] = {Frame{}};
    add_animation_to_thing(*game, sprite, "test", "clipping-fixture", 1, true);
    auto frame = [&](Frame input) {
        game->m_frame_cache.m_frame_cache["clipping-fixture"] = {
            std::move(input)};
        return draw();
    };
    points = frame({{.2, .2, 0, 0, 0},
                    {.3, .2, 1, 0, 0},
                    {1.2, .4, 0, 0, 0},
                    {1.4, .6, 0, 0, 0},
                    {.7, .8, 0, 0, 0},
                    {.8, .8, 0, 1, 0}});
    check(std::none_of(points.begin(), points.end(),
                       [](const auto & p) { return p.x > .8; }),
          "wholly blank outside detour leaves no outside or edge points");
    next = std::find_if(points.begin(), points.end(),
                        [](const auto & p) { return p.g == 1; });
    check(next != points.end() && next != points.begin()
              && same(*std::prev(next), DacPoint{.7, .8, 0, 0, 0}),
          "blank detour retains its on-screen destination anchor");
    points = frame({{-1, .5, 0, 0, 0}, {2, .5, 1, 0, 0}});
    check(std::any_of(points.begin(), points.end(),
                      [&](const auto & p) {
                          return lit(p) && close(p.x, 0) && close(p.y, .5);
                      })
              && std::any_of(points.begin(), points.end(),
                             [&](const auto & p) {
                                 return lit(p) && close(p.x, 1)
                                        && close(p.y, .5);
                             }),
          "outside-to-outside crossing retains its visible screen span");
    points = frame({{.2, .4, 0, 0, 0},
                    {.4, .4, 1, 0, 0},
                    {1.2, .4, 1, 0, 0},
                    {1.4, .6, 0, 1, 0},
                    {.6, .6, 0, 0, 1}});
    check(std::any_of(points.begin(), points.end(),
                      [](const auto & p) {
                          return p.r == 1 && close(p.x, 1) && close(p.y, .4);
                      })
              && std::any_of(points.begin(), points.end(),
                             [](const auto & p) {
                                 return p.b == 1 && close(p.x, 1)
                                        && close(p.y, .6);
                             })
              && std::any_of(points.begin(), points.end(),
                             [](const auto & p) {
                                 return p.b == 1 && close(p.x, .6)
                                        && close(p.y, .6);
                             }),
          "outside portion of a partly visible stroke preserves adjacent lit "
          "exits and entries");
    points = frame({{-1, 1, 0, 0, 0}, {1, -1, 1, 0, 0}});
    check(points.size() == 1 && same(points[0], DacPoint{.5, .5, 0, 0, 0}),
          "corner-only tangency has no visible stroke to retain");
    points = frame({{-.5, 0, 0, 0, 0}, {1.5, 0, 1, 0, 0}});
    check(std::any_of(points.begin(), points.end(),
                      [&](const auto & p) {
                          return lit(p) && close(p.x, 1) && close(p.y, 0);
                      }),
          "border-aligned crossing retains its visible length");
    points = frame({{0, .4, 0, 0, 0}, {0, .4, 1, 0, 0}, {0, .4, 1, 0, 0}});
    check(std::count_if(points.begin(), points.end(), lit) == 2,
          "on-screen border dwells remain visible");
    points = frame({{1.2, .4, 0, 0, 0}, {1.4, .6, 0, 0, 0}});
    check(points.size() == 1 && same(points[0], DacPoint{.5, .5, 0, 0, 0}),
          "fully outside blank frame uses black center fallback");
    points = frame({{.2, .2, 0, 0, 0},
                    {.3, .2, 1, 0, 0},
                    {.3, .2, 0, 0, 0},
                    {.3, .2, 0, 0, 0},
                    {1.2, .4, 0, 0, 0},
                    {1.4, .6, 1, 0, 0}});
    check(points.size() >= 3
              && same(points[points.size() - 3], DacPoint{.3, .2, 1, 0, 0})
              && same(points[points.size() - 2], DacPoint{.3, .2, 0, 0, 0})
              && same(points.back(), DacPoint{.3, .2, 0, 0, 0}),
          "outside stroke removal preserves original blank shutoff dwells");
    points = frame({{.2, .2, 0, 0, 0},
                    {.3, .2, 1, 0, 0},
                    {1.2, .4, 0, 0, 0},
                    {1.4, .6, 1, 0, 0}});
    check(same(points.back(), DacPoint{.3, .2, 0, 0, 0}),
          "direct outside blank jump still leaves a blank shutoff after "
          "removing the final stroke");
    points = frame({{-1, .5, 0, 0, 0}, {.5, .5, 1, 0, 0}, {2, .5, 0, 0, 0}});
    check(same(points.back(), DacPoint{.5, .5, 0, 0, 0}),
          "removed outside blank-only tail leaves blank shutoff at the "
          "retained endpoint");
    points = frame({{-1, .5, 0, 0, 0},
                    {.5, .5, 1, 0, 0},
                    {.5, .5, 0, 0, 0},
                    {.5, .5, 0, 0, 0},
                    {2, .5, 0, 0, 0}});
    check(points.size() >= 3
              && same(points[points.size() - 3], DacPoint{.5, .5, 1, 0, 0})
              && same(points[points.size() - 2], DacPoint{.5, .5, 0, 0, 0})
              && same(points.back(), DacPoint{.5, .5, 0, 0, 0}),
          "outside blank-only tail removal preserves stationary blank shutoff "
          "dwells");
    const Frame retained{{.2, .2, 0, 0, 0},
                         {.3, .2, 1, 0, 0},
                         {.7, .8, 0, 0, 0},
                         {.8, .8, 0, 1, 0}};
    const auto baseline = frame(retained);
    auto & mask = game->m_things.get(
        make_entity(*game, EntityType::masking_box, scene, .1, .1));
    mask.xmin = mask.ymin = 0;
    mask.xmax = .8;
    mask.ymax = .8;
    mask.m_scale = 1;
    points = draw();
    check(points.size() == baseline.size()
              && std::none_of(points.begin(), points.end(), lit),
          "mask still blanks retained geometry without deleting points");
}
void gameplay(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    const auto scene = empty_game_scene(*game);
    const auto player_idx =
        spawn_player(*game, scene, .3, 1 - .001 - 1.5 * .02);
    auto & player = game->m_things.get(player_idx);
    player.m_grounded = 1;
    CommandState jump;
    jump.jump = {true, true, false};
    update_gameplay(*game, game->m_things.get(scene), jump);
    check(player.m_speed_y < 0 && player.m_jumps == 1 && !player.m_grounded,
          "grounded press starts upward jump");
    const auto first_y = player.y;
    update_gameplay(*game, game->m_things.get(scene), CommandState{});
    update_gameplay(*game, game->m_things.get(scene), jump);
    check(player.m_speed_y < 0 && player.m_jumps == 2 && player.y < first_y,
          "second press permits airborne double jump");
    update_gameplay(*game, game->m_things.get(scene), jump);
    check(player.m_jumps == 2, "third jump is rejected");
    game->m_things.remove(player_idx);
    const auto platform_idx =
        make_entity(*game, EntityType::move_platform, scene, .2, .5);
    auto & platform = game->m_things.get(platform_idx);
    platform.m_path_point = {{.2, .5}, {.4, .5}};
    update_gameplay(*game, game->m_things.get(scene), CommandState{});
    check(close(platform.x, .2007) && close(platform.m_speed_x, .07),
          "moving platform follows configured speed");
    platform.x = .3998;
    platform.m_current_node = 0;
    for (int tick = 0; tick < 3; ++tick)
        update_gameplay(*game, game->m_things.get(scene), CommandState{});
    check(platform.x < .4 && platform.x > .39 && platform.m_speed_x < 0,
          "nonloop path reverses at final node");
    const auto target = make_entity(*game, EntityType::decor, scene, .8, .8);
    game->m_things.get(target).m_active = false;
    const auto source = make_entity(*game, EntityType::trigger, scene, .1, .1);
    auto & trigger = game->m_things.get(source);
    trigger.m_count = 2;
    trigger.m_triggers = {{"",
                           TriggerType::activate,
                           {target},
                           2,
                           TriggerActionType::on_count,
                           1,
                           .03,
                           0,
                           false}};
    update_gameplay(*game, game->m_things.get(scene), CommandState{});
    check(!game->m_things.get(target).m_active,
          "delayed count trigger does not fire immediately");
    for (int tick = 0; tick < 4; ++tick)
        update_gameplay(*game, game->m_things.get(scene), CommandState{});
    check(game->m_things.get(target).m_active
              && !trigger.m_triggers[0].m_triggered,
          "delayed count trigger activates target then clears pending flag");
    const auto jump_platform =
        make_entity(*game, EntityType::platform, scene, .6, .5);
    const auto jumping = spawn_player(*game, scene, .65, .5 - 1.5 * .02);
    game->m_things.get(jumping).m_grounded = 1;
    game->m_delta_time = 1. / 150;
    update_gameplay(*game, game->m_things.get(scene), jump);
    check(
        game->m_things.get(jumping).m_speed_y < 0
            && !game->m_things.get(jumping).m_grounded
            && game->m_things.get(jumping).m_jumps == 1,
        "default 150Hz jump leaves platform without resting-contact recapture");
    (void) jump_platform;
}
void portals_and_death(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    const auto scene = empty_game_scene(*game);
    const auto second_device = add_device(*game, -1);
    switch_device_to_scene(*game, game->m_things.get(second_device),
                           SceneType::game);
    const auto other = get_current_scene(game->m_things, second_device).m_idx;
    const auto origin = make_entity(*game, EntityType::portal, scene, .3, .3);
    const auto destination =
        make_entity(*game, EntityType::portal, other, .7, .4);
    game->m_things.get(origin).m_portal_destination_idx = destination;
    game->m_things.get(destination).m_portal_destination_idx = origin;
    const auto idx = spawn_player(*game, scene, .31, .31);
    auto & player = game->m_things.get(idx);
    update_gameplay(*game, game->m_things.get(scene), CommandState{});
    check(player.new_parent_after_transaction == other
              && player.parent_idx == scene,
          "portal defers scene mutation until transaction boundary");
    check(close(player.x, .7) && close(player.y, .4),
          "portal places player at destination");
    game->apply_deferred_reparents();
    check(
        player.parent_idx == other && player.new_parent_after_transaction == 0,
        "portal transaction reparents player across devices");
    update_gameplay(*game, game->m_things.get(other), CommandState{});
    check(
        player.parent_idx == other && player.new_parent_after_transaction == 0,
        "destination latch prevents immediate portal bounce");
    game->m_things.remove(destination);
    const auto hazard = make_entity(*game, EntityType::boss_projectile, other,
                                    player.x, player.y);
    auto & projectile = game->m_things.get(hazard);
    projectile.xmin = projectile.ymin = -.1;
    projectile.xmax = projectile.ymax = .1;
    projectile.m_scale = 1;
    const auto effect_count = game->m_sound_events.size();
    update_gameplay(*game, game->m_things.get(other), CommandState{});
    check(player.m_dead && player.m_animation_state == "dead",
          "projectile contact kills player and selects death animation");
    check(game->m_sound_events.size() > effect_count,
          "death emits a sound event");
    check(game->m_things.validate(),
          "portal and death leave entity hierarchy valid");
}
void fresh_workflow(const std::filesystem::path & root)
{
    auto game = std::make_unique<Game>(root);
    auto type = [&] {
        return get_current_scene(game->m_things, game->m_selected_device_idx)
            .m_scene_type;
    };
    check(type() == SceneType::projection,
          "fresh session begins in projection calibration");
    Input confirm;
    confirm.confirm = true;
    game->update(confirm, 1.0 / 60);
    check(type() == SceneType::scaling,
          "projection confirmation advances to scaling");
    game->update(Input{}, 1.0 / 60);
    game->update(confirm, 1.0 / 60);
    check(type() == SceneType::game,
          "scaling confirmation creates game scenes");
    int players = 0, spawners = 0;
    for (auto & entity : game->m_things)
    {
        players += entity.m_kind == EntityType::player;
        spawners += entity.m_kind == EntityType::spawner;
    }
    check(players == 1 && spawners == 1,
          "fresh scene has player and spawn marker");
    check(game->m_editor_mode && game->m_things.validate(),
          "new game opens as valid editable world");
    game->start_game(false);
    check(!game->m_editor_mode && type() == SceneType::game,
          "start enters gameplay without legacy level dependency");
    check(!game->m_draw_lists[game->m_selected_device_idx].empty(),
          "calibration/game pipeline produces DAC draw points");
}
} // namespace

int main()
{
    Scratch scratch;
    const std::pair<const char *, std::function<void()>> tests[]{
        {"pool capacity, circular hierarchy and owner copies",
         pool_and_hierarchy},
        {"input press, hold and release edges", input_edges},
        {"complete persistence and transactional rejection",
         [&] { persistence(scratch.path / "persistence"); }},
        {"blueprint subtree and self-reference remapping",
         [&] { blueprints(scratch.path / "blueprints"); }},
        {"ILDA decoding, homography and animation loops",
         [&] { render_and_animation(scratch.path / "render"); }},
        {"scaled drawing and active masking",
         [&] { drawing_bounds_and_masks(scratch.path / "masks"); }},
        {"outside drawing detours and blank reconnection",
         [&] { outside_drawing_detours(scratch.path / "clipping"); }},
        {"jump, moving platforms and delayed triggers",
         [&] { gameplay(scratch.path / "gameplay"); }},
        {"cross-device portals and projectile death",
         [&] { portals_and_death(scratch.path / "portals"); }},
        {"fresh calibration through gameplay",
         [&] { fresh_workflow(scratch.path / "fresh"); }}};
    int failures = 0;
    for (const auto & [name, test] : tests)
    {
        try
        {
            test();
            std::cout << "PASS " << name << '\n';
        }
        catch (const std::exception & error)
        {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << std::size(tests) - failures << '/' << std::size(tests)
              << " checks passed\n";
    return failures ? 1 : 0;
}
