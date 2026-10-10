cmake_minimum_required(VERSION 3.16)
project(NexImageEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Targets Qt6 (uses QSinglePointEvent::position(), available since Qt6).
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)
set(QT_VERSION_MAJOR 6)

# OpenCV backs the high-quality "Sharpen" tool (src/engine/CvSharpen.*).
# apt: libopencv-dev (Ubuntu/Debian). We only need core/imgproc/photo.
# Some setups install OpenCV's cmake config outside CMake's default search
# path, so we widen the search first and fail with actionable instructions
# instead of CMake's generic "config file not found" error.
find_package(OpenCV QUIET COMPONENTS core imgproc photo
    HINTS
        /usr/lib/x86_64-linux-gnu/cmake/opencv4
        /usr/lib/aarch64-linux-gnu/cmake/opencv4
        /usr/lib/cmake/opencv4
        /usr/local/lib/cmake/opencv4
        /usr/local/share/opencv4
        /usr/share/opencv4
        /usr/share/OpenCV
)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR
        "OpenCV was not found (required for the Sharpen tool).\n"
        "Install it first:\n"
        "  Ubuntu/Debian : sudo apt install libopencv-dev\n"
        "  Fedora        : sudo dnf install opencv-devel\n"
        "  Arch          : sudo pacman -S opencv\n"
        "\n"
        "If it's already installed but CMake still can't find it, locate its\n"
        "config file and point CMake at that directory:\n"
        "  find / -name OpenCVConfig.cmake 2>/dev/null\n"
        "  cmake -B build -DOpenCV_DIR=<directory containing that file>")
endif()

file(GLOB_RECURSE NEX_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE NEX_HEADERS CONFIGURE_DEPENDS src/*.h)

list(REMOVE_ITEM NEX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
add_library(NexEditor STATIC ${NEX_SOURCES} ${NEX_HEADERS})
add_executable(NexImageEditor src/main.cpp)
target_link_libraries(NexImageEditor PRIVATE NexEditor)

target_include_directories(NexEditor PUBLIC
    src
    src/gui
    src/engine
    src/project
    src/io
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(NexEditor PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    ${OpenCV_LIBS}
)

if(WIN32)
    set_target_properties(NexImageEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(GNUInstallDirs)
install(TARGETS NexImageEditor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/NexImageEditor.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/NexImageEditor.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# NOTE: WebP read/write needs Qt's "qtimageformats" add-on module/plugin
# installed alongside Qt (apt: qt6-image-formats-plugins, or the
# qtimageformats source module built from source). PNG/JPEG work out of
# the box via the plugins Qt ships by default.
include(CTest)
if(BUILD_TESTING)
    add_executable(EditorCoreTests tests/EditorCoreTests.cpp)
    target_link_libraries(EditorCoreTests PRIVATE NexEditor)
    add_test(NAME EditorCoreTests COMMAND EditorCoreTests)

    add_executable(EditorUiTests tests/EditorUiTests.cpp)
    target_link_libraries(EditorUiTests PRIVATE NexEditor)
    add_test(NAME EditorUiTests COMMAND EditorUiTests)
    set_tests_properties(EditorCoreTests EditorUiTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60)
endif()
