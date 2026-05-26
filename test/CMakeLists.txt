file(GLOB DCC_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dcc/*.c")
file(GLOB DCC_CHIBICC_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/chibicc/*.c")
set(DCC_CHIBICC_COMMON_LIB "${CMAKE_CURRENT_SOURCE_DIR}/chibicc/common.lib")

function(_dcc_add_test_suite label compiler_target)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}/${label}")

    file(MAKE_DIRECTORY "${build_dir}/dcc")
    file(MAKE_DIRECTORY "${build_dir}/chibicc")

    foreach(src IN LISTS DCC_TEST_SOURCES)
        get_filename_component(name "${src}" NAME_WE)
        set(exe "${build_dir}/dcc/${name}")

        add_custom_command(
            OUTPUT "${exe}"
            COMMAND "$<TARGET_FILE:${compiler_target}>" "${src}" -o "${exe}"
            DEPENDS ${compiler_target} "${src}"
            COMMENT "[${label}] dcc/${name}.c"
        )
        list(APPEND all_exes "${exe}")

        add_test(
            NAME "${label}/dcc/${name}"
            COMMAND "${exe}"
        )
        set_tests_properties("${label}/dcc/${name}" PROPERTIES
            LABELS "${label}"
        )
    endforeach()

    foreach(src IN LISTS DCC_CHIBICC_TEST_SOURCES)
        get_filename_component(name "${src}" NAME_WE)
        set(exe "${build_dir}/chibicc/${name}")

        if(name STREQUAL "commonsym")
            set(extra_flags "-fcommon")
        else()
            set(extra_flags "")
        endif()

        add_custom_command(
            OUTPUT "${exe}"
            COMMAND "$<TARGET_FILE:${compiler_target}>"
                ${extra_flags}
                "${src}"
                -x c "${DCC_CHIBICC_COMMON_LIB}" -x none
                -o "${exe}"
            DEPENDS ${compiler_target} "${src}" "${DCC_CHIBICC_COMMON_LIB}"
            COMMENT "[${label}] chibicc/${name}.c"
        )
        list(APPEND all_exes "${exe}")

        add_test(
            NAME "${label}/chibicc/${name}"
            COMMAND "${exe}"
        )
        set_tests_properties("${label}/chibicc/${name}" PROPERTIES
            LABELS "${label}"
        )
    endforeach()

    add_custom_target("test_${label}_exes" DEPENDS ${all_exes})

    add_custom_target("test_${label}"
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}"
            -L "${label}" --output-on-failure
        DEPENDS "test_${label}_exes"
    )
endfunction()

_dcc_add_test_suite(stage1 dcc)
_dcc_add_test_suite(stage2 dcc_stage2)
_dcc_add_test_suite(stage3 dcc_stage3)
