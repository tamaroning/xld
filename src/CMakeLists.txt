add_executable(xld)
target_sources(xld PRIVATE
    main.cc
    wasm_main.cc
    parse_object.cc
    input_file.cc
    pass.cc
    )
target_link_libraries(xld PRIVATE
    headers
    common
    tbb
    )

add_subdirectory(common)
