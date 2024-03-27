add_executable(xld)
target_sources(xld PRIVATE
    main.cc
    )
target_link_libraries(xld PRIVATE
    #utils
    wasm
    common
    tbb
    )

#add_subdirectory(utils)
add_subdirectory(wasm)
