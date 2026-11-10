#include "chienpo/persistence.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unistd.h>

namespace chienpo {
namespace {
// These visitors are the .cpl schema. m_things back-pointers are reconstructed.
#define FIELD(name) field(#name, value.name)
template <class V, class F>
void visit_Thing(V & value, F field)
{
    FIELD(m_idx);
    FIELD(m_kind);
    FIELD(m_next_free);
    FIELD(m_active);
    FIELD(m_name);
    FIELD(m_show);
    FIELD(m_selection_sub_index);
    FIELD(m_debug_moving_platform);
    FIELD(m_debug_start_tick_count);
    FIELD(m_game_over);
    FIELD(m_game_win);
    FIELD(m_tick_count_game_over);
    FIELD(m_tick_count_win);
    FIELD(m_animation_state);
    FIELD(m_animation_tick_count);
    FIELD(m_animation_path_map);
    FIELD(m_animation_playing);
    FIELD(m_animation_sprite_sequence);
    FIELD(m_animation_ended);
    FIELD(m_playing);
    FIELD(m_sprite_offset_x);
    FIELD(m_sprite_offset_y);
    FIELD(m_accel_x);
    FIELD(m_accel_y);
    FIELD(m_speed_x);
    FIELD(m_speed_y);
    FIELD(m_grounded);
    FIELD(m_collision);
    FIELD(m_jumps);
    FIELD(m_facing);
    FIELD(m_moving_platform_idx);
    FIELD(m_inside_of_portal);
    FIELD(m_climbing);
    FIELD(m_should_start_climbing);
    FIELD(m_should_stop_climbing);
    FIELD(m_coyote_frame);
    FIELD(m_was_climbing);
    FIELD(m_attack_tick);
    FIELD(m_next_attack);
    FIELD(m_boss_avg_attack_delay);
    FIELD(m_boss_attack_variance);
    FIELD(m_tick_duration);
    FIELD(m_shot_fired);
    FIELD(m_trail_index);
    FIELD(m_should_rebound);
    FIELD(m_trail);
    FIELD(m_no_masking);
    FIELD(m_current_node);
    FIELD(m_direction);
    FIELD(m_loop);
    FIELD(m_path_point);
    FIELD(m_path_point_size);
    FIELD(m_portal_destination_idx);
    FIELD(m_angle);
    FIELD(m_laser_delay_attack);
    FIELD(m_laser_delta);
    FIELD(m_laser_delay_alarm);
    FIELD(m_laser_proj_speed);
    FIELD(m_laser_proj_fold);
    FIELD(m_laser_proj_size);
    FIELD(m_laser_proj_start_pos);
    FIELD(m_laser_proj_scale_y);
    FIELD(m_laser_ondulating);
    FIELD(m_laser_ondulating_color);
    FIELD(m_cursors);
    FIELD(m_cursor_idx);
    FIELD(m_horiz_scaling);
    FIELD(m_vert_scaling);
    FIELD(x);
    FIELD(y);
    FIELD(xmin);
    FIELD(xmax);
    FIELD(ymin);
    FIELD(ymax);
    FIELD(m_scale);
    FIELD(parent_idx);
    FIELD(first_child_idx);
    FIELD(next_child_idx);
    FIELD(prev_child_idx);
    FIELD(new_parent_after_transaction);
    FIELD(m_scene_type);
    FIELD(m_proj_thing_idx);
    FIELD(m_scale_thing_idx);
    FIELD(m_tightness);
    FIELD(m_fps);
    FIELD(m_device);
    FIELD(m_scene_stack);
    FIELD(m_distortion_matrix);
    FIELD(m_scaling_matrix);
    FIELD(m_world_to_view_matrix);
    FIELD(m_base_scale_x);
    FIELD(m_base_scale_y);
    FIELD(m_draw_list);
    FIELD(m_intensity);
    FIELD(m_device_on);
    FIELD(m_point_count);
    FIELD(m_invert_x);
    FIELD(m_show_boundary);
    FIELD(m_helios_index);
    FIELD(m_cancel_projection);
    FIELD(m_blanking_color);
    FIELD(m_triggers);
    FIELD(m_count);
    FIELD(m_trigger_events);
    FIELD(m_dead);
    FIELD(m_intensity_modifier);
}
template <class V, class F>
void visit_GameContext(V & value, F field)
{
    FIELD(m_jump_max_height);
    FIELD(m_max_player_horizontal_speed);
    FIELD(m_half_jump_length);
    FIELD(m_climbing_speed);
    FIELD(m_climbing_jump_boost);
    FIELD(m_climbing_jump_division);
    FIELD(m_ground_accel);
    FIELD(m_ground_accel_max_ratio);
    FIELD(m_air_accel);
    FIELD(m_air_accel_max_ratio);
    FIELD(m_ground_friction);
    FIELD(m_ground_friction_max_ratio);
    FIELD(m_air_friction);
    FIELD(m_air_friction_max_ratio);
    FIELD(m_boss_avg_attack_delay);
    FIELD(m_boss_attack_variance);
    FIELD(m_boss_proj_speed);
    FIELD(m_boss_proj_gravity);
    FIELD(m_boss_proj_spring);
    FIELD(m_boss_proj_split_count);
    FIELD(m_laser_delay_attack);
    FIELD(m_center_bounds);
    FIELD(m_base_player_scale);
    FIELD(m_coyote_frame_amount);
    FIELD(m_moving_platform_speed);
    FIELD(m_intensity);
    FIELD(m_scale);
    FIELD(m_fps);
    FIELD(m_pps);
    FIELD(m_unselected_intensity);
    FIELD(m_selected_intensity);
    FIELD(m_base_intensity_modifier);
    FIELD(m_cursor_speed);
    FIELD(m_draw_bounds);
    FIELD(m_game_over_pos_x);
    FIELD(m_game_over_pos_y);
    FIELD(m_game_over_scale);
    FIELD(m_game_over_proj_index);
    FIELD(m_game_win_pos_x);
    FIELD(m_game_win_pos_y);
    FIELD(m_game_win_scale);
    FIELD(m_game_win_proj_index);
    FIELD(m_smoothing_step_number_long);
    FIELD(m_smoothing_step_number_short);
    FIELD(m_stop_on_end_point);
}
template <class F>
void visit(Thing & value, F field)
{
    visit_Thing(value, field);
}
template <class F>
void visit(const Thing & value, F field)
{
    visit_Thing(value, field);
}
template <class F>
void visit(GameContext & value, F field)
{
    visit_GameContext(value, field);
}
template <class F>
void visit(const GameContext & value, F field)
{
    visit_GameContext(value, field);
}
#define VISITOR(Type, BODY)                                                    \
    template <class F>                                                         \
    void visit(Type & value, F field)                                          \
    {                                                                          \
        BODY;                                                                  \
    }                                                                          \
    template <class F>                                                         \
    void visit(const Type & value, F field)                                    \
    {                                                                          \
        BODY;                                                                  \
    }
VISITOR(Vec2, FIELD(x); FIELD(y))
VISITOR(Color, FIELD(r); FIELD(g); FIELD(b))
VISITOR(DacPoint, FIELD(x); FIELD(y); FIELD(r); FIELD(g); FIELD(b))
VISITOR(Animation, FIELD(path); FIELD(m_loop); FIELD(m_tick_per_frame))
VISITOR(Trigger, FIELD(m_trigger_type); FIELD(m_trigger_target);
        FIELD(m_trigger_count_target);
        FIELD(m_trigger_action);
        FIELD(m_trigger_percentage);
        FIELD(m_trigger_delay);
        FIELD(m_trigger_elapsed);
        FIELD(m_triggered))
#undef VISITOR
#undef FIELD

using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json owned(cJSON * value)
{
    if (!value) throw std::runtime_error("JSON allocation failed");
    return Json(value, cJSON_Delete);
}
[[noreturn]] void invalid(const std::string & message)
{
    throw std::runtime_error(message);
}
template <class T>
struct Sequence : std::false_type
{};
template <class T, class A>
struct Sequence<std::vector<T, A>> : std::true_type
{};
template <class T, std::size_t N>
struct Sequence<std::array<T, N>> : std::true_type
{};
template <class T>
struct Fixed : std::false_type
{};
template <class T, std::size_t N>
struct Fixed<std::array<T, N>> : std::true_type
{};

template <class T>
Json encode(const T & value)
{
    if constexpr (std::is_same_v<T, bool>)
        return owned(cJSON_CreateBool(value));
    else if constexpr (std::is_arithmetic_v<T>)
    {
        if (!std::isfinite(static_cast<double>(value)))
            invalid("Cannot save nonfinite number");
        return owned(cJSON_CreateNumber(static_cast<double>(value)));
    }
    else if constexpr (std::is_same_v<T, std::string>)
        return owned(cJSON_CreateString(value.c_str()));
    else if constexpr (std::is_enum_v<T>)
        return owned(cJSON_CreateString(std::string(to_string(value)).c_str()));
    else if constexpr (Sequence<T>::value)
    {
        auto json = owned(cJSON_CreateArray());
        for (const auto & item : value)
        {
            auto child = encode(item);
            if (!cJSON_AddItemToArray(json.get(), child.get()))
                throw std::bad_alloc();
            child.release();
        }
        return json;
    }
    else if constexpr (std::is_same_v<T, AnimationPathMap>)
    {
        auto json = owned(cJSON_CreateObject());
        for (const auto & pair : value)
        {
            auto child = encode(pair.second);
            if (!cJSON_AddItemToObject(json.get(), pair.first.c_str(),
                                       child.get()))
                throw std::bad_alloc();
            child.release();
        }
        return json;
    }
    else
    {
        auto json = owned(cJSON_CreateObject());
        visit(value, [&](const char * name, const auto & item) {
            auto child = encode(item);
            if (!cJSON_AddItemToObject(json.get(), name, child.get()))
                throw std::bad_alloc();
            child.release();
        });
        return json;
    }
}
template <class T>
void decode(const cJSON * json, T & value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if (!cJSON_IsBool(json)) invalid("Expected boolean");
        value = cJSON_IsTrue(json);
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        if (!cJSON_IsNumber(json) || !std::isfinite(json->valuedouble))
            invalid("Expected finite number");
        const double n = json->valuedouble;
        if constexpr (std::is_integral_v<T>)
        {
            if (std::trunc(n) != n
                || static_cast<long double>(n)
                       < std::numeric_limits<T>::lowest()
                || static_cast<long double>(n) > std::numeric_limits<T>::max())
                invalid("Integer out of range");
        }
        value = static_cast<T>(n);
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        if (!cJSON_IsString(json)) invalid("Expected string");
        value = json->valuestring;
    }
    else if constexpr (std::is_enum_v<T>)
    {
        // The supplied early standalone blueprints used numeric zero for the
        // nil scene.
        if constexpr (std::is_same_v<T, SceneType>)
        {
            if (cJSON_IsNumber(json) && json->valuedouble == 0)
            {
                value = SceneType::nil_scene;
                return;
            }
        }
        if (!cJSON_IsString(json) || !from_string(json->valuestring, value))
            invalid("Unknown enum value");
    }
    else if constexpr (Sequence<T>::value)
    {
        if (!cJSON_IsArray(json)) invalid("Expected array");
        const auto count = static_cast<std::size_t>(cJSON_GetArraySize(json));
        if constexpr (Fixed<T>::value)
        {
            if (count != value.size()) invalid("Incorrect fixed array length");
        }
        else
        {
            if (count > 1000000) invalid("Array too large");
            value.resize(count);
        }
        std::size_t i = 0;
        for (auto * child = json->child; child; child = child->next)
            decode(child, value[i++]);
    }
    else if constexpr (std::is_same_v<T, AnimationPathMap>)
    {
        if (!cJSON_IsObject(json)) invalid("Expected animation map");
        value.clear();
        for (auto * child = json->child; child; child = child->next)
            decode(child, value[child->string]);
    }
    else
    {
        if (!cJSON_IsObject(json)) invalid("Expected object");
        visit(value, [&](const char * name, auto & item) {
            if (auto * child = cJSON_GetObjectItemCaseSensitive(json, name))
            {
                try
                {
                    decode(child, item);
                }
                catch (const std::exception & e)
                {
                    invalid(std::string(name) + ": " + e.what());
                }
            }
        });
    }
}
template <class T>
void put(cJSON * json, const char * name, const T & value)
{
    auto child = encode(value);
    if (!cJSON_AddItemToObject(json, name, child.get())) throw std::bad_alloc();
    child.release();
}
template <class T>
void required(const cJSON * json, const char * name, T & value)
{
    const auto * child = cJSON_GetObjectItemCaseSensitive(json, name);
    if (!child) invalid(std::string("Missing ") + name);
    decode(child, value);
}
void check_keys(const cJSON * json, int depth = 0)
{
    if (depth > 64) invalid("JSON nesting too deep");
    std::set<std::string> keys;
    for (auto * child = json->child; child; child = child->next)
    {
        if (cJSON_IsObject(json)
            && (!child->string || !keys.insert(child->string).second))
            invalid("Duplicate JSON key");
        check_keys(child, depth + 1);
    }
}
Json parse(const std::string & text)
{
    if (text.size() > 32 * 1024 * 1024) invalid("File exceeds 32 MiB");
    const char * end = nullptr;
    auto * parsed =
        cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, &end, true);
    if (!parsed) invalid("Invalid JSON");
    auto json = owned(parsed);
    if (!cJSON_IsObject(json.get())) invalid("Expected JSON root object");
    check_keys(json.get());
    return json;
}
std::string dump(const cJSON * json)
{
    std::unique_ptr<char, decltype(&cJSON_free)> text(
        cJSON_PrintUnformatted(json), cJSON_free);
    if (!text) throw std::bad_alloc();
    return text.get();
}
std::string read_file(const std::filesystem::path & path)
{
    if (std::filesystem::file_size(path) > 32 * 1024 * 1024)
        invalid("File exceeds 32 MiB");
    std::ifstream file(path, std::ios::binary);
    if (!file) invalid("Cannot open " + path.string());
    std::string result((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    if (file.bad()) invalid("Cannot read " + path.string());
    return result;
}
void atomic_write(const std::filesystem::path & path, const std::string & text)
{
    if (path.empty() || path.filename().empty())
        invalid("Empty output filename");
    const auto parent = path.has_parent_path() ? path.parent_path()
                                               : std::filesystem::path(".");
    std::filesystem::create_directories(parent);
    auto pattern =
        (parent / (path.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    const int fd = mkstemp(name.data());
    if (fd < 0)
        invalid("Cannot create temporary save: "
                + std::string(std::strerror(errno)));
    bool closed = false;
    try
    {
        std::size_t offset = 0;
        while (offset < text.size())
        {
            const auto n =
                ::write(fd, text.data() + offset, text.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) invalid("Cannot write save file");
            offset += static_cast<std::size_t>(n);
        }
        if (::fsync(fd) != 0) invalid("Cannot flush save file");
        const int result = ::close(fd);
        closed = true;
        if (result != 0) invalid("Cannot close save file");
        std::filesystem::rename(name.data(), path);
    }
    catch (...)
    {
        if (!closed) ::close(fd);
        ::unlink(name.data());
        throw;
    }
}
bool live(const Things & pool, Thing_idx idx)
{
    return idx > 0 && idx < MAX_THINGS && pool.m_used[idx]
           && ent_valid(pool.m_things[idx]);
}
void validate_references(const Things & pool)
{
    std::string why;
    if (!pool.validate(&why)) invalid(why);
    for (Thing_idx i = 1; i < pool.m_next_empty_slot; ++i)
        if (pool.m_used[i])
        {
            const auto & t = pool.m_things[i];
            for (auto idx :
                 {t.m_moving_platform_idx, t.m_portal_destination_idx,
                  t.m_proj_thing_idx, t.m_scale_thing_idx,
                  t.new_parent_after_transaction})
                if (idx && !live(pool, idx))
                    invalid("Dangling entity reference");
            for (const auto & tr : t.m_triggers)
                for (auto idx : tr.m_trigger_target)
                    if (idx && !live(pool, idx))
                        invalid("Dangling trigger target");
            if (t.m_path_point_size < 0
                || static_cast<std::size_t>(t.m_path_point_size)
                       > t.m_path_point.size())
                invalid("Invalid path point count");
        }
}
void remap_references(Thing & t, const ThingRemap & map)
{
    const auto remap = [&](Thing_idx & idx) {
        const auto it = map.find(idx);
        idx = it == map.end() ? 0 : it->second;
    };
    for (auto * idx : {&t.m_moving_platform_idx, &t.m_portal_destination_idx,
                       &t.m_proj_thing_idx, &t.m_scale_thing_idx,
                       &t.new_parent_after_transaction})
        remap(*idx);
    for (auto & tr : t.m_triggers)
    {
        for (auto & idx : tr.m_trigger_target)
            remap(idx);
        tr.m_trigger_target.erase(std::remove(tr.m_trigger_target.begin(),
                                              tr.m_trigger_target.end(), 0),
                                  tr.m_trigger_target.end());
    }
}
void catalog(const std::filesystem::path & directory,
             std::vector<std::string> & list,
             bool levels = false)
{
    list.clear();
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error), end;
    if (error) return;
    for (; iterator != end; iterator.increment(error))
    {
        if (error) break;
        const auto & entry = *iterator;
        if (entry.is_regular_file(error)
            && (!levels || entry.path().extension() == ".cpl"))
            list.push_back(entry.path().filename().string());
    }
    std::sort(list.begin(), list.end());
}
} // namespace

std::string serialize_world(const Game & game)
{
    validate_references(game.m_things);
    auto world = owned(cJSON_CreateObject()),
         pool = owned(cJSON_CreateObject());
    put(pool.get(), "m_things", game.m_things.m_things);
    put(pool.get(), "m_used", game.m_things.m_used);
    put(pool.get(), "m_next_empty_slot", game.m_things.m_next_empty_slot);
    put(pool.get(), "m_first_free", game.m_things.m_first_free);
    if (!cJSON_AddItemToObject(world.get(), "m_things", pool.get()))
        throw std::bad_alloc();
    pool.release();
    put(world.get(), "m_device_parent_idx", game.m_device_parent_idx);
    put(world.get(), "m_devices", game.m_devices);
    put(world.get(), "m_entity_selected_index", game.m_entity_selected_index);
    put(world.get(), "m_selected_device_idx", game.m_selected_device_idx);
    // Native settings extend the existing world object, without changing its
    // pool schema.
    put(world.get(), "m_context", game.m_context);
    return dump(world.get());
}
bool deserialize_world(Game & game, const std::string & text)
{
    try
    {
        auto json = parse(text);
        auto pool = std::make_unique<Things>();
        const auto * source =
            cJSON_GetObjectItemCaseSensitive(json.get(), "m_things");
        if (!cJSON_IsObject(source)) invalid("Missing thing pool");
        required(source, "m_next_empty_slot", pool->m_next_empty_slot);
        required(source, "m_first_free", pool->m_first_free);
        required(source, "m_things", pool->m_things);
        required(source, "m_used", pool->m_used);
        pool->rebind();
        validate_references(*pool);
        Thing_idx parent = 0, selected = 0, entity = 0;
        std::vector<Thing_idx> devices;
        required(json.get(), "m_device_parent_idx", parent);
        required(json.get(), "m_devices", devices);
        required(json.get(), "m_selected_device_idx", selected);
        required(json.get(), "m_entity_selected_index", entity);
        if (parent
            && (!live(*pool, parent)
                || pool->get(parent).m_kind != EntityType::device_parent))
            invalid("Invalid device parent");
        std::set<Thing_idx> unique;
        for (auto idx : devices)
            if (!live(*pool, idx) || pool->get(idx).m_kind != EntityType::device
                || !unique.insert(idx).second
                || pool->get(idx).parent_idx != parent)
                invalid("Invalid device list");
        for (Thing_idx i = 1; i < pool->m_next_empty_slot; ++i)
            if (pool->m_used[i]
                && pool->m_things[i].m_kind == EntityType::device
                && !unique.count(i))
                invalid("Device missing from list");
        if ((selected && !unique.count(selected))
            || (entity && !live(*pool, entity)))
            invalid("Invalid selection");
        GameContext context = game.m_context;
        if (auto * settings =
                cJSON_GetObjectItemCaseSensitive(json.get(), "m_context"))
            decode(settings, context);
        // Commit only after the complete candidate world passes validation.
        std::vector<CommandState> commands(
            std::max<std::size_t>(1, devices.size()));
        game.m_things.m_things.swap(pool->m_things);
        game.m_things.m_used.swap(pool->m_used);
        game.m_things.m_first_free = pool->m_first_free;
        game.m_things.m_next_empty_slot = pool->m_next_empty_slot;
        game.m_things.rebind();
        game.m_devices = std::move(devices);
        game.m_device_parent_idx = parent;
        game.m_selected_device_idx = selected;
        game.m_entity_selected_index = entity;
        game.m_context = context;
        game.m_editor_mode = true;
        game.m_edit = true;
        game.m_draw_lists.clear();
        game.m_frame_cache.m_frame_cache.clear();
        game.m_command_state_list = std::move(commands);
        ++game.m_world_revision;
        return true;
    }
    catch (const std::exception & e)
    {
        game.send_error(std::string("Cannot load level: ") + e.what());
        return false;
    }
}
bool save_level(Game & game, const std::filesystem::path & path)
{
    try
    {
        atomic_write(path, serialize_world(game));
        game.m_current_game = path.stem().string();
        refresh_resources(game);
        return true;
    }
    catch (const std::exception & e)
    {
        game.send_error(std::string("Cannot save level: ") + e.what());
        return false;
    }
}
bool load_level(Game & game, const std::filesystem::path & path)
{
    try
    {
        if (!deserialize_world(game, read_file(path))) return false;
        game.m_current_game = path.stem().string();
        return true;
    }
    catch (const std::exception & e)
    {
        game.send_error(std::string("Cannot load level: ") + e.what());
        return false;
    }
}
void refresh_resources(Game & game)
{
    catalog(game.m_paths.levels, game.m_level_list, true);
    catalog(game.m_paths.blueprints, game.m_blueprints);
    catalog(game.m_paths.animations, game.m_animation_list);
    catalog(game.m_paths.music, game.m_sound_list);
}
bool save_blueprint(Game & game,
                    Thing_idx entity,
                    const std::filesystem::path & path)
{
    try
    {
        if (!live(game.m_things, entity)) invalid("No entity selected");
        validate_references(game.m_things);
        std::vector<Thing_idx> order{entity};
        ThingRemap included{{entity, entity}};
        for (std::size_t n = 0; n < order.size(); ++n)
            for (auto child : game.m_things.children(order[n]))
                if (included.emplace(child, child).second)
                    order.push_back(child);
        Thing root = game.m_things.get(entity);
        root.parent_idx = root.next_child_idx = root.prev_child_idx = 0;
        root.m_next_free = 0;
        remap_references(root, included);
        auto json = encode(root);
        std::vector<Thing> children;
        for (std::size_t n = 1; n < order.size(); ++n)
        {
            children.push_back(game.m_things.get(order[n]));
            remap_references(children.back(), included);
        }
        // Standalone Thing stays compatible; native subtree export adds this
        // optional list.
        put(json.get(), "m_blueprint_children", children);
        atomic_write(path, dump(json.get()));
        refresh_resources(game);
        return true;
    }
    catch (const std::exception & e)
    {
        game.send_error(std::string("Cannot save blueprint: ") + e.what());
        return false;
    }
}
Thing_idx load_blueprint(Game & game,
                         const std::filesystem::path & path,
                         Thing_idx parent)
{
    try
    {
        auto json = parse(read_file(path));
        Thing root;
        decode(json.get(), root);
        std::vector<Thing> records{root};
        if (auto * children = cJSON_GetObjectItemCaseSensitive(
                json.get(), "m_blueprint_children"))
        {
            std::vector<Thing> extra;
            decode(children, extra);
            records.insert(records.end(), extra.begin(), extra.end());
        }
        else records[0].first_child_idx = 0;
        if (records.size() >= MAX_THINGS) invalid("Blueprint too large");
        if (!parent)
            parent =
                get_current_scene(game.m_things, game.m_selected_device_idx)
                    .m_idx;
        if (!live(game.m_things, parent))
            invalid("No scene selected for blueprint");
        auto source = std::make_unique<Things>();
        ThingRemap identities;
        for (auto & t : records)
        {
            if (t.m_idx == 0 || t.m_idx >= MAX_THINGS
                || t.m_kind == EntityType::nil || t.m_kind == EntityType::device
                || t.m_kind == EntityType::device_parent
                || !identities.emplace(t.m_idx, t.m_idx).second)
                invalid("Invalid blueprint entity");
            source->m_things[t.m_idx] = t;
            source->m_used[t.m_idx] = true;
            source->m_next_empty_slot =
                std::max(source->m_next_empty_slot, t.m_idx + 1);
        }
        auto & source_root = source->m_things[root.m_idx];
        source_root.parent_idx = source_root.next_child_idx =
            source_root.prev_child_idx = 0;
        for (Thing_idx idx = 1; idx < source->m_next_empty_slot; ++idx)
        {
            if (!source->m_used[idx])
            {
                source->m_things[idx].m_next_free = source->m_first_free;
                source->m_first_free = idx;
            }
            else remap_references(source->m_things[idx], identities);
        }
        source->rebind();
        validate_references(*source);
        std::vector<Thing_idx> order{root.m_idx};
        for (std::size_t n = 0; n < order.size(); ++n)
            for (auto child : source->children(order[n]))
                order.push_back(child);
        if (order.size() != records.size())
            invalid("Disconnected blueprint hierarchy");
        auto candidate = std::make_unique<Things>(game.m_things);
        ThingRemap remap;
        for (auto old : order)
        {
            const auto idx = candidate->add_thing(source->get(old));
            if (!idx) invalid("Thing pool is full");
            remap[old] = idx;
        }
        for (auto old : order)
        {
            auto & t = candidate->get(remap.at(old));
            const auto destination =
                old == root.m_idx ? parent
                                  : remap.at(source->get(old).parent_idx);
            if (!add_child(*candidate, destination, t.m_idx))
                invalid("Cannot attach blueprint");
            t.new_parent_after_transaction =
                source->get(old).new_parent_after_transaction;
            remap_references(t, remap);
        }
        validate_references(*candidate);
        game.m_things.m_things.swap(candidate->m_things);
        game.m_things.m_used.swap(candidate->m_used);
        game.m_things.m_first_free = candidate->m_first_free;
        game.m_things.m_next_empty_slot = candidate->m_next_empty_slot;
        game.m_things.rebind();
        game.m_entity_selected_index = remap.at(root.m_idx);
        ++game.m_world_revision;
        return game.m_entity_selected_index;
    }
    catch (const std::exception & e)
    {
        game.send_error(std::string("Cannot load blueprint: ") + e.what());
        return 0;
    }
}
} // namespace chienpo
