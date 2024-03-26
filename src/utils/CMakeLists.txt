add_library(utils)
target_sources(utils PRIVATE
    main.cc
    )
target_link_libraries(utils PRIVATE
    common
    )

add_subdirectory(utils)
