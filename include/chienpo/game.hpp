#pragma once
#include "chienpo/types.hpp"
#include <random>

namespace chienpo {
struct SoundEvent {
  enum class Type { Play, StopOne, StopAll };
  Type type = Type::Play;
  std::string name;
  std::string asset;
  bool loop = false;
  double gain = .3;
};
struct ResourcePaths {
  std::filesystem::path root = "resources";
  std::filesystem::path levels = "resources/saved_levels";
  std::filesystem::path blueprints = "resources/blueprints";
  std::filesystem::path animations = "resources/animations";
  std::filesystem::path music = "resources/music";
};

class Game {
public:
  Things m_things;
  Thing_idx m_device_parent_idx = 0, m_selected_device_idx = 0;
  std::vector<Thing_idx> m_devices;
  std::vector<CommandState> m_command_state_list{CommandState{}};
  GameContext m_context;
  std::uint64_t m_tick_count = 0;
  std::vector<std::string> m_level_list, m_blueprints, m_animation_list,
      m_sound_list;
  std::map<Thing_idx, std::vector<DacPoint>> m_draw_lists;
  decltype(game_context_info) m_context_info = game_context_info;
  std::string m_current_game = "autosave";
  bool m_editor_mode = true;
  Thing_idx m_entity_selected_index = 0;
  FrameCache m_frame_cache;
  bool m_edit = true;
  double m_delta_time = 0, m_autosave_elapsed = 0;
  bool m_autosave_enabled = false;
  bool m_never_played = true, m_no_sound = false;
  int m_helios_count = 0;
  ResourcePaths m_paths;
  std::vector<SoundEvent> m_sound_events;
  std::vector<std::string> m_logs, m_errors;
  std::mt19937 m_random{0x43484945};
  std::uint64_t m_world_revision = 0;
  explicit Game(std::filesystem::path resources = "resources",
                int device_count = 1);
  void reset_game(int device_count = -1);
  void update(const Input &input, double delta);
  void update(const std::vector<Input> &inputs, double delta);
  void start_game(bool reload = true);
  void switch_mode();
  void apply_deferred_reparents();
  void queue_sound(std::string name, std::string asset, bool loop = false,
                   double gain = .3);
  void stop_sound(const std::string &name = {});
  void send_error(const std::string &error);
  void log(const std::string &message);
};
Thing_idx add_device(Game &game, int device_index = -1);
void switch_device_to_scene(Game &game, Thing &device, SceneType type);
void switch_game_to_scene(Game &game, SceneType type);
void create_game_for_device(Game &game, Thing_idx device);
void create_projection_for_device(Game &game, Thing_idx device);
void create_scaling_for_device(Game &game, Thing_idx device);
void check_position_of_things(Game &game, Thing &device, Thing &scene);
} // namespace chienpo
