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

if(MSVC)
  target_compile_options(xld PRIVATE /W4 /WX)
else()
  target_compile_options(xld PRIVATE -Wall -Wpedantic)
endif()

add_subdirectory(common)
