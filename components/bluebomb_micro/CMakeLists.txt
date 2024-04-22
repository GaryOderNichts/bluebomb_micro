# Set required source files
set(BLUEBOMB_SOURCE_FILES
    "bluebomb_micro.c"
    "stage0_bin.h"
    "stage1_bin.h"
)

if (IDF_TARGET)
    # For ESP-IDF register a component
    idf_component_register(SRCS "${BLUEBOMB_SOURCE_FILES}"
                           INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
                           REQUIRES "btstack")
elseif (PICO_SDK_VERSION_STRING)
    # For the pico add a library
    add_library(bluebomb_micro ${BLUEBOMB_SOURCE_FILES})
    target_include_directories(bluebomb_micro PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(bluebomb_micro PUBLIC
        pico_stdlib
        pico_btstack_classic
        pico_btstack_cyw43
        pico_cyw43_arch_none
    )
endif()
