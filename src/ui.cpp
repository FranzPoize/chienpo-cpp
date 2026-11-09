#include "chienpo/ui.hpp"

#include "chienpo/gameplay.hpp"
#include "chienpo/persistence.hpp"
#include "chienpo/render.hpp"
#include "chienpo/types.hpp"
#include "chienpo/vector_math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <graphics/ApplicationGlfw.h>
#include <imgui.h>
#include <sstream>
#include <string>

namespace chienpo {
namespace {
ImGuiCond layout_position_condition(const Ui & ui)
{
    const auto viewport = ImGui::GetMainViewport()->Size;
    // Compositors can resize the native window after its first frame. Rebuild
    // the proportional layout on resize, but leave dragged windows alone
    // while the viewport is unchanged (including when Big is toggled).
    return viewport.x != ui.layout_width || viewport.y != ui.layout_height
               ? ImGuiCond_Always
               : ImGuiCond_Once;
}

bool number(const char * name, double & value, double step = .01)
{
    if (!ImGui::InputDouble(name, &value, step, step * 10, "%.5g"))
    {
        return false;
    }
    return true;
}

bool text_input(const char * label, std::string & value)
{
    value.reserve(256);
    if (!ImGui::InputText(label, value.data(), 256))
    {
        return false;
    }
    return true;
}

std::string label(const Thing & thing)
{
    return std::to_string(thing.m_idx) + " "
           + std::string(to_string(thing.m_kind))
           + (thing.m_name.empty() ? "" : " - " + thing.m_name);
}

template <class Enum>
bool enum_combo(const char * name, Enum & value, Enum last)
{
    bool changed = false;
    if (ImGui::BeginCombo(name, to_string(value).data()))
    {
        for (int i = 0; i <= static_cast<int>(last); ++i)
        {
            const auto item = static_cast<Enum>(i);
            if (ImGui::Selectable(to_string(item).data(), item == value))
            {
                value = item;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool entity_combo(Game & game,
                  const char * name,
                  Thing_idx & index,
                  EntityType filter = EntityType::nil)
{
    bool changed = false;
    const auto preview = index ? label(game.m_things.get(index)) : "None";
    if (ImGui::BeginCombo(name, preview.c_str()))
    {
        if (ImGui::Selectable("None", index == 0))
        {
            index = 0;
            changed = true;
        }
        // Editor references include inactive entities (notably targets of
        // Activate).
        for (std::size_t slot = 1; slot < MAX_THINGS; ++slot)
        {
            auto & item = game.m_things.m_things[slot];
            if (!game.m_things.m_used[slot] || item.m_kind == EntityType::nil)
                continue;
            if (filter != EntityType::nil && item.m_kind != filter) continue;
            if (ImGui::Selectable(label(item).c_str(), item.m_idx == index))
            {
                index = item.m_idx;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void select_entity(Game & game, Thing_idx index)
{
    game.m_entity_selected_index = index;
    auto parent = index;
    SceneType scene = SceneType::nil_scene;
    for (std::size_t depth = 0; parent && depth < MAX_THINGS; ++depth)
    {
        auto & thing = game.m_things.get(parent);
        if (thing.m_kind == EntityType::scene_root_node)
            scene = thing.m_scene_type;
        if (thing.m_kind == EntityType::device)
        {
            game.m_selected_device_idx = parent;
            if (scene != SceneType::nil_scene)
                set_device_to_scene(game.m_things, parent, scene);
            break;
        }
        parent = thing.parent_idx;
    }
}

void tree(Game & game, Thing_idx index, std::size_t depth = 0)
{
    if (depth >= MAX_THINGS)
    {
        return;
    }
    Thing & thing = game.m_things.get(index);
    if (!ent_valid(thing) || thing.m_scene_type == SceneType::projection
        || thing.m_scene_type == SceneType::scaling)
    {
        return;
    }
    std::vector<Thing_idx> children = game.m_things.children(index);
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (index == game.m_entity_selected_index)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (depth < 2)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    const std::string thing_label = label(thing);

    const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(index),
                                          flags, "%s", thing_label.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        select_entity(game, index);
    }
    if (opened)
    {
        for (auto child : children)
            tree(game, child, depth + 1);
        ImGui::TreePop();
    }
}

std::filesystem::path named_path(const std::filesystem::path & folder,
                                 const char * name,
                                 const char * extension)
{
    std::filesystem::path filename = std::filesystem::path(name).filename();

    if (filename.empty() || filename == "." || filename == "..")
    {
        filename = "untitled";
    }
    if (filename.extension().empty())
    {
        filename += extension;
    }
    return folder / filename;
}

Thing_idx current_scene(Game & game)
{
    Thing & scene =
        get_current_scene(game.m_things, game.m_selected_device_idx);

    if (scene.m_scene_type == SceneType::projection
        || scene.m_scene_type == SceneType::scaling || !ent_valid(scene))
    {
        auto & device = game.m_things.get(game.m_selected_device_idx);
        switch_device_to_scene(game, device, SceneType::game);
    }

    return get_current_scene(game.m_things, game.m_selected_device_idx).m_idx;
}

void add_entity(Game & game, EntityType type)
{
    const Thing_idx parent = current_scene(game);
    const Thing_idx index = make_entity(game, type, parent);

    if (index)
    {
        select_entity(game, index);
    }
}

inline ImVec2 screen(const ImVec2 & origin, const DacPoint & p, float size)
{
    return ImVec2(origin.x + static_cast<float>(p.x) * size,
                  origin.y + static_cast<float>(p.y) * size);
}

void draw_points(const Frame & points, float size, bool blanking)
{
    ImDrawList * draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("canvas", ImVec2(size, size));
    draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size),
                        IM_COL32(4, 7, 12, 255));
    draw->PushClipRect(origin, ImVec2(origin.x + size, origin.y + size), true);

    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const DacPoint & p = points[i];
        const DacPoint & previous = points[i - 1];

        if (!std::isfinite(p.x) || !std::isfinite(p.y)
            || !std::isfinite(previous.x) || !std::isfinite(previous.y))
        {
            continue;
        }

        const bool dark = p.r == 0 && p.g == 0 && p.b == 0;
        if (dark && !blanking)
        {
            continue;
        }

        auto byte = [](double v) {
            return static_cast<int>(std::clamp(v, 0., 1.) * 255);
        };
        const auto color = dark
                               ? IM_COL32(0, 75, 110, 110)
                               : IM_COL32(byte(p.r), byte(p.g), byte(p.b), 255);
        draw->AddLine(screen(origin, previous, size), screen(origin, p, size),
                      color, 1.4f);
    }
    draw->PopClipRect();
    draw->AddRect(origin, ImVec2(origin.x + size, origin.y + size),
                  IM_COL32(110, 125, 145, 255));
}

void animation_choice(Ui & ui,
                      const Thing & thing,
                      const std::string & state,
                      int clip = -1)
{
    ui.animation_open = true;
    ui.animation_entity = thing.m_idx;
    ui.animation_state = state;
    ui.animation_clip = clip;
    ui.animation_frame = 0;
    ui.animation_elapsed = 0;
    ui.animation_play = false;
    ui.animation_path = "";
    ui.animation_ticks = 1;
    ui.animation_loop = false;
    if (clip >= 0)
    {
        const auto & a = thing.m_animation_path_map.at(state).at(
            static_cast<std::size_t>(clip));
        ui.animation_path = a.path;
        ui.animation_ticks = a.m_tick_per_frame;
        ui.animation_loop = a.m_loop;
    }
}

void animations(Ui & ui, Thing & thing)
{
    if (!ImGui::CollapsingHeader("Animation states",
                                 ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::Checkbox("Playing", &thing.m_playing);
    ImGui::Checkbox("Animation playing", &thing.m_animation_playing);

    std::string remove_state;
    for (auto & [state, clips] : thing.m_animation_path_map)
    {
        ImGui::PushID(state.c_str());
        if (ImGui::TreeNode(state.c_str()))
        {
            if (ImGui::Button("Play state"))
            {
                change_animation_state(thing, state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete state"))
            {
                remove_state = state;
            }

            for (std::size_t i = 0; i < clips.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                auto & clip = clips[i];
                if (ImGui::Button(clip.path.empty() ? "Choose clip"
                                                    : clip.path.c_str()))
                    animation_choice(ui, thing, state, static_cast<int>(i));
                number("Ticks per frame", clip.m_tick_per_frame, 1);
                clip.m_tick_per_frame = std::max(1., clip.m_tick_per_frame);
                ImGui::Checkbox("Loop", &clip.m_loop);
                if (ImGui::Button("Remove clip"))
                {
                    remove_animation_from_thing(thing, state, i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add clip"))
            {
                animation_choice(ui, thing, state);
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!remove_state.empty())
    {
        thing.m_animation_path_map.erase(remove_state);
        if (thing.m_animation_state == remove_state)
            thing.m_animation_state.clear();
    }
    ImGui::InputText("New state", ui.new_state, sizeof(ui.new_state));

    if (ImGui::Button("Create state") && ui.new_state[0])
    {
        thing.m_animation_path_map.try_emplace(ui.new_state);
    }

    ImGui::Text(
        "Current: %s | clip %d | tick %.1f", thing.m_animation_state.c_str(),
        thing.m_animation_sprite_sequence, thing.m_animation_tick_count);
}

void triggers(Game & game, Thing & thing)
{
    if (!ImGui::CollapsingHeader("Triggers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (ImGui::Button("Add trigger"))
    {
        std::string name = "Trigger " + std::to_string(thing.m_triggers.size());
        name.reserve(256);
        thing.m_triggers.emplace_back(name);
    }

    for (std::size_t i = 0; i < thing.m_triggers.size(); ++i)
    {
        Trigger & trigger = thing.m_triggers[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Separator();
        ImGui::InputText("name", trigger.m_name.data(), 256);
        ImGui::SameLine();
        if (ImGui::Button("X"))
        {
            thing.m_triggers.erase(thing.m_triggers.begin()
                                   + static_cast<std::ptrdiff_t>(i));
            ImGui::PopID();
            break;
        }
        enum_combo("When", trigger.m_trigger_action,
                   TriggerActionType::on_start);
        number("Count", trigger.m_trigger_count_target, 1);
        number("Delay (seconds)", trigger.m_trigger_delay, .1);
        trigger.m_trigger_delay = std::max(0., trigger.m_trigger_delay);
        enum_combo("Do", trigger.m_trigger_type, TriggerType::start_game);
        number("Chance (0..1)", trigger.m_trigger_percentage, .05);
        trigger.m_trigger_percentage =
            std::clamp(trigger.m_trigger_percentage, 0., 1.);
        ImGui::Text("Targets");
        for (std::size_t j = 0; j < trigger.m_trigger_target.size(); ++j)
        {
            ImGui::PushID(static_cast<int>(j));
            entity_combo(game, "Target", trigger.m_trigger_target[j]);
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
            {
                trigger.m_trigger_target.erase(
                    trigger.m_trigger_target.begin()
                    + static_cast<std::ptrdiff_t>(j));
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add target")) trigger.m_trigger_target.push_back(0);
        ImGui::PopID();
    }
}

void inspector(Game & game, Ui & ui)
{
    const auto viewport = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(viewport.x * (1 - 2 * ENTITY_LIST_SIZE), 0),
                            layout_position_condition(ui));
    ImGui::SetNextWindowSize(ImVec2(viewport.x * ENTITY_LIST_SIZE, viewport.y),
                             ImGuiCond_Always);
    if (ImGui::Begin("Entity inspector"))
    {
        auto & thing = game.m_things.get(game.m_entity_selected_index);
        if (ent_valid(thing))
        {
            ImGui::PushID(static_cast<int>(thing.m_idx));
            ImGui::TextUnformatted(label(thing).c_str());
            text_input("Name", thing.m_name);
            if (thing.m_kind != EntityType::device
                && thing.m_kind != EntityType::device_parent
                && thing.m_kind != EntityType::scene_root_node)
            {
                if (ImGui::Button("Delete entity"))
                {
                    const auto index = thing.m_idx;
                    game.m_entity_selected_index = 0;
                    remove_hierarchy(game.m_things, index);
                    ImGui::PopID();
                    ImGui::End();
                    return;
                }
                auto parent = thing.parent_idx;
                if (entity_combo(game, "Parent scene", parent,
                                 EntityType::scene_root_node)
                    && parent)
                {
                    if (!add_child(game.m_things, parent, thing.m_idx))
                        game.send_error("Invalid reparent request");
                    else select_entity(game, thing.m_idx);
                }
                for (auto device : game.m_devices)
                {
                    ImGui::PushID(static_cast<int>(device));
                    if (ImGui::Button(
                            ("Move to screen " + std::to_string(device))
                                .c_str()))
                    {
                        auto & target = get_scene_from_device(
                            game.m_things, game.m_things.get(device),
                            SceneType::game);
                        if (!ent_valid(target))
                            create_game_for_device(game, device);
                        auto & scene = get_scene_from_device(
                            game.m_things, game.m_things.get(device),
                            SceneType::game);
                        add_child(game.m_things, scene.m_idx, thing.m_idx);
                        game.m_selected_device_idx = device;
                        check_position_of_things(
                            game, game.m_things.get(device), scene);
                    }
                    ImGui::PopID();
                }
            }
            if (thing.m_kind == EntityType::scene_root_node)
            {
                auto type = thing.m_scene_type;
                if (enum_combo("Scene type", type, SceneType::game_win))
                {
                    bool duplicate = false;
                    for (auto sibling :
                         game.m_things.children(thing.parent_idx))
                        if (sibling != thing.m_idx
                            && game.m_things.get(sibling).m_scene_type == type)
                            duplicate = true;
                    if (duplicate)
                        game.send_error(
                            "This device already has a scene of that type");
                    else
                    {
                        thing.m_scene_type = type;
                        game.m_things.get(thing.parent_idx).m_scene_stack = {
                            type};
                    }
                }
            }
            if (ImGui::CollapsingHeader("States",
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Active", &thing.m_active);
                ImGui::Checkbox("Visible", &thing.m_show);
                ImGui::Checkbox("Collision", &thing.m_collision);
                ImGui::Checkbox("Don't mask", &thing.m_no_masking);
                ImGui::Checkbox("Dead", &thing.m_dead);
                ImGui::Text("Grounded %.0f | jumps %d | climbing %d",
                            thing.m_grounded, thing.m_jumps, thing.m_climbing);
                ImGui::Text("Parent %u | first child %u", thing.parent_idx,
                            thing.first_child_idx);
            }
            if (ImGui::CollapsingHeader("Transform and bounds",
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                number("x", thing.x);
                number("y", thing.y);
                number("Scale", thing.m_scale);
                number("xmin", thing.xmin);
                number("xmax", thing.xmax);
                number("ymin", thing.ymin);
                number("ymax", thing.ymax);
                number("Sprite offset x", thing.m_sprite_offset_x);
                number("Sprite offset y", thing.m_sprite_offset_y);
                number("Intensity modifier", thing.m_intensity_modifier);
            }
            if (ImGui::CollapsingHeader("Game data",
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                number("Speed x", thing.m_speed_x);
                number("Speed y", thing.m_speed_y);
                number("Acceleration x", thing.m_accel_x);
                number("Acceleration y", thing.m_accel_y);
                if (thing.m_kind == EntityType::portal)
                    entity_combo(game, "Destination portal",
                                 thing.m_portal_destination_idx,
                                 EntityType::portal);
                if (thing.m_kind == EntityType::neon_boss)
                {
                    number("Attack delay", thing.m_boss_avg_attack_delay, .1);
                    number("Attack variance", thing.m_boss_attack_variance, .1);
                }
                if (thing.m_kind == EntityType::trigger)
                    number("Counter", thing.m_count, 1);
                if (thing.m_kind == EntityType::laser)
                {
                    number("Angle", thing.m_angle);
                    number("Attack delay", thing.m_laser_delay_attack, .1);
                    number("Alarm delay", thing.m_laser_delay_alarm, .1);
                    number("Projectile speed", thing.m_laser_proj_speed);
                    number("Projectile size", thing.m_laser_proj_size);
                    number("Projectile folds", thing.m_laser_proj_fold, 1);
                    number("Projectile scale y", thing.m_laser_proj_scale_y);
                    number("Projectile start x",
                           thing.m_laser_proj_start_pos.x);
                    number("Projectile start y",
                           thing.m_laser_proj_start_pos.y);
                    ImGui::Checkbox("Ondulation", &thing.m_laser_ondulating);
                    ImGui::Checkbox("Color ondulation",
                                    &thing.m_laser_ondulating_color);
                }
            }
            if (thing.m_kind == EntityType::move_platform
                && ImGui::CollapsingHeader("Motion path",
                                           ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Loop path", &thing.m_loop);
                ImGui::Checkbox("Preview motion",
                                &thing.m_debug_moving_platform);
                for (std::size_t i = 0; i < thing.m_path_point.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    number("Node x", thing.m_path_point[i].x);
                    number("Node y", thing.m_path_point[i].y);
                    if (ImGui::SmallButton("Remove node"))
                    {
                        thing.m_path_point.erase(
                            thing.m_path_point.begin()
                            + static_cast<std::ptrdiff_t>(i));
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::Button("Add path node"))
                    thing.m_path_point.push_back({thing.x, thing.y});
                thing.m_path_point_size =
                    static_cast<int>(thing.m_path_point.size());
            }
            if (thing.m_kind == EntityType::projection_bound
                || thing.m_kind == EntityType::scaling_bound)
            {
                for (std::size_t i = 0; i < thing.m_cursors.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    number("Corner x", thing.m_cursors[i].x);
                    number("Corner y", thing.m_cursors[i].y);
                    ImGui::PopID();
                }
                ImGui::InputInt("Selected corner", &thing.m_cursor_idx);
                thing.m_cursor_idx = std::clamp(thing.m_cursor_idx, 0, 3);
                number("Horizontal scaling", thing.m_horiz_scaling);
                number("Vertical scaling", thing.m_vert_scaling);
            }
            animations(ui, thing);
            triggers(game, thing);
            ImGui::PopID();
        }
        else
            ImGui::TextUnformatted(
                "Select an entity from the hierarchy or a preview.");
    }
    ImGui::End();
}

void animation_browser(Game & game, Ui & ui, double delta)
{
    if (!ui.animation_open) return;
    ImGui::SetNextWindowSize(ImVec2(850, 680), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Choose animation", &ui.animation_open))
    {
        ImGui::InputText("Search", ui.animation_search,
                         sizeof(ui.animation_search));
        ImGui::BeginChild("Animation files", ImVec2(290, -45), true);
        for (const auto & path : game.m_animation_list)
        {
            if (std::string(path).find(ui.animation_search)
                == std::string::npos)
                continue;
            if (ImGui::Selectable(path.c_str(), ui.animation_path == path))
            {
                ui.animation_path = path;
                ui.animation_frame = 0;
                ui.animation_elapsed = 0;
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Animation preview", ImVec2(0, -45), true);
        text_input("State", ui.animation_state);
        number("Ticks per frame", ui.animation_ticks, 1);
        ui.animation_ticks = std::max(1., ui.animation_ticks);
        ImGui::Checkbox("Loop", &ui.animation_loop);
        ImGui::SameLine();
        ImGui::Checkbox("Preview playing", &ui.animation_play);
        if (!ui.animation_path.empty())
        {
            try
            {
                const auto & frames =
                    get_from_frame_cache(game, ui.animation_path);
                if (!frames.empty())
                {
                    if (ui.animation_play)
                    {
                        ui.animation_elapsed += delta * 60.;
                        while (ui.animation_elapsed >= ui.animation_ticks)
                        {
                            ui.animation_elapsed -= ui.animation_ticks;
                            ++ui.animation_frame;
                            if (ui.animation_frame
                                >= static_cast<int>(frames.size()))
                            {
                                if (ui.animation_loop) ui.animation_frame = 0;
                                else
                                {
                                    ui.animation_frame =
                                        static_cast<int>(frames.size()) - 1;
                                    ui.animation_play = false;
                                    break;
                                }
                            }
                        }
                    }
                    ImGui::SliderInt("Frame", &ui.animation_frame, 0,
                                     static_cast<int>(frames.size()) - 1);
                    ui.animation_frame =
                        std::clamp(ui.animation_frame, 0,
                                   static_cast<int>(frames.size()) - 1);
                    Frame frame =
                        frames[static_cast<std::size_t>(ui.animation_frame)];
                    for (auto & point : frame)
                    {
                        point.x = .5 + point.x * .45;
                        point.y = .5 + point.y * .45;
                    }
                    draw_points(
                        frame,
                        std::max(40.f,
                                 std::min(ImGui::GetContentRegionAvail().x,
                                          ImGui::GetContentRegionAvail().y)),
                        ui.show_blanking);
                }
                else ImGui::TextUnformatted("No frames in this asset.");
            }
            catch (const std::exception & e)
            {
                ImGui::TextWrapped("%s", e.what());
            }
        }
        ImGui::EndChild();
        auto & entity = game.m_things.get(ui.animation_entity);
        ImGui::BeginDisabled(!ent_valid(entity) || ui.animation_path.empty()
                             || ui.animation_state.empty());
        if (ImGui::Button("Save clip"))
        {
            if (ui.animation_clip >= 0)
                change_animation_on_thing(
                    game, entity, ui.animation_state,
                    static_cast<std::size_t>(ui.animation_clip),
                    ui.animation_path, ui.animation_ticks, ui.animation_loop);
            else
                add_animation_to_thing(game, entity, ui.animation_state,
                                       ui.animation_path, ui.animation_ticks,
                                       ui.animation_loop);
            ui.animation_open = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ui.animation_open = false;
    }
    ImGui::End();
}

inline void field(const Ui & ui, const char * name, double & value)
{
    if (std::strstr(name, ui.context_search))
    {
        number(name, value);
    }
}

void context(Game & game, Ui & ui)
{
    if (!ui.show_context) return;
    ImGui::SetNextWindowSize(ImVec2(480, 720), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Game context", &ui.show_context))
    {
        ImGui::InputText("Search", ui.context_search,
                         sizeof(ui.context_search));
        field(ui, "m_jump_max_height", game.m_context.m_jump_max_height);
        field(ui, "m_max_player_horizontal_speed",
              game.m_context.m_max_player_horizontal_speed);
        field(ui, "m_half_jump_length", game.m_context.m_half_jump_length);
        field(ui, "m_climbing_speed", game.m_context.m_climbing_speed);
        field(ui, "m_climbing_jump_boost",
              game.m_context.m_climbing_jump_boost);
        field(ui, "m_climbing_jump_division",
              game.m_context.m_climbing_jump_division);
        field(ui, "m_ground_accel", game.m_context.m_ground_accel);
        field(ui, "m_ground_accel_max_ratio",
              game.m_context.m_ground_accel_max_ratio);
        field(ui, "m_air_accel", game.m_context.m_air_accel);
        field(ui, "m_air_accel_max_ratio",
              game.m_context.m_air_accel_max_ratio);
        field(ui, "m_ground_friction", game.m_context.m_ground_friction);
        field(ui, "m_ground_friction_max_ratio",
              game.m_context.m_ground_friction_max_ratio);
        field(ui, "m_air_friction", game.m_context.m_air_friction);
        field(ui, "m_air_friction_max_ratio",
              game.m_context.m_air_friction_max_ratio);
        field(ui, "m_boss_avg_attack_delay",
              game.m_context.m_boss_avg_attack_delay);
        field(ui, "m_boss_attack_variance",
              game.m_context.m_boss_attack_variance);
        field(ui, "m_boss_proj_speed", game.m_context.m_boss_proj_speed);
        field(ui, "m_boss_proj_gravity", game.m_context.m_boss_proj_gravity);
        field(ui, "m_boss_proj_spring", game.m_context.m_boss_proj_spring);
        field(ui, "m_boss_proj_split_count",
              game.m_context.m_boss_proj_split_count);
        field(ui, "m_laser_delay_attack", game.m_context.m_laser_delay_attack);
        field(ui, "m_base_player_scale", game.m_context.m_base_player_scale);
        field(ui, "m_coyote_frame_amount",
              game.m_context.m_coyote_frame_amount);
        field(ui, "m_moving_platform_speed",
              game.m_context.m_moving_platform_speed);
        field(ui, "m_intensity", game.m_context.m_intensity);
        field(ui, "m_scale", game.m_context.m_scale);
        field(ui, "m_fps", game.m_context.m_fps);
        field(ui, "m_pps", game.m_context.m_pps);
        field(ui, "m_unselected_intensity",
              game.m_context.m_unselected_intensity);
        field(ui, "m_selected_intensity", game.m_context.m_selected_intensity);
        field(ui, "m_base_intensity_modifier",
              game.m_context.m_base_intensity_modifier);
        field(ui, "m_cursor_speed", game.m_context.m_cursor_speed);
        field(ui, "m_game_over_pos_x", game.m_context.m_game_over_pos_x);
        field(ui, "m_game_over_pos_y", game.m_context.m_game_over_pos_y);
        field(ui, "m_game_over_scale", game.m_context.m_game_over_scale);
        field(ui, "m_game_over_proj_index",
              game.m_context.m_game_over_proj_index);
        field(ui, "m_game_win_pos_x", game.m_context.m_game_win_pos_x);
        field(ui, "m_game_win_pos_y", game.m_context.m_game_win_pos_y);
        field(ui, "m_game_win_scale", game.m_context.m_game_win_scale);
        field(ui, "m_game_win_proj_index",
              game.m_context.m_game_win_proj_index);
        field(ui, "m_smoothing_step_number_long",
              game.m_context.m_smoothing_step_number_long);
        field(ui, "m_smoothing_step_number_short",
              game.m_context.m_smoothing_step_number_short);
        field(ui, "m_stop_on_end_point", game.m_context.m_stop_on_end_point);
        field(ui, "m_center_bounds.x", game.m_context.m_center_bounds.x);
        field(ui, "m_center_bounds.y", game.m_context.m_center_bounds.y);
        ImGui::Checkbox("Draw bounds", &game.m_context.m_draw_bounds);
        game.m_context.m_fps = std::clamp(game.m_context.m_fps, 1., 1000.);
        game.m_context.m_pps = std::clamp(game.m_context.m_pps, 1., 65535.);
        game.m_context.m_max_player_horizontal_speed =
            std::max(.00001, game.m_context.m_max_player_horizontal_speed);
        game.m_context.m_half_jump_length =
            std::max(.00001, game.m_context.m_half_jump_length);
        game.m_context.m_climbing_jump_division =
            std::max(.00001, game.m_context.m_climbing_jump_division);
        game.m_context.m_scale = std::max(.00001, game.m_context.m_scale);
    }
    ImGui::End();
}

Frame preview_points(const Frame & source, const Thing & device)
{
    Frame points = source;
    if (device.m_cancel_projection)
    {
        // Undo the projector homography for the editor's flat screen preview.
        const auto & m = device.m_distortion_matrix;
        const double a = m[0][0], b = m[0][1], c = m[0][2], d = m[1][0],
                     e = m[1][1], f = m[1][2], g = m[2][0], h = m[2][1],
                     i = m[2][2];
        const double determinant =
            a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
        if (std::abs(determinant) > 1e-12)
        {
            TransformMatrix inverse{
                {{e * i - f * h, c * h - b * i, b * f - c * e},
                 {f * g - d * i, a * i - c * g, c * d - a * f},
                 {d * h - e * g, b * g - a * h, a * e - b * d}}};
            for (auto & row : inverse)
                for (auto & value : row)
                    value /= determinant;
            apply_perspective_transform(points, inverse);
        }
    }
    return points;
}

void previews(Game & game, Ui & ui, HeliosOutput & helios)
{
    Thing_idx remove = 0;
    const auto viewport = ImGui::GetMainViewport()->Size;
    const float base_width = viewport.x * GAME_SIZE;
    const float frame_spacing = ImGui::GetFrameHeightWithSpacing();
    for (std::size_t pos_index = 0; pos_index < game.m_devices.size();
         ++pos_index)
    {
        const Thing_idx device_index = game.m_devices[pos_index];
        Thing & device = game.m_things.get(device_index);
        const float game_width =
            base_width * (ui.big_previews.count(device_index) ? 2.f : 1.f);
        const float game_height = game_width + frame_spacing * 5;
        // Enlarging a screen leaves its neighbours in their original cells.
        const float game_offset_x =
            viewport.x * LEFT_BAR_SIZE + base_width * (pos_index % 3);
        const float game_offset_y =
            (base_width + frame_spacing * 5) * (pos_index / 3);
        ImGui::SetNextWindowPos(ImVec2(game_offset_x, game_offset_y),
                                layout_position_condition(ui));
        ImGui::SetNextWindowSize(ImVec2(game_width, game_height),
                                 ImGuiCond_Always);
        const std::string projector_string =
            device.m_helios_index >= 0
                ? " (Projector " + std::to_string(device.m_helios_index) + ")"
                : "";
        const std::string title = "Screen " + std::to_string(device_index)
                                  + projector_string + "###device "
                                  + std::to_string(device_index);
        constexpr auto flags = ImGuiWindowFlags_NoResize
                               | ImGuiWindowFlags_NoScrollbar
                               | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(title.c_str(), nullptr, flags))
        {
            ImGui::PushID(static_cast<int>(device_index));
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                game.m_selected_device_idx = device_index;

            // Keep the original single-line controls readable in a narrow
            // three-column preview. Big screens use the normal font size.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 3));
            ImGui::SetWindowFontScale(1.f);
            const float available = ImGui::GetContentRegionAvail().x;
            const std::string points =
                "Points:" + std::to_string(device.m_point_count);
            const float toolbar_text =
                ImGui::CalcTextSize("OnProjMapBigProjector").x
                + ImGui::GetFontSize();
            const float footer_text =
                ImGui::CalcTextSize(("Invert xTightness" + points).c_str()).x
                + ImGui::GetFontSize();
            const float control_scale = std::min(
                {1.f, (available - 112.f) / toolbar_text,
                 (available - (pos_index ? 102.f : 90.f)) / footer_text});
            ImGui::SetWindowFontScale(std::max(.5f, control_scale));

            ImGui::Checkbox("On", &device.m_device_on);
            ImGui::SameLine();
            const bool projection_valid =
                !mat_equal(device.m_distortion_matrix, g_identity_matrix);
            if (projection_valid)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      IM_COL32(50, 150, 52, 255));
            const bool projection_clicked = ImGui::Button("Proj");
            if (projection_valid) ImGui::PopStyleColor();
            if (projection_clicked)
            {
                device.m_distortion_matrix = g_identity_matrix;
                device.m_world_to_view_matrix = g_identity_matrix;
                switch_device_to_scene(game, device, SceneType::projection);
            }
            ImGui::SameLine();
            const bool map_valid =
                !mat_equal(device.m_world_to_view_matrix, g_identity_matrix);
            if (map_valid)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      IM_COL32(50, 150, 52, 255));
            const bool map_clicked = ImGui::Button("Map");
            if (map_valid) ImGui::PopStyleColor();
            if (map_clicked)
                switch_device_to_scene(game, device, SceneType::scaling);
            ImGui::SameLine();
            if (ImGui::Button("Big"))
            {
                if (!ui.big_previews.erase(device_index))
                    ui.big_previews.insert(device_index);
            }
            const std::string projector =
                device.m_helios_index < 0
                    ? "None"
                    : std::to_string(device.m_helios_index);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::BeginCombo("Projector", projector.c_str()))
            {
                for (int value = -1; value < helios.device_count; ++value)
                {
                    const auto name =
                        value < 0
                            ? "None"
                            : std::to_string(value) + " "
                                  + (value < static_cast<int>(
                                         helios.device_names.size())
                                         ? helios.device_names
                                               [static_cast<std::size_t>(value)]
                                         : "");
                    if (ImGui::Selectable(name.c_str(),
                                          value == device.m_helios_index))
                    {
                        for (auto other : game.m_devices)
                            if (other != device_index && value >= 0
                                && game.m_things.get(other).m_helios_index
                                       == value)
                                game.m_things.get(other).m_helios_index =
                                    device.m_helios_index;
                        device.m_helios_index = value;
                    }
                }
                ImGui::EndCombo();
            }

            const auto it = game.m_draw_lists.find(device_index);
            const float canvas_size = ImGui::GetContentRegionAvail().x;
            draw_points(it != game.m_draw_lists.end()
                            ? preview_points(it->second, device)
                            : Frame{},
                        canvas_size, ui.show_blanking);
            if (ImGui::IsItemClicked())
                game.m_selected_device_idx = device_index;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * .4f);
            float intensity = static_cast<float>(device.m_intensity);
            if (ImGui::DragFloat("intensity", &intensity, .005f, 0.f, 1.f))
                device.m_intensity =
                    std::clamp(static_cast<double>(intensity), 0., 1.);
            ImGui::SameLine();
            ImGui::Checkbox("Boundary on", &device.m_show_boundary);
            if (pos_index != 0)
            {
                if (ImGui::Button("x")) remove = device_index;
                ImGui::SameLine();
            }
            ImGui::Checkbox("Invert x", &device.m_invert_x);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            int tightness = static_cast<int>(device.m_tightness);
            if (ImGui::InputInt("Tightness", &tightness))
                device.m_tightness = std::clamp(tightness, 0, 100);
            ImGui::SameLine();
            ImGui::TextUnformatted(points.c_str());
            int fps = static_cast<int>(device.m_fps);
            if (ImGui::InputInt("FPS", &fps))
                device.m_fps = std::clamp(fps, 1, 1000);
            ImGui::SetWindowFontScale(1.f);
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        }
        ImGui::End();
    }
    if (remove)
    {
        helios_stop(helios);
        remove_hierarchy(game.m_things, remove);
        game.m_devices.erase(
            std::remove(game.m_devices.begin(), game.m_devices.end(), remove),
            game.m_devices.end());
        ui.big_previews.erase(remove);
        game.m_selected_device_idx = game.m_devices.front();
        game.m_entity_selected_index = 0;
    }
}

void data(Game & game, Ui & ui, Audio & audio)
{
    const auto viewport = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(viewport.x * (1 - ENTITY_LIST_SIZE), 0),
                            layout_position_condition(ui));
    ImGui::SetNextWindowSize(ImVec2(viewport.x * ENTITY_LIST_SIZE, viewport.y),
                             ImGuiCond_Always);
    if (ImGui::Begin("Data") && ImGui::BeginTabBar("Data tabs"))
    {
        if (ImGui::BeginTabItem("Entities"))
        {
            tree(game, game.m_device_parent_idx);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Levels"))
        {
            ImGui::InputText("Name", ui.level_name, sizeof(ui.level_name));
            if (ImGui::Button("Save level"))
            {
                if (save_level(game, named_path(game.m_paths.levels,
                                                ui.level_name, ".cpl")))
                    refresh_resources(game);
            }
            ImGui::Checkbox("Autosave (10 seconds)", &game.m_autosave_enabled);
            for (const auto & name : game.m_level_list)
            {
                ImGui::PushID(name.c_str());
                ImGui::TextUnformatted(name.c_str());
                ImGui::BeginDisabled(!game.m_editor_mode);
                if (ImGui::Button("Load"))
                {
                    load_level(game, game.m_paths.levels / name);
                    std::snprintf(
                        ui.level_name, sizeof(ui.level_name), "%s",
                        std::filesystem::path(name).stem().string().c_str());
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Blueprints"))
        {
            ImGui::InputText("Name", ui.blueprint_name,
                             sizeof(ui.blueprint_name));
            ImGui::BeginDisabled(
                !ent_valid(game.m_things.get(game.m_entity_selected_index)));
            if (ImGui::Button("Export selected"))
            {
                if (save_blueprint(game, game.m_entity_selected_index,
                                   named_path(game.m_paths.blueprints,
                                              ui.blueprint_name, ".json")))
                    refresh_resources(game);
            }
            ImGui::EndDisabled();
            for (const auto & name : game.m_blueprints)
            {
                ImGui::PushID(name.c_str());
                if (ImGui::Button(("Add " + name).c_str()))
                {
                    auto idx =
                        load_blueprint(game, game.m_paths.blueprints / name,
                                       current_scene(game));
                    if (idx) select_entity(game, idx);
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sounds"))
        {
            ImGui::Checkbox("Mute", &game.m_no_sound);
            if (ImGui::Button("Stop all"))
            {
                game.stop_sound();
                audio_stop(audio);
            }
            if (!audio.available)
                ImGui::TextWrapped("Audio unavailable: %s",
                                   audio.error.c_str());
            for (const auto & sound : game.m_sound_list)
            {
                const auto asset = sound_asset_name(sound);
                auto [it, inserted] = audio.volumes.try_emplace(asset, .3f);
                (void) inserted;
                ImGui::PushID(asset.c_str());
                ImGui::TextUnformatted(sound.c_str());
                ImGui::SliderFloat("Gain", &it->second, 0, 1);
                ImGui::Checkbox("Loop", &audio.loops[asset]);
                if (ImGui::Button("Play"))
                    game.queue_sound(asset, asset, audio.loops[asset],
                                     it->second);
                ImGui::SameLine();
                if (ImGui::Button("Stop"))
                {
                    game.stop_sound(asset);
                    audio_stop(audio, asset);
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Import"))
        {
            ImGui::InputText("File path", ui.import_path,
                             sizeof(ui.import_path));
            if (ImGui::Button("Import level")) load_level(game, ui.import_path);
            if (ImGui::Button("Import blueprint"))
            {
                auto idx =
                    load_blueprint(game, ui.import_path, current_scene(game));
                if (idx) select_entity(game, idx);
            }
            if (ImGui::Button("Import ILDA animation"))
            {
                try
                {
                    const std::filesystem::path source = ui.import_path;
                    if (load_ilda(source).empty())
                        throw std::runtime_error(
                            "ILDA contains no drawable frames");
                    std::filesystem::create_directories(
                        game.m_paths.animations);
                    const auto target =
                        game.m_paths.animations / source.filename();
                    if (std::filesystem::absolute(source).lexically_normal()
                        != std::filesystem::absolute(target).lexically_normal())
                        std::filesystem::copy_file(
                            source, target,
                            std::filesystem::copy_options::overwrite_existing);
                    game.m_frame_cache.m_frame_cache.clear();
                    refresh_resources(game);
                    game.log("Imported animation " + target.string());
                }
                catch (const std::exception & e)
                {
                    game.send_error(e.what());
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void execute_console(Game & game, Ui & ui, Audio & audio, HeliosOutput & helios)
{
    const std::string command = ui.console_command;
    game.log("> " + command);
    std::istringstream input(command);
    std::string verb;
    input >> verb;
    std::string rest;
    std::getline(input >> std::ws, rest);
    if (verb == "help")
        game.log(
            "help | start | reset | mode | clear | scan | stop | save PATH | "
            "load PATH | select ID | scene NAME | add KIND | set FIELD VALUE");
    else if (verb == "start") game.start_game();
    else if (verb == "reset")
    {
        helios_stop(helios);
        game.reset_game(0);
        for (auto index : game.m_devices)
            game.m_things.get(index).m_device_on = false;
    }
    else if (verb == "mode") game.switch_mode();
    else if (verb == "clear") game.m_logs.clear();
    else if (verb == "scan")
    {
        if (ui.hardware_enabled) game.m_helios_count = helios_rescan(helios);
        else game.log("Helios disabled with --no-helios");
    }
    else if (verb == "stop")
    {
        audio_stop(audio);
        game.stop_sound();
        helios_stop(helios);
    }
    else if (verb == "save" && !rest.empty()) save_level(game, rest);
    else if (verb == "load" && !rest.empty()) load_level(game, rest);
    else if (verb == "select")
    {
        try
        {
            const auto value = std::stoul(rest);
            if (value < MAX_THINGS
                && ent_valid(game.m_things.get(static_cast<Thing_idx>(value))))
                select_entity(game, static_cast<Thing_idx>(value));
            else game.send_error("Invalid entity ID");
        }
        catch (...)
        {
            game.send_error("Expected entity ID");
        }
    }
    else if (verb == "scene")
    {
        SceneType type;
        if (from_string(rest, type)) switch_game_to_scene(game, type);
        else game.send_error("Unknown scene type");
    }
    else if (verb == "add")
    {
        EntityType type;
        if (from_string(rest, type) && type != EntityType::nil
            && type != EntityType::device_parent)
            add_entity(game, type);
        else game.send_error("Unknown entity type");
    }
    else if (verb == "set")
    {
        std::istringstream values(rest);
        std::string field;
        double value = 0;
        values >> field >> value;
        bool found = false;
        if (values && std::isfinite(value))
        {
            if (field == "m_jump_max_height")
            {
                game.m_context.m_jump_max_height = value;
                found = true;
            }
            if (field == "m_max_player_horizontal_speed")
            {
                game.m_context.m_max_player_horizontal_speed = value;
                found = true;
            }
            if (field == "m_half_jump_length")
            {
                game.m_context.m_half_jump_length = value;
                found = true;
            }
            if (field == "m_climbing_speed")
            {
                game.m_context.m_climbing_speed = value;
                found = true;
            }
            if (field == "m_climbing_jump_boost")
            {
                game.m_context.m_climbing_jump_boost = value;
                found = true;
            }
            if (field == "m_climbing_jump_division")
            {
                game.m_context.m_climbing_jump_division = value;
                found = true;
            }
            if (field == "m_ground_accel")
            {
                game.m_context.m_ground_accel = value;
                found = true;
            }
            if (field == "m_ground_accel_max_ratio")
            {
                game.m_context.m_ground_accel_max_ratio = value;
                found = true;
            }
            if (field == "m_air_accel")
            {
                game.m_context.m_air_accel = value;
                found = true;
            }
            if (field == "m_air_accel_max_ratio")
            {
                game.m_context.m_air_accel_max_ratio = value;
                found = true;
            }
            if (field == "m_ground_friction")
            {
                game.m_context.m_ground_friction = value;
                found = true;
            }
            if (field == "m_ground_friction_max_ratio")
            {
                game.m_context.m_ground_friction_max_ratio = value;
                found = true;
            }
            if (field == "m_air_friction")
            {
                game.m_context.m_air_friction = value;
                found = true;
            }
            if (field == "m_air_friction_max_ratio")
            {
                game.m_context.m_air_friction_max_ratio = value;
                found = true;
            }
            if (field == "m_boss_avg_attack_delay")
            {
                game.m_context.m_boss_avg_attack_delay = value;
                found = true;
            }
            if (field == "m_boss_attack_variance")
            {
                game.m_context.m_boss_attack_variance = value;
                found = true;
            }
            if (field == "m_boss_proj_speed")
            {
                game.m_context.m_boss_proj_speed = value;
                found = true;
            }
            if (field == "m_boss_proj_gravity")
            {
                game.m_context.m_boss_proj_gravity = value;
                found = true;
            }
            if (field == "m_boss_proj_spring")
            {
                game.m_context.m_boss_proj_spring = value;
                found = true;
            }
            if (field == "m_boss_proj_split_count")
            {
                game.m_context.m_boss_proj_split_count = value;
                found = true;
            }
            if (field == "m_laser_delay_attack")
            {
                game.m_context.m_laser_delay_attack = value;
                found = true;
            }
            if (field == "m_base_player_scale")
            {
                game.m_context.m_base_player_scale = value;
                found = true;
            }
            if (field == "m_coyote_frame_amount")
            {
                game.m_context.m_coyote_frame_amount = value;
                found = true;
            }
            if (field == "m_moving_platform_speed")
            {
                game.m_context.m_moving_platform_speed = value;
                found = true;
            }
            if (field == "m_intensity")
            {
                game.m_context.m_intensity = value;
                found = true;
            }
            if (field == "m_scale")
            {
                game.m_context.m_scale = value;
                found = true;
            }
            if (field == "m_fps")
            {
                game.m_context.m_fps = value;
                found = true;
            }
            if (field == "m_pps")
            {
                game.m_context.m_pps = value;
                found = true;
            }
            if (field == "m_unselected_intensity")
            {
                game.m_context.m_unselected_intensity = value;
                found = true;
            }
            if (field == "m_selected_intensity")
            {
                game.m_context.m_selected_intensity = value;
                found = true;
            }
            if (field == "m_base_intensity_modifier")
            {
                game.m_context.m_base_intensity_modifier = value;
                found = true;
            }
            if (field == "m_cursor_speed")
            {
                game.m_context.m_cursor_speed = value;
                found = true;
            }
            if (field == "m_game_over_pos_x")
            {
                game.m_context.m_game_over_pos_x = value;
                found = true;
            }
            if (field == "m_game_over_pos_y")
            {
                game.m_context.m_game_over_pos_y = value;
                found = true;
            }
            if (field == "m_game_over_scale")
            {
                game.m_context.m_game_over_scale = value;
                found = true;
            }
            if (field == "m_game_over_proj_index")
            {
                game.m_context.m_game_over_proj_index = value;
                found = true;
            }
            if (field == "m_game_win_pos_x")
            {
                game.m_context.m_game_win_pos_x = value;
                found = true;
            }
            if (field == "m_game_win_pos_y")
            {
                game.m_context.m_game_win_pos_y = value;
                found = true;
            }
            if (field == "m_game_win_scale")
            {
                game.m_context.m_game_win_scale = value;
                found = true;
            }
            if (field == "m_game_win_proj_index")
            {
                game.m_context.m_game_win_proj_index = value;
                found = true;
            }
            if (field == "m_smoothing_step_number_long")
            {
                game.m_context.m_smoothing_step_number_long = value;
                found = true;
            }
            if (field == "m_smoothing_step_number_short")
            {
                game.m_context.m_smoothing_step_number_short = value;
                found = true;
            }
            if (field == "m_stop_on_end_point")
            {
                game.m_context.m_stop_on_end_point = value;
                found = true;
            }
        }
        if (!found)
            game.send_error("Expected: set CONTEXT_FIELD NUMBER (field names "
                            "shown in Game context)");
    }
    else if (!verb.empty()) game.send_error("Unknown command. Type help.");
    ui.console_command[0] = '\0';
}

void console(Game & game, Ui & ui, Audio & audio, HeliosOutput & helios)
{
    if (!ui.show_console) return;
    ImGui::SetNextWindowSize(ImVec2(800, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Console", &ui.show_console))
    {
        ImGui::BeginChild("History", ImVec2(0, -40), true);
        for (const auto & line : game.m_logs)
            ImGui::TextUnformatted(line.c_str());
        for (const auto & line : game.m_errors)
            ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1);
        ImGui::EndChild();
        if (ImGui::InputText("Command", ui.console_command,
                             sizeof(ui.console_command),
                             ImGuiInputTextFlags_EnterReturnsTrue))
            execute_console(game, ui, audio, helios);
    }
    ImGui::End();
}

void controls(Game & game,
              Ui & ui,
              Audio & audio,
              HeliosOutput & helios,
              ad::graphics::ApplicationGlfw & application)
{
    const float left_width = ImGui::GetMainViewport()->Size.x * LEFT_BAR_SIZE;
    const float control_height = 300 + ImGui::GetFrameHeightWithSpacing();
    ImGui::SetNextWindowPos(ImVec2(0, 0), layout_position_condition(ui));
    ImGui::SetNextWindowSize(ImVec2(left_width, control_height),
                             ImGuiCond_Always);
    if (ImGui::Begin("Game control"))
    {
        ImGui::TextWrapped("Level: %s", game.m_current_game.c_str());
        ImGui::Text("%s | tick %llu", game.m_editor_mode ? "Editor" : "Playing",
                    static_cast<unsigned long long>(game.m_tick_count));
        if (ImGui::Button("Start / Restart")) game.start_game();
        if (ImGui::Button(game.m_editor_mode ? "Play mode" : "Back to editor"))
            game.switch_mode();
        if (ImGui::Button("New world"))
        {
            helios_stop(helios);
            game.reset_game(0);
            for (auto index : game.m_devices)
                game.m_things.get(index).m_device_on = false;
            game.m_current_game = "autosave";
        }
        if (ImGui::Button("Stop sound"))
        {
            game.stop_sound();
            audio_stop(audio);
        }
        if (ImGui::Button("Add screen"))
        {
            auto index = add_device(game, -1);
            game.m_things.get(index).m_device_on = false;
            create_projection_for_device(game, index);
            game.m_selected_device_idx = index;
        }
        ImGui::BeginDisabled(!ui.hardware_enabled);
        if (ImGui::Button("Scan Helios"))
        {
            game.m_helios_count = helios_rescan(helios);
            if (!helios.error.empty()) game.send_error(helios.error);
        }
        ImGui::EndDisabled();
        ImGui::Text("Helios connected: %d", helios.device_count);
        if (ImGui::Button("Add scene"))
        {
            for (auto device : game.m_devices)
            {
                const auto index =
                    game.m_things.add(EntityType::scene_root_node);
                game.m_things.get(index).m_name = "New scene";
                add_child(game.m_things, device, index);
            }
        }
        if (ImGui::Button("Refresh resources")) refresh_resources(game);
        if (ImGui::Button("Fullscreen"))
        {
            auto * window = application.getGlfwWindow();
            ui.fullscreen = !ui.fullscreen;
            if (ui.fullscreen)
            {
                glfwGetWindowPos(window, &ui.window_x, &ui.window_y);
                glfwGetWindowSize(window, &ui.window_width, &ui.window_height);
                auto * monitor = glfwGetPrimaryMonitor();
                const auto * mode = glfwGetVideoMode(monitor);
                if (mode)
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width,
                                         mode->height, mode->refreshRate);
            }
            else
                glfwSetWindowMonitor(window, nullptr, ui.window_x, ui.window_y,
                                     ui.window_width, ui.window_height,
                                     GLFW_DONT_CARE);
        }
        if (ImGui::Button("Quit")) application.markWindowShouldClose();
        ImGui::Checkbox("Game context", &ui.show_context);
        ImGui::Checkbox("Console", &ui.show_console);
        ImGui::Checkbox("Show blanking", &ui.show_blanking);
        ImGui::Checkbox("Draw bounds", &game.m_context.m_draw_bounds);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, control_height),
                            layout_position_condition(ui));
    ImGui::SetNextWindowSize(
        ImVec2(left_width, 250 + ImGui::GetFrameHeightWithSpacing()),
        ImGuiCond_Always);
    if (ImGui::Begin("Entities palette"))
    {
        constexpr EntityType palette[]{
            EntityType::platform,    EntityType::move_platform,
            EntityType::masking_box, EntityType::neon_boss,
            EntityType::portal,      EntityType::decor,
            EntityType::laser,       EntityType::trigger,
            EntityType::player,      EntityType::spawner};
        for (auto type : palette)
        {
            if (ImGui::Button(to_string(type).data())) add_entity(game, type);
        }
        if (ImGui::CollapsingHeader("Keyboard"))
            ImGui::TextWrapped(
                "Arrows: move. Space/X: jump or next point. Enter: confirm "
                "calibration. Escape: cancel. Shift: precision. O/P: "
                "next/previous entity. Backspace/Delete: delete. I/K: scale. "
                "=: reset scale. L: loop path. F2: mode. 1..8: entity "
                "palette.");
    }
    ImGui::End();
}

} // namespace
Input poll_input(const ad::graphics::ApplicationGlfw & application,
                 bool capture_keyboard)
{
    // Poll a complete snapshot every simulation tick: focus loss and ImGui text
    // capture produce releases as well as presses, so a key cannot get stuck
    // while editing.
    Input input;
    if (capture_keyboard
        || !glfwGetWindowAttrib(application.getGlfwWindow(), GLFW_FOCUSED))
        return input;
    auto key = [&](int code) { return application.getKey(code) == GLFW_PRESS; };
    input.up = key(GLFW_KEY_UP);
    input.down = key(GLFW_KEY_DOWN);
    input.left = key(GLFW_KEY_LEFT);
    input.right = key(GLFW_KEY_RIGHT);
    input.jump = input.next = key(GLFW_KEY_SPACE) || key(GLFW_KEY_X);
    input.prev = key(GLFW_KEY_Z);
    input.confirm = key(GLFW_KEY_ENTER);
    input.cancel = key(GLFW_KEY_ESCAPE);
    input.shift = key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_RIGHT_SHIFT);
    input.next_entity = key(GLFW_KEY_O);
    input.prev_entity = key(GLFW_KEY_P);
    input.delete_key = key(GLFW_KEY_BACKSPACE) || key(GLFW_KEY_DELETE);
    input.switch_mode = key(GLFW_KEY_F2);
    input.scale_up = key(GLFW_KEY_I);
    input.scale_down = key(GLFW_KEY_K);
    input.reset_scale = key(GLFW_KEY_EQUAL);
    input.loop_platform = key(GLFW_KEY_L);
    input.add_platform = key(GLFW_KEY_1);
    input.add_moving_platform = key(GLFW_KEY_2);
    input.add_masking_box = key(GLFW_KEY_3);
    input.add_neon_boss = key(GLFW_KEY_4);
    input.add_portal = key(GLFW_KEY_5);
    input.add_laser = key(GLFW_KEY_6);
    input.add_trigger_thing = key(GLFW_KEY_7);
    input.add_decor = key(GLFW_KEY_8);
    return input;
}

void draw_ui(Game & game,
             Ui & ui,
             Audio & audio,
             HeliosOutput & helios,
             ad::graphics::ApplicationGlfw & application,
             double delta)
{
    const auto viewport = ImGui::GetMainViewport()->Size;
    controls(game, ui, audio, helios, application);
    previews(game, ui, helios);
    data(game, ui, audio);
    inspector(game, ui);
    context(game, ui);
    animation_browser(game, ui, delta);
    console(game, ui, audio, helios);
    ui.layout_width = viewport.x;
    ui.layout_height = viewport.y;
    if (!game.m_errors.empty())
    {
        ImGui::SetNextWindowSize(ImVec2(540, 180), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Error message"))
        {
            ImGui::TextWrapped("%s", game.m_errors.back().c_str());
            if (ImGui::Button("Dismiss")) game.m_errors.pop_back();
            ImGui::SameLine();
            if (ImGui::Button("Show console")) ui.show_console = true;
        }
        ImGui::End();
    }
}

} // namespace chienpo
