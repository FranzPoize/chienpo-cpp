# Build the user's Helios fork from vendored source, never a prebuilt SDK binary.
add_subdirectory(vendor/helios)
# Keep the SDK beside the executable in the build tree for optional dlopen.
set_target_properties(HeliosDACAPI PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
install(TARGETS HeliosDACAPI LIBRARY DESTINATION lib)
install(FILES vendor/helios/LICENSE.md DESTINATION share/chienpo/licenses RENAME Helios-LICENSE.md)
