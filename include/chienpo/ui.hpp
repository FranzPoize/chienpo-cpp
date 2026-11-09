#pragma once
#include "chienpo/audio.hpp"
#include "chienpo/helios.hpp"
#include <unordered_set>
namespace ad::graphics {

class ApplicationGlfw;
}

namespace chienpo {

constexpr float LEFT_BAR_SIZE = 0.1f;
constexpr float ENTITY_LIST_SIZE = 0.2f;
constexpr float GAME_SIZE = (1 - (LEFT_BAR_SIZE + 2 * ENTITY_LIST_SIZE)) / 3;

struct Ui
{
    std::unordered_set<Thing_idx> big_previews;
    float layout_width = 0, layout_height = 0;
    bool hardware_enabled = true;
    bool show_context = false, show_console = false, show_blanking = false,
         fullscreen = false;
    bool animation_open = false, animation_play = false, animation_loop = false;
    Thing_idx animation_entity = 0;
    std::string animation_path, animation_state = "idle";
    int animation_clip = -1, animation_frame = 0;
    double animation_ticks = 1, animation_elapsed = 0;
    char level_name[256] = "my_level", blueprint_name[256] = "my_blueprint";
    char import_path[1024] = "", animation_search[256] = "",
         new_state[128] = "idle";
    char context_search[128] = "", console_command[1024] = "";
    int window_x = 0, window_y = 0, window_width = 1440, window_height = 900;
};

Input poll_input(const ad::graphics::ApplicationGlfw & application,
                 bool capture_keyboard);

void draw_ui(Game & game,
             Ui & ui,
             Audio & audio,
             HeliosOutput & helios,
             ad::graphics::ApplicationGlfw & application,
             double delta);

} // namespace chienpo
