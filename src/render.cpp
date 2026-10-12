#include "chienpo/render.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace chienpo {
namespace {
constexpr double pi = 3.14159265358979323846;
Color hsv(double h) {
  h = std::fmod(h, 360.);
  if (h < 0)
    h += 360;
  double x = 1 - std::abs(std::fmod(h / 60, 2.) - 1);
  if (h < 60)
    return {1, x, 0};
  if (h < 120)
    return {x, 1, 0};
  if (h < 180)
    return {0, 1, x};
  if (h < 240)
    return {0, x, 1};
  if (h < 300)
    return {x, 0, 1};
  return {1, 0, x};
}
bool blank(const DacPoint &p) { return p.r == 0 && p.g == 0 && p.b == 0; }
Vec2 pos(const Thing &t) { return {t.x, t.y}; }
Vec2 pos(const DacPoint &t) { return {t.x, t.y}; }
Color col(const DacPoint &t) { return {t.r, t.g, t.b}; }
struct Pen {
  Thing &device;
  Frame &points;
  void raw(Vec2 p, Color c) { points.push_back({p.x, p.y, c.r, c.g, c.b}); }
  void wait(Vec2 p, Color c) {
    for (int i = 0; i < std::clamp(std::ceil(device.m_tightness), 0., 100.);
         ++i)
      raw(p, c);
  }
  void line(Vec2 p, Color c) {
    if (points.empty())
      raw(p, c);
    Vec2 a = pos(points.back());
    const int n = std::clamp(
        int(std::ceil(device.m_tightness / 4 *
                      (std::hypot(p.x - a.x, p.y - a.y) < .05 ? 6 : 24))),
        2, 4096);
    for (int i = 0; i < n; ++i) {
      double x = double(i) / n;
      double t = i == n - 1 ? 1 : x * x * (2 - x * x);
      raw({a.x * (1 - t) + p.x * t, a.y * (1 - t) + p.y * t}, c);
    }
  }
  void move(Vec2 p) { line(p, device.m_blanking_color); }
  void segment(Vec2 a, Vec2 b, Color c) {
    move(a);
    wait(a, device.m_blanking_color);
    wait(a, c);
    line(b, c);
    wait(b, c);
    wait(b, device.m_blanking_color);
  }
  void box(const std::array<Vec2, 4> &v, Color c) {
    move(v[0]);
    wait(v[0], device.m_blanking_color);
    for (int i = 1; i <= 4; ++i) {
      line(v[i % 4], c);
      wait(v[i % 4], c);
    }
    wait(v[0], device.m_blanking_color);
  }
  void rect(Vec2 p, double w, double h, Color c) {
    box({{{p.x - w / 2, p.y + h / 2},
          {p.x - w / 2, p.y - h / 2},
          {p.x + w / 2, p.y - h / 2},
          {p.x + w / 2, p.y + h / 2}}},
        c);
  }
  void cross(Vec2 p, Color c = {0, 1, 1}) {
    segment({p.x - .01, p.y - .01}, {p.x + .01, p.y + .01}, c);
    segment({p.x - .01, p.y + .01}, {p.x + .01, p.y - .01}, c);
  }
  void diamond(Vec2 p, Color c = {1, .4, 0}) {
    Vec2 a{p.x, p.y - .01};
    move(a);
    wait(a, device.m_blanking_color);
    wait(a, c);
    raw(a, c);
    raw({p.x + .01, p.y}, c);
    raw({p.x, p.y + .01}, c);
    raw({p.x - .01, p.y}, c);
    raw(a, c);
    wait(a, c);
    wait(a, device.m_blanking_color);
  }
  void circle(Vec2 p, double r, int segments, Color c, double angle = 0) {
    Vec2 a{p.x + r * std::cos(angle), p.y + r * std::sin(angle)};
    move(a);
    wait(a, device.m_blanking_color);
    wait(a, c);
    for (int i = 1; i <= segments; ++i) {
      double t = angle + 2 * pi * i / segments;
      line({p.x + r * std::cos(t), p.y + r * std::sin(t)}, c);
    }
    wait(a, c);
    wait(a, device.m_blanking_color);
  }
};
std::array<Vec2, 4> corners(const Thing &t) {
  const auto x1=t.x+t.xmin*t.m_scale, x2=t.x+t.xmax*t.m_scale;
  const auto y1=t.y+t.ymin*t.m_scale, y2=t.y+t.ymax*t.m_scale;
  const auto left=std::min(x1,x2), right=std::max(x1,x2);
  const auto top=std::min(y1,y2), bottom=std::max(y1,y2);
  return {{{left,top},{right,top},{right,bottom},{left,bottom}}};
}
Vec2 rotated(const Thing &t, double x, double y) {
  return {t.x + x * std::cos(t.m_angle) - y * std::sin(t.m_angle),
          t.y + x * std::sin(t.m_angle) + y * std::cos(t.m_angle)};
}
Vec2 portal_center(const Thing &t) {
  return {t.x + t.xmax * t.m_scale / 2, t.y + t.ymax * t.m_scale / 2};
}
void sprite(Game &g, Pen &pen, Thing &t, bool editor) {
  auto frame = get_current_frame(g, t);
  if (frame.empty())
    return;
  for (auto &p : frame) {
    p.x = (editor || !t.m_facing ? p.x : -p.x) * t.m_scale + t.x +
          t.m_sprite_offset_x + (t.xmax - t.xmin) * t.m_scale / 2;
    p.y = p.y * t.m_scale + t.y + t.m_sprite_offset_y +
          (t.ymax - t.ymin) * t.m_scale / 2;
  }
  frame.push_back(frame.front());
  pen.move(pos(frame.front()));
  pen.wait(pos(frame.front()), col(frame.front()));
  pen.points.insert(pen.points.end(), frame.begin(), frame.end());
  pen.wait(pos(frame.back()), pen.device.m_blanking_color);
}
void moving_platform(Pen &pen, const Thing &t) {
  auto v = corners(t);
  Color green{0, 1, 0};
  Vec2 a{v[3].x + .01, v[3].y};
  pen.move(a);
  pen.wait(a, {});
  pen.wait(a, {});
  pen.wait(a, green);
  pen.line({v[3].x, v[3].y - .01}, green);
  pen.line({v[0].x, v[0].y + .01}, green);
  a = {v[0].x + .01, v[0].y};
  pen.line(a, green);
  for (int i = 0;
       i < std::min(4096, int(std::ceil(std::abs(t.xmax - t.xmin) / .03)) + 1);
       ++i) {
    a.x = std::min(a.x + .03, v[1].x - .01);
    pen.line(a, i % 2 ? Color{.3, .3, 1} : Color{1, 0, 0});
  }
  pen.line({v[1].x, v[1].y + .01}, green);
  pen.line({v[2].x, v[2].y - .01}, green);
  a = {v[2].x - .01, v[2].y};
  pen.line(a, green);
  pen.wait(a, green);
  pen.wait(a, {});
}
void laser_projectile(Pen &pen, const Thing &t) {
  int folds = std::clamp(int(t.m_laser_proj_fold), 2, 1024);
  double size = t.m_laser_proj_size;
  if (std::abs(size) < 1e-12)
    return;
  for (int i = 0; i < folds * (t.m_laser_ondulating ? 4 : 1); ++i) {
    double x = double(i) / (folds - 1) / (t.m_laser_ondulating ? 4 : 1),
           width =
               (std::cos(x * 2 * pi + pi) + 1) * t.m_laser_proj_scale_y * size;
    double xp = t.x - size / 2 + x * size,
           yp = t.m_laser_ondulating
                    ? width * std::cos(xp / size * folds / 4 * pi + pi / 2) / 6
                    : width * (i % 2 ? 1 : -1) / 4;
    Vec2 p = rotated(t, -size / 2 + x * size, yp);
    Color c = hsv((t.m_laser_ondulating_color ? xp / (size * 2) : x) * 360);
    if (i == 0) {
      pen.move(p);
      pen.wait(p, pen.device.m_blanking_color);
    }
    pen.line(p, c);
  }
}
void entity(Game &g, Pen &pen, Thing &t, bool editor) {
  if (!t.m_show && !editor)
    return;
  const auto start = pen.points.size();
  switch (t.m_kind) {
  case EntityType::player:
  case EntityType::neon_boss:
  case EntityType::decor:
    sprite(g, pen, t, editor);
    break;
  case EntityType::portal:
    if (editor) {
      for (auto &source : g.m_things)
        if (source.m_portal_destination_idx == t.m_idx &&
            source.m_idx == g.m_entity_selected_index) {
          pen.circle(portal_center(t), .05, 20, {1, 0, 1});
          break;
        }
    }
    sprite(g, pen, t, editor);
    break;
  case EntityType::platform: {
    Color c{.2, .7, 1};
    const auto bounds=corners(t);
    Vec2 a=bounds[0], b=bounds[1];
    pen.move(a);
    pen.wait(a, pen.device.m_blanking_color);
    pen.wait(a, c);
    pen.line(b, c);
    pen.wait(b, c);
    break;
  }
  case EntityType::move_platform:
    moving_platform(pen, t);
    if (editor)
      for (std::size_t i = 1; i < t.m_path_point.size(); ++i) {
        pen.move(t.m_path_point[i - 1]);
        pen.wait(t.m_path_point[i - 1], {0, .8, 1});
        pen.line(t.m_path_point[i], {0, .8, 1});
      }
    break;
  case EntityType::masking_box:
    pen.box(corners(t), {1, 0, 0});
    break;
  case EntityType::spawner:
    if (editor)
      pen.cross(pos(t));
    break;
  case EntityType::trigger:
    if (editor)
      pen.diamond(pos(t), {1, .5, 1});
    break;
  case EntityType::laser: {
    Vec2 a = rotated(t, .03, 0);
    if (editor)
      pen.segment(a, pos(t), {.8, .8, .3});
    pen.move(a);
    if (t.m_laser_delay_alarm > 0 &&
        t.m_attack_tick > t.m_laser_delay_attack - t.m_laser_delay_alarm) {
      int phase =
          std::clamp(int(std::floor((t.m_attack_tick - t.m_laser_delay_attack +
                                     t.m_laser_delay_alarm) /
                                    (t.m_laser_delay_alarm / 4.5))),
                     0, 3);
      const Color colors[] = {
          {0, .5, .2}, {.33, .4, .2}, {.66, .3, .2}, {1, .2, .2}};
      pen.circle(rotated(t, .06, 0), .01 * (phase + 1), 5, colors[phase],
                 std::uniform_real_distribution<double>(0, 2 * pi)(g.m_random));
    }
    break;
  }
  case EntityType::laser_projectile:
    laser_projectile(pen, t);
    break;
  case EntityType::boss_projectile: {
    int a = (t.m_trail_index + 9) % 10, b = (t.m_trail_index + 2) % 10;
    pen.segment({t.m_trail[a * 2], t.m_trail[a * 2 + 1]},
                {t.m_trail[b * 2], t.m_trail[b * 2 + 1]}, {1, .4, .4});
    break;
  }
  default:
    break;
  }
  if (editor) {
    t.m_intensity_modifier = t.m_idx == g.m_entity_selected_index
                                 ? g.m_context.m_selected_intensity
                                 : g.m_context.m_unselected_intensity;
    for (std::size_t i = start; i < pen.points.size(); ++i) {
      auto &p = pen.points[i];
      p.r *= t.m_intensity_modifier;
      p.g *= t.m_intensity_modifier;
      p.b *= t.m_intensity_modifier;
    }
  }
}
// Split every segment at each box boundary. This also handles a segment whose
// endpoints both lie outside a mask, preserving blanking across the interior.
void clip(Frame &points, Box b, bool keep_inside) {
  if (points.empty())
    return;
  Frame out;
  out.reserve(points.size() * 2);
  auto visible = [&](Vec2 p) {
    bool inside = p.x > b.xmin && p.x < b.xmax && p.y > b.ymin && p.y < b.ymax;
    return keep_inside ? (p.x >= b.xmin && p.x <= b.xmax && p.y >= b.ymin &&
                          p.y <= b.ymax)
                       : !inside;
  };
  auto first = points.front();
  if (!visible(pos(first)))
    first.r = first.g = first.b = 0;
  out.push_back(first);
  for (std::size_t i = 1; i < points.size(); ++i) {
    const auto a = points[i - 1], z = points[i];
    std::vector<double> cuts{0, 1};
    auto cut = [&](double t) {
      if (t > 0 && t < 1 && std::isfinite(t))
        cuts.push_back(t);
    };
    if (a.x != z.x) {
      cut((b.xmin - a.x) / (z.x - a.x));
      cut((b.xmax - a.x) / (z.x - a.x));
    }
    if (a.y != z.y) {
      cut((b.ymin - a.y) / (z.y - a.y));
      cut((b.ymax - a.y) / (z.y - a.y));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    auto point = [&](double t) {
      auto p = z;
      p.x = a.x + (z.x - a.x) * t;
      p.y = a.y + (z.y - a.y) * t;
      return p;
    };
    for (std::size_t k = 1; k < cuts.size(); ++k) {
      bool show = visible(pos(point((cuts[k - 1] + cuts[k]) / 2))) && !blank(z);
      auto p = point(cuts[k]);
      if (!show)
        p.r = p.g = p.b = 0;
      if (k > 1) {
        auto edge = point(cuts[k - 1]);
        if (!show)
          edge.r = edge.g = edge.b = 0;
        out.push_back(edge);
      }
      out.push_back(p);
    }
  }
  points.swap(out);
}
void calibration_drawing(Pen &pen, const Thing &scene) {
  auto v = scene.m_cursors;
  if (scene.m_scene_type == SceneType::scaling) {
    double x = scene.m_horiz_scaling, y = scene.m_vert_scaling;
    v = {{{.5 + x, .5 + y},
          {.5 - x, .5 + y},
          {.5 - x, .5 - y},
          {.5 + x, .5 - y}}};
  }
  pen.box(v, {0, 1, 0});
  if (scene.m_scene_type == SceneType::projection)
    pen.rect(v[std::clamp(scene.m_cursor_idx, 0, 3)], .01, .01, {0, 1, 0});
  else
    pen.rect({.5, .5}, (v[1].x - v[0].x) / .2 * .03,
             (v[2].y - v[0].y) / .2 * .03, {1, .3, 1});
  auto mid = [](Vec2 a, Vec2 b) {
    return Vec2{(a.x + b.x) / 2, (a.y + b.y) / 2};
  };
  pen.segment(mid(v[0], v[1]), mid(v[2], v[3]), {0, 1, 1});
  pen.segment(mid(v[0], v[3]), mid(v[1], v[2]), {0, 1, 1});
  if (scene.m_scene_type == SceneType::scaling)
    apply_perspective_transform(pen.points, pen.device.m_distortion_matrix);
}
} // namespace

Frames load_ilda(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Cannot open ILDA: " + path.string());
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
  Frames result;
  std::array<Color, 256> palette{};
  // Suggested 64-color ILDA palette; custom format-2 palettes replace these.
  for (int i = 0; i < 256; ++i) {
    int n = i % 64;
    if (n <= 15)
      palette[i] = {1, n / 15., 0};
    else if (n <= 23)
      palette[i] = {n == 16 ? 1 : (24 - n) * 2 / 15., 1, 0};
    else if (n <= 31)
      palette[i] = {0, 1, (n - 24) * 2 / 15.};
    else if (n <= 39) {
      const int values[] = {8, 7, 6, 6, 5, 4, 4, 2};
      palette[i] = {0, values[n - 32] / 15., 1};
    } else if (n <= 47)
      palette[i] = {(n - 40) * 2 / 15., 0, 1};
    else if (n <= 55)
      palette[i] = {1, (n - 48) * 2 / 15., 1};
    else if (n == 56)
      palette[i] = {1, 1, 1};
    else
      palette[i] = {n == 63 ? 0. : 1., (64 - n) * 2 / 15., (64 - n) * 2 / 15.};
  }
  auto u16 = [&](std::size_t p) {
    return unsigned(bytes[p]) * 256 + bytes[p + 1];
  };
  auto s16 = [&](std::size_t p) {
    unsigned n = u16(p);
    return n >= 32768 ? int(n) - 65536 : int(n);
  };
  for (std::size_t p = 0; p < bytes.size();) {
    if (bytes.size() - p < 32)
      throw std::runtime_error("Truncated ILDA header: " + path.string());
    if (bytes[p] != 'I' || bytes[p + 1] != 'L' || bytes[p + 2] != 'D' ||
        bytes[p + 3] != 'A')
      throw std::runtime_error("Invalid ILDA signature: " + path.string());
    unsigned format = bytes[p + 7], count = u16(p + 24);
    p += 32;
    if (count == 0)
      break;
    int stride = format == 0   ? 8
                 : format == 1 ? 6
                 : format == 2 ? 3
                 : format == 4 ? 10
                 : format == 5 ? 8
                               : 0;
    if (!stride)
      throw std::runtime_error("Unsupported ILDA format " +
                               std::to_string(format));
    if (count > (bytes.size() - p) / stride)
      throw std::runtime_error("Truncated ILDA records: " + path.string());
    if (format == 2) {
      for (unsigned i = 0; i < count && i < 256; ++i)
        palette[i] = {bytes[p + i * 3] / 255., bytes[p + i * 3 + 1] / 255.,
                      bytes[p + i * 3 + 2] / 255.};
    } else {
      Frame f;
      f.reserve(count);
      for (unsigned i = 0; i < count; ++i) {
        std::size_t q = p + i * stride;
        bool three = format == 0 || format == 4;
        std::size_t status = q + (three ? 6 : 4);
        Color c{};
        if (!(bytes[status] & 0x40)) {
          if (format == 4 || format == 5)
            c = {bytes[status + 3] / 255., bytes[status + 2] / 255.,
                 bytes[status + 1] / 255.};
          else
            c = palette[bytes[status + 1]];
        }
        f.push_back({s16(q) / 32768., -s16(q + 2) / 32768., c.r, c.g, c.b});
      }
      result.push_back(std::move(f));
    }
    p += count * stride;
  }
  return result;
}
const Frames &get_from_frame_cache(Game &g, const std::string &path) {
  auto &cache = g.m_frame_cache.m_frame_cache;
  auto found = cache.find(path);
  if (found != cache.end())
    return found->second;
  Frames frames;
  try {
    frames = load_ilda(g.m_paths.animations / path);
  } catch (const std::exception &e) {
    g.send_error(e.what());
  }
  return cache.emplace(path, std::move(frames)).first->second;
}
void load_animation_list(Game &g, const AnimationPathMap &paths) {
  for (const auto &[state, list] : paths)
    for (const auto &a : list)
      get_from_frame_cache(g, a.path);
}
void add_animation_to_thing(Game &g, Thing &t, const std::string &state,
                            const std::string &path, double ticks, bool loop) {
  t.m_animation_path_map[state].push_back({path, loop, std::max(.001, ticks)});
  get_from_frame_cache(g, path);
  if (t.m_animation_state.empty())
    change_animation_state(t, state);
}
void change_animation_on_thing(Game &g, Thing &t, const std::string &state,
                               std::size_t index, const std::string &path,
                               double ticks, bool loop) {
  auto &list = t.m_animation_path_map[state];
  if (index >= list.size())
    return;
  list[index] = {path, loop, std::max(.001, ticks)};
  get_from_frame_cache(g, path);
}
void remove_animation_from_thing(Thing &t, const std::string &state,
                                 std::size_t index) {
  auto i = t.m_animation_path_map.find(state);
  if (i == t.m_animation_path_map.end() || index >= i->second.size())
    return;
  i->second.erase(i->second.begin() + index);
  t.m_animation_sprite_sequence = 0;
  t.m_animation_tick_count = 0;
}
void advance_animation(Game &g, Thing &t, double advance) {
  t.m_animation_tick_count += advance * g.m_delta_time * 60;
}
void change_animation_state(Thing &t, const std::string &state) {
  t.m_animation_state = state;
  t.m_animation_tick_count = 0;
  t.m_animation_sprite_sequence = 0;
  t.m_animation_ended = false;
}
double get_animation_state_duration(Game &g, Thing &t,
                                    const std::string &state) {
  double total = 0;
  auto it = t.m_animation_path_map.find(state);
  if (it == t.m_animation_path_map.end())
    return 0;
  for (auto &a : it->second)
    total += get_from_frame_cache(g, a.path).size() *
             std::max(.001, a.m_tick_per_frame);
  return total / ((g.m_delta_time > 0 ? g.m_delta_time : 1. / 60) * 60);
}
Frame get_current_frame(Game &g, Thing &t) {
  t.m_animation_ended = false;
  auto it = t.m_animation_path_map.find(t.m_animation_state);
  if (it == t.m_animation_path_map.end() || it->second.empty())
    return {};
  auto &sequence = it->second;
  t.m_animation_sprite_sequence =
      std::clamp(t.m_animation_sprite_sequence, 0, int(sequence.size() - 1));
  for (std::size_t attempts = 0; attempts < sequence.size(); ++attempts) {
    auto &a = sequence[t.m_animation_sprite_sequence];
    const auto &f = get_from_frame_cache(g, a.path);
    double ticks = std::max(.001, a.m_tick_per_frame);
    std::size_t index =
        std::size_t(std::max(0., std::floor(t.m_animation_tick_count / ticks)));
    if (f.empty()) {
      if (std::size_t(t.m_animation_sprite_sequence + 1) < sequence.size()) {
        ++t.m_animation_sprite_sequence;
        t.m_animation_tick_count = 0;
        continue;
      }
      t.m_animation_ended = true;
      return {};
    }
    if (a.m_loop)
      return f[index % f.size()];
    if (index >= f.size() &&
        std::size_t(t.m_animation_sprite_sequence + 1) < sequence.size()) {
      t.m_animation_tick_count -= f.size() * ticks;
      ++t.m_animation_sprite_sequence;
      continue;
    }
    index = std::min(index, f.size() - 1);
    t.m_animation_ended = index == f.size() - 1;
    return f[index];
  }
  return {};
}
TransformMatrix multiply_matrix(const TransformMatrix &a,
                                const TransformMatrix &b) {
  TransformMatrix r{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k)
        r[i][j] += a[i][k] * b[k][j];
  return r;
}
Vec2 apply_projection_matrix(const TransformMatrix &m, Vec2 p) {
  double w = m[2][0] * p.x + m[2][1] * p.y + m[2][2];
  if (std::abs(w) < 1e-12)
    return {std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()};
  return {(m[0][0] * p.x + m[0][1] * p.y + m[0][2]) / w,
          (m[1][0] * p.x + m[1][1] * p.y + m[1][2]) / w};
}
void apply_perspective_transform(std::vector<DacPoint> &points,
                                 const TransformMatrix &m) {
  for (auto &p : points) {
    auto v = apply_projection_matrix(m, {p.x, p.y});
    if (!std::isfinite(v.x) || !std::isfinite(v.y)) {
      p = {.5, .5, 0, 0, 0};
    } else {
      p.x = v.x;
      p.y = v.y;
    }
  }
}
TransformMatrix compute_projection(const std::array<Vec2, 4> &v) {
  double a[8][9]{};
  const Vec2 source[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  for (int i = 0; i < 4; ++i) {
    double x = source[i].x, y = source[i].y, u = v[i].x, w = v[i].y;
    a[2 * i][0] = x;
    a[2 * i][1] = y;
    a[2 * i][2] = 1;
    a[2 * i][6] = -u * x;
    a[2 * i][7] = -u * y;
    a[2 * i][8] = u;
    a[2 * i + 1][3] = x;
    a[2 * i + 1][4] = y;
    a[2 * i + 1][5] = 1;
    a[2 * i + 1][6] = -w * x;
    a[2 * i + 1][7] = -w * y;
    a[2 * i + 1][8] = w;
  }
  for (int k = 0; k < 8; ++k) {
    int pivot = k;
    for (int i = k + 1; i < 8; ++i)
      if (std::abs(a[i][k]) > std::abs(a[pivot][k]))
        pivot = i;
    if (std::abs(a[pivot][k]) < 1e-12)
      throw std::runtime_error("Projection corners are degenerate");
    for (int j = 0; j < 9; ++j)
      std::swap(a[k][j], a[pivot][j]);
    double scale = a[k][k];
    for (int j = k; j < 9; ++j)
      a[k][j] /= scale;
    for (int i = 0; i < 8; ++i)
      if (i != k) {
        double scale2 = a[i][k];
        for (int j = k; j < 9; ++j)
          a[i][j] -= scale2 * a[k][j];
      }
  }
  return {{{a[0][8], a[1][8], a[2][8]},
           {a[3][8], a[4][8], a[5][8]},
           {a[6][8], a[7][8], 1}}};
}
void update_calibration(Game &g, Thing &scene, const CommandState &c) {
  auto &device = g.m_things.get(scene.parent_idx);
  double speed =
      g.m_context.m_cursor_speed * g.m_delta_time * (c.shift.pressed ? .05 : 1);
  if (scene.m_scene_type == SceneType::projection) {
    if (c.next.positive_edge)
      scene.m_cursor_idx = (scene.m_cursor_idx + 1) % 4;
    if (c.prev.positive_edge)
      scene.m_cursor_idx = (scene.m_cursor_idx + 3) % 4;
    auto &v = scene.m_cursors[std::clamp(scene.m_cursor_idx, 0, 3)];
    v.x += (int(c.right.pressed) - int(c.left.pressed)) * speed *
           (device.m_invert_x ? -1 : 1);
    v.y += (int(c.down.pressed) - int(c.up.pressed)) * speed;
    if (c.confirm.positive_edge) {
      try {
        device.m_distortion_matrix = compute_projection(scene.m_cursors);
        device.m_cursors = scene.m_cursors;
        switch_device_to_scene(g, device, SceneType::scaling);
      } catch (const std::exception &e) {
        g.send_error(e.what());
      }
    }
  } else if (scene.m_scene_type == SceneType::scaling) {
    scene.m_horiz_scaling = std::max(
        .0001, scene.m_horiz_scaling +
                   (int(c.right.pressed) - int(c.left.pressed)) * speed);
    scene.m_vert_scaling =
        std::max(.0001, scene.m_vert_scaling +
                            (int(c.up.pressed) - int(c.down.pressed)) * speed);
    if (c.confirm.positive_edge) {
      device.m_base_scale_x = scene.m_horiz_scaling * 10;
      device.m_base_scale_y = scene.m_vert_scaling * 10;
      device.m_scaling_matrix = {{{device.m_base_scale_x, 0, 0},
                                  {0, device.m_base_scale_y, 0},
                                  {0, 0, 1}}};
      device.m_world_to_view_matrix =
          multiply_matrix(device.m_distortion_matrix, device.m_scaling_matrix);
      switch_device_to_scene(g, device, SceneType::editor);
    }
  }
}
void build_draw_lists(Game &g) {
  g.m_draw_lists.clear();
  for (auto idx : g.m_devices) {
    auto &d = g.m_things.get(idx);
    if (d.m_kind != EntityType::device)
      continue;
    auto &points = g.m_draw_lists[idx];
    Pen pen{d, points};
    auto &scene = get_current_scene(g.m_things, idx);
    if (scene.m_scene_type == SceneType::projection ||
        scene.m_scene_type == SceneType::scaling) {
      if (idx == g.m_selected_device_idx)
        calibration_drawing(pen, scene);
    } else if (scene.m_kind != EntityType::nil) {
      for (auto child : g.m_things.children(scene.m_idx, !g.m_editor_mode))
        entity(g, pen, g.m_things.get(child), g.m_editor_mode);
      auto &selected = g.m_things.get(g.m_entity_selected_index);
      if (g.m_editor_mode && idx == g.m_selected_device_idx &&
          selected.parent_idx == scene.m_idx && selected.m_active) {
        if ((selected.m_kind == EntityType::platform ||
             selected.m_kind == EntityType::masking_box) &&
            selected.m_selection_sub_index == 1)
          pen.diamond(corners(selected)[2]);
        if (selected.m_kind == EntityType::move_platform &&
            selected.m_selection_sub_index % 3 == 1)
          pen.diamond(corners(selected)[2]);
        if (selected.m_kind == EntityType::portal) {
          auto &target = g.m_things.get(selected.m_portal_destination_idx);
          if (target.m_kind == EntityType::portal &&
              target.parent_idx == scene.m_idx)
            pen.segment(portal_center(selected), portal_center(target),
                        {0, .5, 0});
        }
      }
      double sx = std::abs(d.m_base_scale_x) > 1e-12 ? d.m_base_scale_x : 1,
             sy = std::abs(d.m_base_scale_y) > 1e-12 ? d.m_base_scale_y : 1;
      clip(points, {0, 1 / sx, 0, 1 / sy}, true);
      for (auto child : g.m_things.children(scene.m_idx,true)) {
        auto &t = g.m_things.get(child);
        if (t.m_kind == EntityType::masking_box && !t.m_no_masking) {
          const auto bounds=corners(t);
          clip(points, {bounds[0].x,bounds[2].x,bounds[0].y,bounds[2].y},false);
        }
      }
      if (d.m_invert_x)
        for (auto &p : points)
          p.x = 1 / sx - p.x;
      apply_perspective_transform(points, d.m_world_to_view_matrix);
      if (d.m_show_boundary) {
        Frame bounds;
        Pen boundary{d, bounds};
        boundary.box({{{0, 0}, {1, 0}, {1, 1}, {0, 1}}}, {1, 0, 0});
        for (auto &p : bounds)
          p.x = 1 - p.x;
        apply_perspective_transform(bounds, d.m_distortion_matrix);
        points.insert(points.end(), bounds.begin(), bounds.end());
      }
    }
    if (points.empty())
      points.push_back({.5, .5, 0, 0, 0});
    d.m_point_count = int(points.size());
    d.m_draw_list = points;
  }
}
} // namespace chienpo
