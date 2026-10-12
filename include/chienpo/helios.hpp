#pragma once
#include "chienpo/game.hpp"
namespace chienpo {
// Explicit lifetime keeps the adapter a plain struct. Call helios_close before
// destroying it. The SDK is optional: preview/editor work without a DAC.
struct HeliosOutput {
  void *implementation = nullptr;
  int device_count = 0;
  std::vector<std::string> device_names;
  std::string error;
};
bool helios_open(HeliosOutput &output,
                 const std::filesystem::path &library = {});
int helios_rescan(HeliosOutput &output);
void helios_submit(HeliosOutput &output, Game &game);
void helios_stop(HeliosOutput &output);
void helios_close(HeliosOutput &output);
} // namespace chienpo
