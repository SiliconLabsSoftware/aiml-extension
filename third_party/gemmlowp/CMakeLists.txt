############
# Gemmlowp #
############

add_library(gemmlowp INTERFACE)
target_include_directories(gemmlowp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
