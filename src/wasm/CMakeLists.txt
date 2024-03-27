add_library(wasm)
target_sources(wasm PRIVATE
    main.cc
    )
target_link_libraries(wasm PRIVATE
    common
    tbb
    )
