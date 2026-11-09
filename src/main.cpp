#include "chienpo/persistence.hpp"
#include "chienpo/render.hpp"
#include "chienpo/ui.hpp"
#include <algorithm>
#include <chrono>
#include <graphics/ApplicationGlfw.h>
#include <imguiui/ImguiUi.h>
#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace {
struct Options
{
    std::filesystem::path resources;
    bool headless = false, audio = true, helios = true;
    std::uint64_t frames = 0;
};
std::filesystem::path default_resources(const char * executable)
{
    std::error_code error;
    auto executable_path =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) executable_path = std::filesystem::absolute(executable);
    const auto installed = executable_path.parent_path() / "../share/chienpo";
    if (std::filesystem::is_directory(installed)) return installed;
    return CHIENPO_RESOURCE_DIR;
}
} // namespace
int main(int argc, char ** argv)
{
    try
    {
        Options options;
        options.resources = default_resources(argv[0]);
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                std::cout << "Chienpo native editor and game\nUsage: chienpo "
                             "[--resources PATH] [--headless] [--frames N] "
                             "[--no-audio] [--no-helios]\n"
                             "--headless runs a fresh calibration world "
                             "without graphics, audio, or hardware.\n"
                             "--frames N exits after N ticks (headless) or "
                             "displayed frames (graphical).\n"
                             "Startup never loads saved_levels; use the Levels "
                             "panel to explicitly load your own world.\n";
                return 0;
            }
            if (arg == "--headless") options.headless = true;
            else if (arg == "--no-audio") options.audio = false;
            else if (arg == "--no-helios") options.helios = false;
            else if (arg == "--resources" && i + 1 < argc)
                options.resources = argv[++i];
            else if (arg == "--frames" && i + 1 < argc)
            {
                const std::string value = argv[++i];
                std::size_t end = 0;
                if (value.empty() || value.front() == '-')
                    throw std::runtime_error(
                        "--frames expects a positive integer");
                options.frames = std::stoull(value, &end);
                if (end != value.size() || options.frames == 0)
                    throw std::runtime_error(
                        "--frames expects a positive integer");
            }
            else
                throw std::runtime_error("Unknown or incomplete argument: "
                                         + arg);
        }
        if (!std::filesystem::is_directory(options.resources))
            throw std::runtime_error("Resource directory not found: "
                                     + options.resources.string());
        // The large fixed entity pool belongs on the heap, including on
        // small-stack hosts.
        auto game = std::make_unique<chienpo::Game>(options.resources, 0);
        chienpo::refresh_resources(*game);
        for (auto index : game->m_devices)
            game->m_things.get(index).m_device_on = false;
        if (options.headless)
        {
            const auto frames = options.frames ? options.frames : 1;
            for (std::uint64_t i = 0; i < frames; ++i)
                game->update(chienpo::Input{},
                             1. / std::clamp(game->m_context.m_fps, 1., 1000.));
            std::string error;
            if (!game->m_things.validate(&error))
                throw std::runtime_error("World validation failed: " + error);
            std::cout << "Headless completed: " << frames << " ticks, "
                      << game->m_devices.size()
                      << " device(s), fresh calibration world\n";
            return 0;
        }
        // ApplicationGlfw initializes and owns its graphics/OpenGL loggers.
        ad::graphics::ApplicationGlfw application{"Chienpo", 1440, 900};
        // Replace ApplicationGlfw's default Escape-to-close callback. Escape
        // belongs to editor cancellation; GLFW still handles the title-bar
        // close button normally.
        application.getAppInterface()->registerKeyCallback(
            [](int, int, int, int) {});
        ad::imguiui::ImguiUi imgui{application};
        imgui.registerGlfwCallbacks(application);
        chienpo::Ui ui;
        ui.hardware_enabled = options.helios;
        chienpo::Audio audio;
        chienpo::HeliosOutput helios;
        struct Cleanup
        {
            chienpo::Audio & audio;
            chienpo::HeliosOutput & helios;
            ~Cleanup()
            {
                chienpo::audio_close(audio);
                chienpo::helios_close(helios);
            }
        } cleanup{audio, helios};
        if (options.audio && !chienpo::audio_open(audio))
            game->log(audio.error);
        else if (!options.audio) audio.error = "Disabled with --no-audio";
        if (options.helios)
        {
            chienpo::helios_open(helios);
            game->m_helios_count = helios.device_count;
            if (!helios.error.empty()) game->log(helios.error);
        }
        else helios.error = "Disabled with --no-helios";
        // Virtual screens start unassigned and off: physical output requires an
        // explicit projector choice and Laser on in the UI, including after a
        // device rescan.
        auto previous = std::chrono::steady_clock::now();
        double accumulator = 1. / game->m_context.m_fps;
        std::uint64_t displayed = 0;
        while (application.handleEvents())
        {
            const auto now = std::chrono::steady_clock::now();
            const double delta = std::clamp(
                std::chrono::duration<double>(now - previous).count(), 0., .1);
            previous = now;
            imgui.newFrame();
            chienpo::draw_ui(*game, ui, audio, helios, application, delta);
            accumulator += delta;
            const double step =
                1. / std::clamp(game->m_context.m_fps, 1., 1000.);
            while (accumulator >= step)
            {
                game->update(chienpo::poll_input(application,
                                                 imgui.isCapturingKeyboard()),
                             step);
                accumulator -= step;
            }
            chienpo::build_draw_lists(*game);
            chienpo::audio_update(audio, *game);
            if (options.helios) chienpo::helios_submit(helios, *game);
            int width = 0, height = 0;
            glfwGetFramebufferSize(application.getGlfwWindow(), &width,
                                   &height);
            glViewport(0, 0, width, height);
            glClearColor(.025f, .03f, .045f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            imgui.render();
            imgui.renderBackend();
            application.swapBuffers();
            if (options.frames && ++displayed >= options.frames) break;
        }
        return 0;
    }
    catch (const std::exception & e)
    {
        std::cerr << "Chienpo: " << e.what() << '\n';
        return 1;
    }
}
