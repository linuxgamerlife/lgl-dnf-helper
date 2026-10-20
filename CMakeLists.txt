cmake_minimum_required(VERSION 3.20)

project(lgl-dnf-helper VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic> $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

option(DNFHELPER_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(DNFHELPER_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.20)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(JSONGLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

# --- resources -----------------------------------------------------------------

set(RESOURCE_XML ${PROJECT_SOURCE_DIR}/data/dnfhelper.gresource.xml)
set(RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/dnfhelper-resources.c)
set(RESOURCE_DEPS
    ${PROJECT_SOURCE_DIR}/data/style.css
    ${PROJECT_SOURCE_DIR}/packaging/icons/256x256/lgl-dnf-helper.png
    ${PROJECT_SOURCE_DIR}/HELP.html)
add_custom_command(
    OUTPUT ${RESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${RESOURCE_C}
            --sourcedir=${PROJECT_SOURCE_DIR}/data --sourcedir=${PROJECT_SOURCE_DIR}
            ${RESOURCE_XML}
    DEPENDS ${RESOURCE_XML} ${RESOURCE_DEPS}
    VERBATIM)
set_source_files_properties(${RESOURCE_C} PROPERTIES COMPILE_OPTIONS "-w")

# --- core (no widgets; testable) ------------------------------------------------

add_library(dnfhelper_gtk_core STATIC
    src/core/backend.cpp
    src/core/backend.h
    src/core/commandrunner.cpp
    src/core/commandrunner.h
    src/core/package.cpp
    src/core/package.h
    src/core/parsers.cpp
    src/core/parsers.h
    src/core/settings.cpp
    src/core/settings.h
    src/core/theme.cpp
    src/core/theme.h
)
target_include_directories(dnfhelper_gtk_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(dnfhelper_gtk_core PUBLIC PkgConfig::GTKMM PkgConfig::JSONGLIB)

# --- application ----------------------------------------------------------------

add_executable(lgl-dnf-helper
    ${RESOURCE_C}
    src/main.cpp
    src/ui/datatable.cpp
    src/ui/datatable.h
    src/ui/helpwindow.cpp
    src/ui/helpwindow.h
    src/ui/themewatcher.cpp
    src/ui/themewatcher.h
    src/ui/window.cpp
    src/ui/window.h
)
target_compile_definitions(lgl-dnf-helper PRIVATE DNFHELPER_VERSION="${PROJECT_VERSION}")
target_link_libraries(lgl-dnf-helper PRIVATE dnfhelper_gtk_core PkgConfig::ADWAITA)

install(TARGETS lgl-dnf-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/com.linuxgamerlife.lgl-dnf-helper.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
foreach(size 48 64 128 256)
    install(FILES packaging/icons/${size}x${size}/lgl-dnf-helper.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
endforeach()

# --- tests -----------------------------------------------------------------------

if(BUILD_TESTING)
    set(DNFHELPER_TESTS backend package parsers settings theme)
    foreach(test ${DNFHELPER_TESTS})
        add_executable(${test}_test tests/${test}_test.cpp)
        target_link_libraries(${test}_test PRIVATE dnfhelper_gtk_core)
        add_test(NAME ${test} COMMAND ${test}_test)
    endforeach()
    # g_test_skip() reports 77 under the TAP harness.
    set_tests_properties(${DNFHELPER_TESTS} PROPERTIES SKIP_RETURN_CODE 77)
endif()
