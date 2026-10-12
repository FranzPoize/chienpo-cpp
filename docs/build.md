# Native C++ build

The native application uses Snacman's `ad::graphics`, `ad::imguiui` and
`ad::sounds` libraries: GLFW/OpenGL, Dear ImGui 1.89.8 and OpenAL. The upstream
sources build in this project's build directory; source checkouts are not edited.
CMake 3.23+, Ninja and a C++20 compiler are required. Persistence uses vendored
cJSON C code; nlohmann_json appears only as an upstream graphics dependency.

## Headless core and tests

No Conan, display server or sound device is needed for the core tests:

```sh
python3 scripts/build.py --headless
```

The result is `build-headless/chienpo_core_tests`. Native desktop and device
behavior still requires the native build and separate runtime validation.

## Conan and clean native builds

This project intentionally uses **Conan 1.66.0**, matching the Conan 1 recipes in
Snacman. Conan 2 is not a drop-in replacement. Use Python 3.10–3.12 in an isolated
virtual environment; don't replace the system Python or global Conan installation.

```sh
python3.12 -m venv .tools/conan1
.tools/conan1/bin/python -m pip install 'conan==1.66.0'
export CONAN_USER_HOME="$PWD/.tools/conan-home"
export CONAN_REVISIONS_ENABLED=1
.tools/conan1/bin/conan profile new native --detect
.tools/conan1/bin/conan profile update settings.compiler.cppstd=20 native
.tools/conan1/bin/conan profile update settings.compiler.libcxx=libstdc++11 native
.tools/conan1/bin/conan remote add shred https://conan.shredeagle.fr
python3 scripts/build.py --fetch-adnn \
  --profile "$CONAN_USER_HOME/.conan/profiles/native"
```

Install the platform's OpenGL, X11 development headers and audio dependencies
beforehand (for Debian/Ubuntu these include `build-essential`, `ninja-build`,
`libgl1-mesa-dev`, `xorg-dev`, `libasound2-dev`, `libusb-1.0-0-dev`, `pkg-config`, and `python3-venv`). CMake may
need installing separately to reach 3.23. Dependency recipes can report further
system requirements. The script does not run sudo or change global Conan settings.

`cmake/adnn-sources.json` fixes all four Adnn source commits and their repositories.
`--fetch-adnn` downloads missing directories and their submodules. Existing
checkouts are preserved; to reproduce the pinned source state, use an empty
`--adnn-root`. The source pins intentionally match the available working Snacman
workspace; its historical recipe refers to a different graphics revision.

The recipe pins direct dependency versions. The first resolution also depends on
the configured remotes and available recipe revisions. It produces
`build-native/conan.lock`; archive that lock, the Conan profiles, sources and
package artifacts for reproducibility, and pass `--lockfile` on subsequent builds.
The custom `glfw/3.3.8@adnn/patch` recipe requires the Shred remote or an archived
cache; these old packages are not guaranteed to remain downloadable from public
servers. A clean remote-only build has not been verified here.

CMake uses the standard Conan 1
[CMakeDeps and CMakeToolchain flow](https://docs.conan.io/1/reference/conanfile/tools/cmake/cmaketoolchain.html).
Conan's host and build profiles are explicit. For cross compilation supply a
separate `--build-profile`; don't label a newer compiler as an older one to force
binary cache reuse.

## Reusing the existing Snacman dependency cache

The following local development build was used to verify the actual native
libraries without changing Snacman or its dependency checkouts:

```sh
python3 scripts/build.py --adnn-root /home/franz/gamedev \
  --toolchain /home/franz/gamedev/snacman/build-conan-gcc14.0/Release/generators/conan_toolchain.cmake
```

The path is an explicit machine-local option, never an automatic fallback. It
requires the referenced Conan package files to remain present. Upstream graphics,
ImGui adapter and sounds sources are compiled afresh; their transitive dependencies
come from that cache. The old toolchain selects `_GLIBCXX_USE_CXX11_ABI=0`, which
must match those cached C++ dependencies. This is different from a glibc deployment
baseline: see GCC's [dual ABI documentation](https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html).
Use a fresh build directory when changing toolchains, build types or ABI settings.

## Older Ubuntu deployment

Choose the oldest supported Ubuntu release **before** producing distribution
binaries. No minimum Ubuntu version is certified by the host development build.
Build the application, all four Adnn libraries, every Conan dependency, and the
optional Helios SDK inside that oldest release's container/VM or against a complete
matching sysroot. Do not reuse the host's modern Conan binaries for this build.
Use a dedicated Conan home/cache and `--build-all-deps` with the target profile.
A recent compiler can be built in that old environment to supply C++20 while still
linking its libc baseline; test whether the resulting libstdc++ must be bundled.

A container setup needs the release's compiler/build packages and CMake >=3.23.
Mount the project and use separate build/cache paths for each Ubuntu baseline.
The container itself does not provide a desktop or GPU test: run the installed
application on a real machine or VM with that release, display driver and audio
stack afterwards. Capture the container image digest, compiler version, profiles,
Conan lockfile and runtime test result with the release artifacts.

For a sysroot build, set `CMAKE_SYSROOT`, compiler paths, and CMake find-root modes
in a toolchain file and pass it through Conan's
`tools.cmake.cmaketoolchain:user_toolchain` profile configuration. The sysroot must
contain compatible headers, loader, libc, libstdc++, OpenGL/X11/audio development
files and target libraries. See [CMake's sysroot documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_SYSROOT.html).
Host libraries leaking into that build defeat the compatibility target.

Inspect **every shipped ELF executable and shared library**, including libraries
loaded through `dlopen`, against the target machine's supported symbol versions:

```sh
readelf --version-info build-native/chienpo
objdump -T build-native/chienpo | grep -E 'GLIBC_|GLIBCXX_|CXXABI_'
ldd build-native/chienpo
```

Use `ldd` only on trusted binaries and repeat on the deployment target. Check for
missing libraries and version requirements newer than the chosen baseline.
Static `libstdc++`/`libgcc` may remove those shared-runtime dependencies, but it
**does not lower the glibc requirement** of the application or other libraries.
A successful compile or host launch is not proof of older Ubuntu compatibility.

Helios output loads `libHeliosDACAPI.so` at runtime. The user's fork from
`/home/franz/project/helios_dac`, commit
`d552776553ddb8748d72ebfd1ee46a6b51a13334`, is included as source under
`vendor/helios`, with original licenses and a SHA-256 provenance manifest.
Its USB and IDN network implementations are built by default into the native
build directory and installed in `lib`; no prebuilt SDK or USB binary is copied.
The build uses the target system's `libusb-1.0` through pkg-config. The ELF SDK
and its USB dependency need the same oldest-target build and symbol checks as
the game. Set `-DCHIENPO_BUILD_HELIOS=OFF` only when using an externally supplied
compatible SDK. Physical laser output requires separate hardware validation.
