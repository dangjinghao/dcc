set(DCC_STAGE2_OBJECTS "")

foreach(src IN LISTS DCC_STAGE2_ALL_SOURCES)
    get_filename_component(src_name "${src}" NAME_WE)
    set(obj "${CMAKE_CURRENT_BINARY_DIR}/${src_name}.o")

    add_custom_command(
        OUTPUT "${obj}"
        COMMAND "$<TARGET_FILE:dcc>"
            -I "${CMAKE_SOURCE_DIR}/src"
            -c "${CMAKE_SOURCE_DIR}/${src}"
            -o "${obj}"
        DEPENDS dcc "${CMAKE_SOURCE_DIR}/${src}"
        COMMENT "Stage2: Compiling ${src}"
    )

    list(APPEND DCC_STAGE2_OBJECTS "${obj}")
endforeach()

add_executable(dcc_stage2 ${DCC_STAGE2_OBJECTS})
set_target_properties(dcc_stage2 PROPERTIES LINKER_LANGUAGE C)
add_dependencies(dcc_stage2 dcc)

if(DCC_BACKEND STREQUAL "llvm")
    target_link_libraries(dcc_stage2 PRIVATE ${DCC_LLVM_LIBS})
endif()
