"""Conan 1.66 external dependencies; native Adnn libraries are pinned source builds.

Keeping their source builds in CMake avoids depending on unpublished Adnn binaries.
Game persistence uses the vendored C library; nlohmann_json is upstream Graphics' dependency.
"""
from conans import ConanFile


class ChienpoDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    requires = (
        "freetype/2.12.1", "glad/0.1.36", "glfw/3.3.8@adnn/patch",
        "nlohmann_json/3.11.2", "spdlog/1.13.0", "utfcpp/4.0.1",
        "imgui/1.89.8", "implot/0.16", "openal-soft/1.23.1",
        "stb/cci.20230920", "zlib/1.3",
    )
    default_options = {
        "*:shared": False,
        "glad:gl_version": "4.6",
        "glad:extensions": "GL_KHR_debug,GL_ARB_texture_storage,GL_ARB_clear_texture,GL_ARB_program_interface_query,GL_ARB_shader_storage_buffer_object,GL_ARB_base_instance,GL_ARB_multi_draw_indirect,GL_ARB_texture_filter_anisotropic,",
    }
