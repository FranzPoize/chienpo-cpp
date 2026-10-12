# Chienpo C++

Native desktop port of `real-chienpo`, with the game, editor, ImGui interface,
sound playback and laser output in one process. There is no web server or browser
client. The original `Game` and array of 1,024 `Thing` structs remain the core
architecture; entities keep stable indices and circular scene hierarchies.

## Build and run

See [the build guide](docs/build.md) for Conan, dependencies and older Ubuntu
deployment. The existing Snacman checkout can supply the native dependencies:

```sh
python3 scripts/build.py --adnn-root /home/franz/gamedev \
  --toolchain /home/franz/gamedev/snacman/build-conan-gcc14.0/Release/generators/conan_toolchain.cmake
./build-native/chienpo
```

For the portable core tests, only a C++ compiler, CMake and Ninja are needed:

```sh
python3 scripts/build.py --headless
```

The desktop uses Snacman's `ad::graphics`, `ad::imguiui` and `ad::sounds` libraries.
The custom Helios SDK is included as source under `vendor/helios`, with its
licenses and provenance, and builds alongside the application. Game JSON uses
the small C library cJSON under `third_party/cjson`.

## Resources and new levels

The app starts with a fresh world. Old saved levels are deliberately excluded.
Reusable ILDA animations, blueprints and converted sounds are included, so runtime
does not require the original TypeScript checkout. New levels are written into
`resources/saved_levels`. See [asset provenance and import instructions](docs/assets.md).

Select a screen to calibrate its four projection corners, then confirm with Enter
to set the mapping scale. Confirm again to open a fresh game scene. The screen's
Game button also opens the editor directly. Use the entity palette and inspector
to build a level; Start saves the editing world before running it, and Editor
restores that saved world.

Arrows move, Space/X jumps or cycles editor points, Enter confirms, Escape cancels,
O/P selects entities, I/K changes scale, and F2 switches play/edit mode. The UI
includes animation, trigger, blueprint and sound editors. Projector selection and
laser output controls are separate from the on-screen preview.

For a short desktop check without hardware output:

```sh
./build-native/chienpo --no-audio --no-helios --frames 20
./build-native/chienpo --headless --frames 150
```

Use `--resources /path/to/resources` to choose a writable asset/level directory.
An installed application first looks in `../share/chienpo` beside its `bin` folder.

## Source layout

| File | Responsibility |
| --- | --- |
| `include/chienpo/types.hpp`, `src/types.cpp` | Structs, context, indexed pool, hierarchy and input edges |
| `src/game.cpp` | Game lifetime, scene flow, play/edit transitions and sound events |
| `src/gameplay.cpp` | Physics, entities, triggers and keyboard editing |
| `src/render.cpp` | ILDA, animations, vector drawing and calibration |
| `src/persistence.cpp` | JSON levels, blueprints and resource catalogs |
| `src/main.cpp`, `src/ui.cpp` | Native loop and direct ImGui controls |
| `src/audio.cpp` | Snacman sound adapter |
| `src/helios.cpp` | Custom Helios SDK adapter |
| `tests/core_tests.cpp` | Behavioral regression tests without a display or DAC |

The original source repositories are not modified by this port. For a distributable
binary, build all dependencies on the oldest supported Ubuntu baseline and test
there; a successful build on this workstation does not establish that compatibility.

Validation covers nine core regression suites, the native build and desktop
startup, OpenAL startup with its silent backend, all 53 ILDA files, and loading
the bundled SDK. Physical DAC output and audible playback require testing with
the actual hardware.
