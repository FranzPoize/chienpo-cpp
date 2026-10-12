# The same library sources as Snacman, rebuilt in this project's build tree.
# Their own CMake package registration resolves dependencies between libraries.
set(CHIENPO_ADNN_ROOT "" CACHE PATH "Directory containing handy, math, graphics and sounds checkouts")
if(CHIENPO_ADNN_ROOT)
    set(BUILD_tests OFF CACHE BOOL "Upstream demo/test applications" FORCE)
    set(BUILD_CONF_WarningAsError OFF CACHE BOOL "Upstream warnings are not fatal" FORCE)
    find_package(imgui CONFIG REQUIRED)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _configuration)
    find_path(CHIENPO_IMGUI_BACKENDS imgui_impl_glfw.cpp
        HINTS "${imgui_PACKAGE_FOLDER_${_configuration}}/res/bindings"
        "${CMAKE_BINARY_DIR}/conan_imports/imgui_backends"
        DOC "Dear ImGui 1.89.8 backend source directory" REQUIRED)
    set(_backend_destination "${CMAKE_BINARY_DIR}/upstream/graphics/conan_imports/imgui_backends")
    file(MAKE_DIRECTORY "${_backend_destination}")
    foreach(_file imgui_impl_glfw.cpp imgui_impl_glfw.h imgui_impl_opengl3.cpp imgui_impl_opengl3.h imgui_impl_opengl3_loader.h)
        configure_file("${CHIENPO_IMGUI_BACKENDS}/${_file}" "${_backend_destination}/${_file}" COPYONLY)
    endforeach()
    foreach(_library handy math graphics sounds)
        if(NOT EXISTS "${CHIENPO_ADNN_ROOT}/${_library}/CMakeLists.txt")
            message(FATAL_ERROR "Missing ${_library} checkout under CHIENPO_ADNN_ROOT; see docs/build.md")
        endif()
        add_subdirectory("${CHIENPO_ADNN_ROOT}/${_library}" "upstream/${_library}" EXCLUDE_FROM_ALL)
    endforeach()
else()
    # Packaged installations of exactly the same libraries are also supported.
    find_package(Graphics CONFIG REQUIRED COMPONENTS graphics imguiui)
    find_package(Sounds CONFIG REQUIRED COMPONENTS sounds)
endif()
