cmake_minimum_required(VERSION 3.16)
project(lgl-dnf-helper VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(lgl-dnf-helper
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/backend/PackageBackend.h
    src/backend/Dnf5CliBackend.cpp
    src/backend/Dnf5CliBackend.h
    src/backend/Dnf5Process.cpp
    src/backend/Dnf5Process.h
    src/model/Dependency.cpp
    src/model/Dependency.h
    src/model/Package.cpp
    src/model/Package.h
    src/model/PackageQueryResult.cpp
    src/model/PackageQueryResult.h
    src/services/PackageClassifier.cpp
    src/services/PackageClassifier.h
    src/util/HumanSize.cpp
    src/util/HumanSize.h
    resources/resources.qrc
)

target_include_directories(lgl-dnf-helper PRIVATE src)

target_link_libraries(lgl-dnf-helper PRIVATE Qt6::Core Qt6::Widgets)

target_compile_options(lgl-dnf-helper PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS lgl-dnf-helper DESTINATION bin)
install(FILES data/lgl-dnf-helper.desktop DESTINATION share/applications)
install(FILES resources/icons/lgl-dnf-helper-icon.png
        DESTINATION share/icons/hicolor/512x512/apps
        RENAME lgl-dnf-helper.png)
install(FILES packaging/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml
        DESTINATION share/metainfo)
