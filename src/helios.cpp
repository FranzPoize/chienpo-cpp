#include "chienpo/helios.hpp"
#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <memory>
#include <set>

namespace chienpo {
namespace {
// C ABI from the user-supplied sdk/cpp/shared_library/HeliosDacAPI.h.
// Do not include that header: it defines global SDK controller storage.
struct SdkPoint {
  std::uint16_t x, y;
  std::uint8_t r, g, b, i;
};
static_assert(sizeof(SdkPoint) == 8, "Helios WriteFrame ABI");
struct Api {
  void *library = nullptr;
  int (*open)() = nullptr;
  int (*rescan)() = nullptr;
  int (*close)() = nullptr;
  int (*status)(unsigned) = nullptr;
  int (*is_closed)(unsigned) = nullptr;
  int (*stop)(unsigned) = nullptr;
  int (*name)(unsigned, char *) = nullptr;
  int (*write)(unsigned, int, std::uint8_t, SdkPoint *, int) = nullptr;
  unsigned (*max_rate)(unsigned) = nullptr;
  unsigned (*min_rate)(unsigned) = nullptr;
  unsigned (*max_points)(unsigned) = nullptr;
  std::set<int> running;
  std::vector<SdkPoint> buffer;
};
template <class T> bool bind(Api &a, T &f, const char *symbol) {
  f = reinterpret_cast<T>(dlsym(a.library, symbol));
  return f != nullptr;
}
void names(HeliosOutput &out, Api &a) {
  out.device_names.clear();
  for (int i = 0; i < out.device_count; ++i) {
    char name[32]{};
    if (a.name(i, name) < 0)
      out.device_names.push_back("Helios " + std::to_string(i));
    else
      out.device_names.emplace_back(name, std::find(name, name + 32, '\0'));
  }
}
double unit(double x) { return std::isfinite(x) ? std::clamp(x, 0., 1.) : 0.; }
std::uint8_t channel(double x) {
  return std::uint8_t(std::floor(unit(x) * 255));
}
std::uint16_t coordinate(double x) {
  return std::uint16_t(std::floor((1 - unit(x)) * 4095));
}
} // namespace
bool helios_open(HeliosOutput &out, const std::filesystem::path &requested) {
  helios_close(out);
  out.error.clear();
  auto a = std::make_unique<Api>();
  std::vector<std::filesystem::path> paths;
  if (!requested.empty())
    paths.push_back(requested);
  else {
    std::error_code ec;
    auto executable = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
      paths.push_back(executable.parent_path() / "libHeliosDACAPI.so");
      paths.push_back(executable.parent_path() / "../lib/libHeliosDACAPI.so");
    }
#ifdef CHIENPO_HELIOS_LIBRARY_PATH
    paths.emplace_back(CHIENPO_HELIOS_LIBRARY_PATH);
#endif
    paths.emplace_back("libHeliosDACAPI.so");
  }
  std::string loader_error;
  for (const auto &path : paths) {
    a->library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (a->library)
      break;
    const char *error = dlerror();
    if (error)
      loader_error = error;
  }
  if (!a->library) {
    out.error = "Helios SDK unavailable: " + loader_error;
    return false;
  }
  bool valid =
      bind(*a, a->open, "OpenDevices") && bind(*a, a->close, "CloseDevices") &&
      bind(*a, a->status, "GetStatus") && bind(*a, a->stop, "Stop") &&
      bind(*a, a->name, "GetName") && bind(*a, a->write, "WriteFrame") &&
      bind(*a, a->max_rate, "GetMaxSampleRate") &&
      bind(*a, a->min_rate, "GetMinSampleRate") &&
      bind(*a, a->max_points, "GetMaxFrameSize");
  if (!valid) {
    out.error = "Helios SDK missing required custom SDK API symbols";
    dlclose(a->library);
    return false;
  }
  bind(*a, a->rescan, "ReScanDevices");
  bind(*a, a->is_closed, "GetIsClosed");
  int count = a->open();
  if (count < 0) {
    out.error = "Helios OpenDevices failed: " + std::to_string(count);
    a->close();
    dlclose(a->library);
    return false;
  }
  out.device_count = count;
  names(out, *a);
  out.implementation = a.release();
  return true;
}
void helios_stop(HeliosOutput &out) {
  auto *a = static_cast<Api *>(out.implementation);
  if (!a)
    return;
  for (int i : a->running)
    a->stop(i);
  a->running.clear();
}
void helios_close(HeliosOutput &out) {
  auto *a = static_cast<Api *>(out.implementation);
  if (a) {
    helios_stop(out);
    a->close();
    dlclose(a->library);
    delete a;
  }
  out.implementation = nullptr;
  out.device_count = 0;
  out.device_names.clear();
}
int helios_rescan(HeliosOutput &out) {
  if (!out.implementation)
    return helios_open(out) ? out.device_count : -1;
  auto &a = *static_cast<Api *>(out.implementation);
  helios_stop(out);
  int count;
  if (a.rescan)
    count = a.rescan();
  else {
    a.close();
    count = a.open();
  }
  if (count < 0) {
    out.error = "Helios rescan failed: " + std::to_string(count);
    return count;
  }
  out.device_count = count;
  names(out, a);
  out.error.clear();
  return count;
}
void helios_submit(HeliosOutput &out, Game &game) {
  game.m_helios_count = out.device_count;
  auto *a = static_cast<Api *>(out.implementation);
  if (!a)
    return;
  std::set<int> assigned;
  std::string failure;
  for (auto idx : game.m_devices) {
    auto &device = game.m_things.get(idx);
    int dac = device.m_helios_index;
    if (!device.m_device_on || dac < 0 || dac >= out.device_count)
      continue;
    if (!assigned.insert(dac).second) {
      failure =
          "Multiple virtual devices assigned to Helios " + std::to_string(dac);
      continue;
    }
    if (a->is_closed && a->is_closed(dac) != 0) {
      failure =
          "Helios " + std::to_string(dac) + " disconnected; rescan devices";
      continue;
    }
    auto frame = game.m_draw_lists.find(idx);
    if (frame == game.m_draw_lists.end() || frame->second.empty()) {
      if (a->running.erase(dac))
        a->stop(dac);
      continue;
    }
    unsigned max_points = a->max_points(dac), max_rate = a->max_rate(dac),
             min_rate = a->min_rate(dac);
    if (max_points == 0 || max_points > 1000000 || max_rate == 0 ||
        min_rate > max_rate) {
      failure = "Invalid Helios limits for device " + std::to_string(dac);
      if (a->running.erase(dac))
        a->stop(dac);
      continue;
    }
    // Never truncate a frame: truncation can remove blanked travel points and
    // leave a lit beam at an unintended endpoint. Report oversize frames so the
    // operator can reduce tightness/detail or use a DAC with a larger buffer.
    if (frame->second.size() > max_points) {
      failure = "Helios " + std::to_string(dac) + ": " +
                std::to_string(frame->second.size()) +
                " points exceeds frame limit " + std::to_string(max_points) +
                "; reduce tightness/detail";
      if (a->running.erase(dac))
        a->stop(dac);
      continue;
    }
    int ready = a->status(dac);
    if (ready != 1) {
      if (ready < 0)
        failure = "Helios status failed: " + std::to_string(ready);
      continue;
    }
    a->buffer.clear();
    a->buffer.reserve(frame->second.size());
    double intensity = unit(device.m_intensity);
    for (auto p : frame->second) {
      bool finite = std::isfinite(p.x) && std::isfinite(p.y);
      if (!finite)
        p = {.5, .5, 0, 0, 0};
      a->buffer.push_back({coordinate(p.x), coordinate(p.y),
                           channel(p.r * intensity), channel(p.g * intensity),
                           channel(p.b * intensity), channel(intensity)});
    }
    double desired = a->buffer.size() * std::max(1., device.m_fps);
    int rate = int(std::clamp(desired, double(min_rate), double(max_rate)));
    // Original server uses DONT_BLOCK (4); ONCE (2) additionally avoids stale
    // repeating output when the native loop stops providing fresh frames.
    int result =
        a->write(dac, rate, 6, a->buffer.data(), int(a->buffer.size()));
    if (result < 0) {
      failure = "Helios WriteFrame failed: " + std::to_string(result);
      if (a->running.erase(dac))
        a->stop(dac);
    } else
      a->running.insert(dac);
  }
  for (auto it = a->running.begin(); it != a->running.end();) {
    if (!assigned.count(*it)) {
      a->stop(*it);
      it = a->running.erase(it);
    } else
      ++it;
  }
  out.error = std::move(failure);
}
} // namespace chienpo
