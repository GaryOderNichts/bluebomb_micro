# include the generated list
set(L2CB_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${L2CB_DIR}/l2cb_list.cmake)

function(bluebomb_generate_l2cb bluebomb_target)
    set(BLUEBOMB_L2CB_VALUE "${${bluebomb_target}}")
    if (BLUEBOMB_L2CB_VALUE STREQUAL "")
        message(SEND_ERROR "Target must be one of ${BLUEBOMB_L2CB_LIST}")
        return()
    endif()

    message(STATUS "Target is ${bluebomb_target}, using L2CB value ${BLUEBOMB_L2CB_VALUE}")
    configure_file(${L2CB_DIR}/l2cb.c.in ${CMAKE_CURRENT_SOURCE_DIR}/l2cb.c)
endfunction()
