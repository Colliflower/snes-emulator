set(src_files emulator.cpp)
add_executable(myemulator ${src_files})

set(warnings $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

target_include_directories(myemulator PUBLIC ${PROJECT_SOURCE_DIR}/include)

install(TARGETS myemulator DESTINATION bin)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION .)

if(MSVC)
    # Install pdb files when available
    install(FILES $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWIthDebInfo>>:$<TARGET_PDB_FILE:myemulator>> DESTINATION lib)
    set_target_properties(myemulator PROPERTIES
        COMPILE_PDB_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin
        COMPILE_PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_INSTALL_PREFIX}/bin
        COMPILE_PDB_NAME myemulator
    )
endif()
