cmake_minimum_required(VERSION 3.16)
project(NexImageEditor VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(NEX_BUILD_TESTS "Build the unit test executable" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Everything except main.cpp lives in a static library so tests can link it.
file(GLOB_RECURSE NEX_LIB_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/tools/*.cpp src/ui/*.cpp)
file(GLOB_RECURSE NEX_LIB_HEADERS CONFIGURE_DEPENDS src/core/*.h src/tools/*.h src/ui/*.h)

add_library(nexcore STATIC ${NEX_LIB_SOURCES} ${NEX_LIB_HEADERS})
target_include_directories(nexcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nexcore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    target_compile_options(nexcore PRIVATE /W4 /permissive-)
else()
    target_compile_options(nexcore PRIVATE -Wall -Wextra)
endif()

add_executable(NexImageEditor src/main.cpp)
target_link_libraries(NexImageEditor PRIVATE nexcore)
if(WIN32)
    set_target_properties(NexImageEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(GNUInstallDirs)
install(TARGETS NexImageEditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NEX_BUILD_TESTS)
    enable_testing()
    add_executable(nex_tests tests/tests.cpp)
    target_link_libraries(nex_tests PRIVATE nexcore)
    add_test(NAME nex_tests COMMAND nex_tests)
endif()
