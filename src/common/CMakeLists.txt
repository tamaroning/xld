add_library(common)
target_sources(common PRIVATE
    common.cc
    file.cc
    leb.cc)
target_link_libraries(common PRIVATE headers tbb)
