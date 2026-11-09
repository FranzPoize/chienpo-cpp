#include "chienpo/game.hpp"

#include "chienpo/gameplay.hpp"
#include "chienpo/persistence.hpp"
#include "chienpo/render.hpp"
#include "chienpo/types.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace chienpo {
namespace {

std::filesystem::path current_level_path(const Game & game)
{
    auto name = std::filesystem::path(game.m_current_game).filename();
    if (name.extension() != ".cpl") name += ".cpl";
    return game.m_paths.levels / name;
}

inline Thing &
create_scene(Game & a_game, SceneType a_type, Thing_idx & a_parent_idx)
{
    Thing_idx scene_idx = a_game.m_things.add(EntityType::scene_root_node);
    if (!scene_idx)
    {
        a_game.send_error("Entity pool exhausted creating scenes");
        return a_game.m_things.get(0);
    }
    Thing & scene = a_game.m_things.get(scene_idx);
    scene.m_name = to_string(a_type);
    scene.m_scene_type = a_type;
    scene.m_debug_start_tick_count = static_cast<double>(a_game.m_tick_count);

    add_child(a_game.m_things, a_parent_idx, scene_idx);

    return scene;
}

} // namespace

Game::Game(std::filesystem::path resources, int device_count)
{
    m_paths.root = std::move(resources);
    m_paths.levels = m_paths.root / "saved_levels";
    m_paths.blueprints = m_paths.root / "blueprints";
    m_paths.animations = m_paths.root / "animations";
    m_paths.music = m_paths.root / "music";
    reset_game(device_count);
}

void Game::reset_game(int device_count)
{
    if (device_count >= 0) m_helios_count = device_count;
    stop_sound();
    m_things.reset();
    m_devices.clear();
    m_draw_lists.clear();
    m_selected_device_idx = m_entity_selected_index = 0;
    m_device_parent_idx = m_things.add(EntityType::device_parent);
    m_editor_mode = true;
    m_never_played = true;
    m_autosave_enabled = false;
    m_autosave_elapsed = 0;
    m_command_state_list.assign(1, CommandState{});
    for (int i = 0; i < std::max(1, m_helios_count); ++i)
        add_device(*this, m_helios_count ? i : -1);
    m_selected_device_idx = m_devices.front();
    for (auto idx : m_devices)
        create_projection_for_device(*this, idx);
    ++m_world_revision;
}

void Game::update(const Input & input, double delta)
{
    update(std::vector<Input>{input}, delta);
}

void Game::update(const std::vector<Input> & inputs, double delta)
{
    // Ignore invalid wall-clock values; cap long pauses to avoid physics
    // tunnelling.
    m_delta_time = std::isfinite(delta) ? std::clamp(delta, 0.0, .1) : 0;
    ++m_tick_count;
    if (m_command_state_list.size() < std::max<std::size_t>(1, inputs.size()))
        m_command_state_list.resize(std::max<std::size_t>(1, inputs.size()));
    for (std::size_t i = 0; i < m_command_state_list.size(); ++i)
        transform_command_state(m_command_state_list[i],
                                i < inputs.size() ? inputs[i] : Input{});
    const auto commands = m_command_state_list.front();
    for (auto & thing : m_things)
        reset_thing_frame_lifetime_value(m_context, thing);
    const auto devices = m_devices;
    for (auto idx : devices)
    {
        auto & scene = get_current_scene(m_things, idx);
        if (!ent_valid(scene)) continue;
        const auto revision = m_world_revision;
        switch (scene.m_scene_type)
        {
        case SceneType::projection:
        case SceneType::scaling:
            if (idx == m_selected_device_idx)
                update_calibration(*this, scene, commands);
            break;
        case SceneType::game:
        case SceneType::game_win:
        case SceneType::game_over:
            if (m_editor_mode)
            {
                if (idx == m_selected_device_idx)
                    update_editor(*this, scene, commands);
            }
            else update_gameplay(*this, scene, commands);
            break;
        default:
            break;
        }
        // Loading a level invalidates this tick's snapshot of scene indices.
        if (revision != m_world_revision) break;
    }
    if (commands.switch_mode.positive_edge) switch_mode();
    apply_deferred_reparents();
    build_draw_lists(*this);
    if (m_editor_mode && m_autosave_enabled)
    {
        m_autosave_elapsed += m_delta_time;
        if (m_autosave_elapsed >= 10)
        {
            m_autosave_elapsed = 0;
            auto name = m_current_game;
            if (name.find("autosave") == std::string::npos) name += "_autosave";
            const auto editing_name = m_current_game;
            save_level(*this, m_paths.levels / (name + ".cpl"));
            m_current_game = editing_name;
        }
    }
}

void Game::apply_deferred_reparents()
{
    std::vector<std::pair<Thing_idx, Thing_idx>> changes;
    for (auto & thing : m_things)
        if (thing.new_parent_after_transaction)
            changes.emplace_back(thing.m_idx,
                                 thing.new_parent_after_transaction);
    for (auto [child, parent] : changes)
    {
        if (!add_child(m_things, parent, child))
        {
            m_things.get(child).new_parent_after_transaction = 0;
            send_error("Rejected invalid deferred reparent of entity "
                       + std::to_string(child));
        }
    }
}

void Game::switch_mode()
{
    if (m_editor_mode)
    {
        // Save the editing world before gameplay mutates entity state.
        if (!save_level(*this, current_level_path(*this))) return;
        m_editor_mode = false;
    }
    else
    {
        if (!load_level(*this, current_level_path(*this))) return;
        m_editor_mode = true;
        stop_sound();
    }
}

void Game::start_game(bool reload)
{
    if (reload)
    {
        if (m_editor_mode)
        {
            switch_game_to_scene(*this, SceneType::game);
            if (!save_level(*this, current_level_path(*this))) return;
        }
        else if (std::filesystem::exists(current_level_path(*this)))
        {
            if (!load_level(*this, current_level_path(*this))) return;
        }
    }
    stop_sound();
    switch_game_to_scene(*this, SceneType::game);
    Thing_idx player = 0, spawner = 0;
    for (auto & thing : m_things)
    {
        if (!player && thing.m_kind == EntityType::player) player = thing.m_idx;
        if (!spawner && thing.m_kind == EntityType::spawner)
            spawner = thing.m_idx;
    }
    if (spawner)
    {
        auto spawn = m_things.get(spawner);
        if (player)
        {
            auto & p = m_things.get(player);
            p.x = spawn.x;
            p.y = spawn.y;
            p.m_dead = false;
            p.m_active = true;
            p.m_speed_x = p.m_speed_y = 0;
            p.m_accel_x = p.m_accel_y = 0;
            p.m_jumps = 0;
            p.m_grounded = 0;
            p.m_climbing = p.m_was_climbing = p.m_coyote_frame = 0;
            p.m_inside_of_portal = 0;
            p.m_moving_platform_idx = 0;
            p.m_should_start_climbing = p.m_should_stop_climbing = 0;
            p.m_animation_state.clear();
            change_animation_state(p, "idle");
            p.new_parent_after_transaction = spawn.parent_idx;
        }
        else spawn_player(*this, spawn.parent_idx, spawn.x, spawn.y);
    }
    for (auto & thing : m_things)
    {
        if (thing.m_kind == EntityType::move_platform
            && !thing.m_path_point.empty())
        {
            thing.x = thing.m_path_point.front().x;
            thing.y = thing.m_path_point.front().y;
            thing.m_current_node = thing.m_direction = 0;
            thing.m_speed_x = thing.m_speed_y = 0;
        }
    }
    m_editor_mode = false;
    m_never_played = false;
    queue_sound("music", "music_game.pcm", true);
    queue_sound("start", "press_start.pcm");
}

void Game::queue_sound(std::string name,
                       std::string asset,
                       bool loop,
                       double gain)
{
    if (m_no_sound) return;
    m_sound_events.push_back({SoundEvent::Type::Play, std::move(name),
                              std::move(asset), loop,
                              std::clamp(gain, 0.0, 1.0)});
}

void Game::stop_sound(const std::string & name)
{
    m_sound_events.push_back(
        {name.empty() ? SoundEvent::Type::StopAll : SoundEvent::Type::StopOne,
         name, "", false, 0});
}

void Game::send_error(const std::string & error)
{
    m_errors.push_back(error);
    if (m_errors.size() > 200) m_errors.erase(m_errors.begin());
}

void Game::log(const std::string & message)
{
    m_logs.push_back(message);
    if (m_logs.size() > 200) m_logs.erase(m_logs.begin());
}

Thing_idx add_device(Game & game, int device_index)
{
    auto idx = game.m_things.add(EntityType::device);
    if (!idx) return 0;
    auto & device = game.m_things.get(idx);
    device.m_device_on = true;
    device.m_helios_index = device_index;
    device.m_intensity = .2;
    // Identity calibration is finite before the first calibration confirmation.
    device.m_base_scale_x = device.m_base_scale_y = 1;
    game.m_devices.push_back(idx);
    game.m_draw_lists[idx] = {};
    add_child(game.m_things, game.m_device_parent_idx, idx);
    return idx;
}

void create_projection_for_device(Game & game, Thing_idx device_idx)
{
    Thing & device = game.m_things.get(device_idx);

    if (ent_valid(get_scene_from_device(game.m_things, device,
                                        SceneType::projection)))
    {
        return;
    }

    Thing & scene = create_scene(game, SceneType::projection, device_idx);

    if (!ent_valid(scene))
    {
        return;
    }

    scene.m_cursors = device.m_cursors[0].x == 0 && device.m_cursors[0].y == 0
                          ? base_cursor_list
                          : device.m_cursors;
    set_device_to_scene(game.m_things, device_idx, SceneType::projection);
}

void create_scaling_for_device(Game & game, Thing_idx device_idx)
{
    auto & device = game.m_things.get(device_idx);
    if (ent_valid(
            get_scene_from_device(game.m_things, device, SceneType::scaling)))
        return;
    Thing & scene = create_scene(game, SceneType::scaling, device_idx);

    if (!ent_valid(scene))
    {
        return;
    }

    scene.m_horiz_scaling = scene.m_vert_scaling = .1;
    set_device_to_scene(game.m_things, device_idx, SceneType::scaling);
}

void create_game_for_device(Game & game, Thing_idx device_idx)
{
    auto & device = game.m_things.get(device_idx);
    if (ent_valid(
            get_scene_from_device(game.m_things, device, SceneType::game)))
        return;
    Thing_idx root = 0;
    for (auto type :
         {SceneType::game, SceneType::game_win, SceneType::game_over})
    {
        const Thing scene = create_scene(game, type, device_idx);
        if (type == SceneType::game)
        {
            root = scene.m_idx;
        };
    }
    game.m_never_played = false;
    if (!game.m_devices.empty() && device_idx == game.m_devices.front())
    {
        double sx = device.m_base_scale_x;
        double sy = device.m_base_scale_y;

        if (std::abs(sx) < 1e-9)
        {
            sx = 1;
        }
        if (std::abs(sy) < 1e-9)
        {
            sy = 1;
        }

        const Thing_idx player = spawn_player(game, root, .5 / sx, .5 / sy);
        make_entity(game, EntityType::spawner, root, .1 / sx, .9 / sy);
        if (game.m_editor_mode) game.m_entity_selected_index = player;
    }
}

void check_position_of_things(Game & game, Thing & device, Thing & scene)
{
    const auto sx =
        std::abs(device.m_base_scale_x) > 1e-9 ? device.m_base_scale_x : 1;
    const auto sy =
        std::abs(device.m_base_scale_y) > 1e-9 ? device.m_base_scale_y : 1;
    for (auto idx : game.m_things.children(scene.m_idx))
    {
        auto & thing = game.m_things.get(idx);
        if (thing.x < 0 || thing.x > 1 / sx) thing.x = .5 / sx;
        if (thing.y < 0 || thing.y > 1 / sy) thing.y = .5 / sy;
    }
}

void switch_device_to_scene(Game & game, Thing & device, SceneType value)
{
    if (device.m_kind != EntityType::device) return;
    auto type = value;
    if (type == SceneType::editor)
    {
        type = SceneType::game;
        game.m_editor_mode = true;
    }
    if (get_current_scene(game.m_things, device.m_idx).m_scene_type
        == SceneType::scaling)
        game.m_autosave_enabled = true;
    if (!ent_valid(get_scene_from_device(game.m_things, device, type)))
    {
        switch (type)
        {
        case SceneType::projection:
            create_projection_for_device(game, device.m_idx);
            break;
        case SceneType::scaling:
            create_scaling_for_device(game, device.m_idx);
            break;
        case SceneType::game:
        case SceneType::game_win:
        case SceneType::game_over:
            create_game_for_device(game, device.m_idx);
            break;
        default:
            break;
        }
    }
    auto & scene = get_scene_from_device(game.m_things, device, type);
    if (scene.m_scene_type == SceneType::game)
        check_position_of_things(game, device, scene);
    set_device_to_scene(game.m_things, device.m_idx, type);
}

void switch_game_to_scene(Game & game, SceneType type)
{
    for (auto idx : game.m_devices)
        switch_device_to_scene(game, game.m_things.get(idx), type);
}

} // namespace chienpo
