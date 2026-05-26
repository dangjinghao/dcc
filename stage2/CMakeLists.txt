function(_dcc_bootstrap label compiler_old compiler_new)
    set(objects "")

    foreach(src IN LISTS DCC_STAGE2_ALL_SOURCES)
        get_filename_component(name "${src}" NAME_WE)
        set(obj "${CMAKE_CURRENT_BINARY_DIR}/${label}_${name}.o")

        add_custom_command(
            OUTPUT "${obj}"
            COMMAND "$<TARGET_FILE:${compiler_old}>"
                -I "${CMAKE_SOURCE_DIR}/src"
                -c "${CMAKE_SOURCE_DIR}/${src}"
                -o "${obj}"
            DEPENDS ${compiler_old} "${CMAKE_SOURCE_DIR}/${src}"
            COMMENT "[${label}] Compiling ${src}"
        )
        list(APPEND objects "${obj}")
    endforeach()

    add_executable(${compiler_new} ${objects})
    set_target_properties(${compiler_new} PROPERTIES
        LINKER_LANGUAGE C
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    add_dependencies(${compiler_new} ${compiler_old})

    if(DCC_BACKEND STREQUAL "llvm")
        target_link_libraries(${compiler_new} PRIVATE ${DCC_LLVM_LIBS})
    endif()
endfunction()

_dcc_bootstrap(stage2 dcc dcc_stage2)
_dcc_bootstrap(stage3 dcc_stage2 dcc_stage3)
