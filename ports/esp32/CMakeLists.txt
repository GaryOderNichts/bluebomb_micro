cmake_minimum_required(VERSION 3.16)

# Append bluebomb_micro components
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bluebomb)

# Check system value
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/l2cb.cmake)
if (NOT DEFINED BLUEBOMB_TARGET)
    message(SEND_ERROR "Missing BLUEBOMB_TARGET, available options are: ${BLUEBOMB_L2CB_LIST}")
endif()

# Generate the l2cb value
bluebomb_generate_l2cb(${BLUEBOMB_TARGET})
