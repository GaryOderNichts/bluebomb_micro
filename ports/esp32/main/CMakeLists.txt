idf_component_register(SRCS "main.c;${CMAKE_CURRENT_SOURCE_DIR}/../l2cb.c"
                       INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
                       REQUIRES "bluebomb_micro driver")
