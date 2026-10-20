cmake_minimum_required(VERSION 3.20)

# Builds the GTK 4 application, which lives in gtk/. The Qt 6 sources in src/ are kept for
# reference only and are not built.
project(lgl-dnf-helper LANGUAGES C CXX)

include(CTest)

# Put the binary at the top of the build directory (build/lgl-dnf-helper).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(gtk)
