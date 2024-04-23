add_executable(xld)
target_sources(xld PRIVATE
    main.cc
    linker.cc
    parse_object.cc
    input_file.cc
    pass.cc
    chunk.cc
    symbol.cc
    output_elem.cc
    )
target_link_libraries(xld PRIVATE
    headers
    common
    tbb
    )

add_subdirectory(common)
