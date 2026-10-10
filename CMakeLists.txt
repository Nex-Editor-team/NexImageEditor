cmake_minimum_required(VERSION 3.16)
project(NexImageEditor VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NEX_BUILD_TESTS "Build the unit test executable" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(GDKPIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gobject-2.0)

# Core (no GTK): document model, pixel operations, file formats, settings, tool metadata.
file(GLOB NEX_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/tools/*.cpp)
add_library(nexcore STATIC ${NEX_CORE_SOURCES})
target_include_directories(nexcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nexcore PUBLIC PkgConfig::CAIRO PkgConfig::PANGOCAIRO PkgConfig::GDKPIXBUF PkgConfig::GLIB)
if(MSVC)
    target_compile_options(nexcore PRIVATE /W4 /permissive-)
else()
    target_compile_options(nexcore PRIVATE -Wall -Wextra)
endif()

# GUI: GTK 4 through gtkmm.
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
file(GLOB NEX_UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp)
add_executable(NexImageEditor src/main.cpp ${NEX_UI_SOURCES})
target_link_libraries(NexImageEditor PRIVATE nexcore PkgConfig::GTKMM)
if(MSVC)
    target_compile_options(NexImageEditor PRIVATE /W4 /permissive-)
else()
    target_compile_options(NexImageEditor PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS NexImageEditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NEX_BUILD_TESTS)
    enable_testing()
    add_executable(nex_tests tests/tests.cpp)
    target_link_libraries(nex_tests PRIVATE nexcore)
    add_test(NAME nex_tests COMMAND nex_tests)
endif()
