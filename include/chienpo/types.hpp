#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace chienpo {
using Thing_idx = std::uint32_t;
using Sprite_idx = Thing_idx;
using Frame_idx = Thing_idx;
using Point_idx = Thing_idx;
inline constexpr std::size_t MAX_THINGS = 1024;
struct Vec2
{
    double x = 0, y = 0;
};
struct Box
{
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
};
struct Size
{
    double width = 0, height = 0;
};
struct Color
{
    double r = 0, g = 0, b = 0;
};
using ColorArray = std::array<double, 3>;
struct DacPoint
{
    double x = 0, y = 0, r = 0, g = 0, b = 0;
};
using TransformMatrix = std::array<std::array<double, 3>, 3>;
inline constexpr TransformMatrix g_identity_matrix{
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
inline constexpr std::array<Vec2, 4> base_cursor_list{
    {{.4, .4}, {.6, .4}, {.6, .6}, {.4, .6}}};
using ThingRemap = std::map<Thing_idx, Thing_idx>;
using SpriteRemap = ThingRemap;
using FrameRemap = ThingRemap;
using PointRemap = ThingRemap;
enum class EntityType
{
    nil,
    frame,
    sprite,
    point,
    player,
    neon_boss,
    platform,
    move_platform,
    masking_box,
    laser,
    laser_projectile,
    boss_projectile,
    projection_bound,
    scaling_bound,
    decor,
    spawner,
    portal,
    scene_root_node,
    device,
    device_parent,
    trigger
};
std::string_view to_string(EntityType value);
bool from_string(std::string_view text, EntityType & value);
enum class TriggerActionType : uint8_t
{
    death,
    shot,
    activated,
    deactivated,
    on_count,
    on_start
};
std::string_view to_string(TriggerActionType value);
bool from_string(std::string_view text, TriggerActionType & value);
enum class TriggerType : uint8_t
{
    no_trigger,
    win,
    activate,
    deactivate,
    add_to_count,
    remove_from_count,
    lose,
    start_game
};
std::string_view to_string(TriggerType value);
bool from_string(std::string_view text, TriggerType & value);
enum class SceneType : uint8_t
{
    nil_scene,
    projection,
    scaling,
    game,
    editor,
    game_over,
    game_win
};
std::string_view to_string(SceneType value);
bool from_string(std::string_view text, SceneType & value);
SceneType string_to_scene_type(std::string_view value);
std::string_view scene_type_to_string(SceneType value);
struct Animation
{
    std::string path;
    bool m_loop = false;
    double m_tick_per_frame = 1;
};
using AnimationPathMap = std::map<std::string, std::vector<Animation>>;
using Frame = std::vector<DacPoint>;
using Frames = std::vector<Frame>;
struct FrameCache
{
    std::map<std::string, Frames> m_frame_cache;
};
struct Trigger
{
    std::string m_name;
    TriggerType m_trigger_type = TriggerType::no_trigger;
    std::vector<Thing_idx> m_trigger_target;
    double m_trigger_count_target = 0;
    TriggerActionType m_trigger_action = TriggerActionType::death;
    double m_trigger_percentage = 1, m_trigger_delay = 0, m_trigger_elapsed = 0;
    bool m_triggered = false;
};
struct GameContext
{
    double m_jump_max_height = 0.2;
    double m_max_player_horizontal_speed = 0.2;
    double m_half_jump_length = 0.18;
    double m_climbing_speed = 0.1;
    double m_climbing_jump_boost = 1.05;
    double m_climbing_jump_division = 6;
    double m_ground_accel = 0.01;
    double m_ground_accel_max_ratio = 4.0;
    double m_air_accel = 1.0;
    double m_air_accel_max_ratio = 2.0;
    double m_ground_friction = 4.0;
    double m_ground_friction_max_ratio = 10.0;
    double m_air_friction = 1.0;
    double m_air_friction_max_ratio = 5.0;
    double m_boss_avg_attack_delay = 4;
    double m_boss_attack_variance = 0.5;
    double m_boss_proj_speed = 0.1;
    double m_boss_proj_gravity = 0.1;
    double m_boss_proj_spring = 0.2;
    double m_boss_proj_split_count = 3;
    double m_laser_delay_attack = 2;
    Vec2 m_center_bounds{.5, .5};
    double m_base_player_scale = 0.02;
    double m_coyote_frame_amount = 20;
    double m_moving_platform_speed = 0.07;
    double m_intensity = 1.0;
    double m_scale = 1;
    double m_fps = 150;
    double m_pps = 35000;
    double m_unselected_intensity = 0.5;
    double m_selected_intensity = 1;
    double m_base_intensity_modifier = 1;
    double m_cursor_speed = 0.3;
    bool m_draw_bounds = false;
    double m_game_over_pos_x = 0.5;
    double m_game_over_pos_y = 0.5;
    double m_game_over_scale = 0.2;
    double m_game_over_proj_index = 0;
    double m_game_win_pos_x = 0.5;
    double m_game_win_pos_y = 0.5;
    double m_game_win_scale = 0.2;
    double m_game_win_proj_index = 0;
    double m_smoothing_step_number_long = 24;
    double m_smoothing_step_number_short = 6;
    double m_stop_on_end_point = 3;
};
struct ContextFieldInfo
{
    const char * name;
    bool zero_allowed = true;
    const char * type = "number";
};
inline constexpr std::array<ContextFieldInfo, 2> game_context_info{
    {{"m_max_player_horizontal_speed", false, "number"},
     {"m_center_bounds", true, "vec2"}}};
struct Things;
struct Thing
{
    Things * m_things = nullptr;
    Thing_idx m_idx = 0;
    EntityType m_kind = EntityType::nil;
    Thing_idx m_next_free = 0;
    bool m_active = true;
    std::string m_name = "";
    bool m_show = true;
    int m_selection_sub_index = 0;
    bool m_debug_moving_platform = false;
    double m_debug_start_tick_count = 0;
    bool m_game_over = false;
    bool m_game_win = false;
    double m_tick_count_game_over = 0;
    double m_tick_count_win = 0;
    std::string m_animation_state = "";
    double m_animation_tick_count = 0;
    AnimationPathMap m_animation_path_map;
    bool m_animation_playing = false;
    int m_animation_sprite_sequence = 0;
    bool m_animation_ended = false;
    bool m_playing = false;
    double m_sprite_offset_x = 0;
    double m_sprite_offset_y = 0;
    double m_accel_x = 0;
    double m_accel_y = 0;
    double m_speed_x = 0;
    double m_speed_y = 0;
    double m_grounded = 0;
    bool m_collision = true;
    int m_jumps = 0;
    int m_facing = 0;
    Thing_idx m_moving_platform_idx = 0;
    int m_inside_of_portal = 0;
    int m_climbing = 0;
    int m_should_start_climbing = 0;
    int m_should_stop_climbing = 0;
    int m_coyote_frame = 0;
    int m_was_climbing = 0;
    double m_attack_tick = 0;
    double m_next_attack = 0;
    double m_boss_avg_attack_delay = 0;
    double m_boss_attack_variance = 0;
    double m_tick_duration = 0;
    bool m_shot_fired = false;
    int m_trail_index = 0;
    int m_should_rebound = 0;
    std::array<double, 20> m_trail{};
    bool m_no_masking = false;
    int m_current_node = 0;
    int m_direction = 0;
    bool m_loop = false;
    std::vector<Vec2> m_path_point;
    int m_path_point_size = 0;
    Thing_idx m_portal_destination_idx = 0;
    double m_angle = 0;
    double m_laser_delay_attack = 0;
    double m_laser_delta = 0;
    double m_laser_delay_alarm = 0;
    double m_laser_proj_speed = 0.1;
    double m_laser_proj_fold = 10;
    double m_laser_proj_size = 0.03;
    Vec2 m_laser_proj_start_pos{};
    double m_laser_proj_scale_y = 1;
    bool m_laser_ondulating = false;
    bool m_laser_ondulating_color = false;
    std::array<Vec2, 4> m_cursors{};
    int m_cursor_idx = 0;
    double m_horiz_scaling = 0;
    double m_vert_scaling = 0;
    double x = 0;
    double y = 0;
    double xmin = 0;
    double xmax = 0;
    double ymin = 0;
    double ymax = 0;
    double m_scale = 1;
    Thing_idx parent_idx = 0;
    Thing_idx first_child_idx = 0;
    Thing_idx next_child_idx = 0;
    Thing_idx prev_child_idx = 0;
    Thing_idx new_parent_after_transaction = 0;
    SceneType m_scene_type = SceneType::nil_scene;
    Thing_idx m_proj_thing_idx = 0;
    Thing_idx m_scale_thing_idx = 0;
    double m_tightness = 4;
    double m_fps = 60;
    int m_device = 0;
    std::vector<SceneType> m_scene_stack;
    TransformMatrix m_distortion_matrix = g_identity_matrix;
    TransformMatrix m_scaling_matrix = g_identity_matrix;
    TransformMatrix m_world_to_view_matrix = g_identity_matrix;
    double m_base_scale_x = 0;
    double m_base_scale_y = 0;
    std::vector<DacPoint> m_draw_list;
    double m_intensity = 0;
    bool m_device_on = false;
    int m_point_count = 0;
    bool m_invert_x = false;
    bool m_show_boundary = false;
    int m_helios_index = -1;
    bool m_cancel_projection = true;
    Color m_blanking_color{};
    std::vector<Trigger> m_triggers;
    double m_count = 0;
    std::vector<TriggerActionType> m_trigger_events;
    bool m_dead = false;
    double m_intensity_modifier = 1;
    Thing & parent();
    Thing & first_child();
    Thing & next_child(bool active = true);
    Thing & prev_child();
    Thing & last_child();
    Thing_idx last_child_idx() const;
};
struct Things
{
    std::array<Thing, MAX_THINGS> m_things{};
    std::array<bool, MAX_THINGS> m_used{};
    Thing_idx m_next_empty_slot = 1, m_first_free = 0;
    Things();
    Things(const Things & other);
    Things & operator=(const Things & other);
    void reset();
    void rebind();
    Thing_idx add(EntityType kind);
    Thing_idx add_thing(const Thing & thing);
    void remove(Thing_idx idx);
    Thing & get(Thing_idx idx);
    const Thing & get(Thing_idx idx) const;
    std::vector<Thing_idx> children(Thing_idx parent,
                                    bool active_only = false) const;
    bool validate(std::string * error = nullptr) const;
    struct Iterator
    {
        Things * pool;
        Thing_idx index;
        void skip();
        Thing & operator*() const { return pool->m_things[index]; }
        Iterator & operator++()
        {
            ++index;
            skip();
            return *this;
        }
        bool operator!=(const Iterator & other) const
        {
            return index != other.index;
        }
    };
    Iterator begin();
    Iterator end();
};
bool ent_valid(const Thing & thing);
bool add_child(Things & things, Thing_idx parent, Thing_idx child);
void remove_child(Things & things, Thing_idx child);
void remove_hierarchy(Things & things, Thing_idx root);
Thing &
get_scene_from_device(Things & things, const Thing & device, SceneType type);
Thing & get_current_scene(Things & things, Thing_idx device);
bool set_device_to_scene(Things & things, Thing_idx device, SceneType type);
void reset_thing_frame_lifetime_value(const GameContext & context,
                                      Thing & thing);
void pop_box_in_thing(Thing & thing, const Box & box);
void pop_pos_in_thing(Thing & thing, const Vec2 & position);
struct Command
{
    bool pressed = false, positive_edge = false, negative_edge = false;
};
struct Input
{
    bool next_entity = false;
    bool prev_entity = false;
    bool next = false;
    bool prev = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool confirm = false;
    bool shift = false;
    bool cancel = false;
    bool delete_key = false;
    bool jump = false;
    bool add_platform = false;
    bool add_moving_platform = false;
    bool add_masking_box = false;
    bool add_neon_boss = false;
    bool add_portal = false;
    bool add_decor = false;
    bool add_laser = false;
    bool add_trigger_thing = false;
    bool switch_mode = false;
    bool scale_up = false;
    bool scale_down = false;
    bool reset_scale = false;
    bool loop_platform = false;
};
struct CommandState
{
    Command next_entity;
    Command prev_entity;
    Command next;
    Command prev;
    Command up;
    Command down;
    Command left;
    Command right;
    Command confirm;
    Command shift;
    Command cancel;
    Command delete_key;
    Command jump;
    Command add_platform;
    Command add_moving_platform;
    Command add_masking_box;
    Command add_neon_boss;
    Command add_portal;
    Command add_decor;
    Command add_laser;
    Command add_trigger_thing;
    Command switch_mode;
    Command scale_up;
    Command scale_down;
    Command reset_scale;
    Command loop_platform;
};
void transform_command_state(CommandState & state, const Input & input);
void remove_command_edges(CommandState & state);
} // namespace chienpo
