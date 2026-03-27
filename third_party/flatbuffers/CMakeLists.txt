###############
# Flatbuffers #
###############

add_library(flatbuffers INTERFACE)
target_include_directories(flatbuffers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
