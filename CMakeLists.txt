# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensor-trap-demo)

# See https://github.com/espressif/esp-idf/issues/9511#issuecomment-1207817643
# Check whether this is fixed when LVGL 8.3.5 is released
idf_component_get_property(lib lvgl COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=format= -Wno-format)
