# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components)
set(EXTRA_COMPONENT_DIRS components)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_s2_dmx)
