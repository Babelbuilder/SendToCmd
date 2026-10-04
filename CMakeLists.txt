cmake_minimum_required(VERSION 3.21)
project(SendToCmd VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)

option(SENDTOCMD_STATIC_WINDOWS "Build a standalone Windows EXE with static Qt and MinGW runtimes" OFF)
option(SENDTOCMD_SIZE_OPTIMIZED "Optimize the static Windows EXE for file size" ON)
if(SENDTOCMD_STATIC_WINDOWS)
    if(NOT WIN32 OR NOT MINGW)
        message(FATAL_ERROR "SENDTOCMD_STATIC_WINDOWS requires a Windows MinGW build")
    endif()
    get_target_property(SENDTOCMD_QT_CORE_TYPE Qt6::Core TYPE)
    if(NOT SENDTOCMD_QT_CORE_TYPE STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "SENDTOCMD_STATIC_WINDOWS requires a static Qt installation; a regular Qt DLL SDK is insufficient")
    endif()
endif()

set(SOURCES
    src/main.cpp
    resources/icons.qrc
    src/MainWindow.cpp
    src/MainWindow.h
    src/CommandEditor.cpp
    src/CommandEditor.h
    src/core/SendValidator.cpp
    src/core/SendValidator.h
    src/core/AutoSendEngine.cpp
    src/core/AutoSendEngine.h
    src/target/ICommandTarget.h
    src/target/ExternalWindowTarget.cpp
    src/target/ExternalWindowTarget.h
    src/platform/NativeWindow.h
)

if(WIN32)
    enable_language(RC)
    set(SENDTOCMD_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/SendToCmd.ico")
    configure_file(resources/windows.rc.in "${CMAKE_CURRENT_BINARY_DIR}/windows.rc" @ONLY)
    list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/windows.rc")
    list(APPEND SOURCES src/platform/NativeWindow_win.cpp)
elseif(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(resources/SendToCmd.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND SOURCES resources/SendToCmd.icns)
    list(APPEND SOURCES src/platform/NativeWindow_mac.mm)
else()
    list(APPEND SOURCES src/platform/NativeWindow_linux.cpp)
    find_package(X11 REQUIRED)
endif()

qt_add_executable(SendToCmd WIN32 MACOSX_BUNDLE ${SOURCES})
if(APPLE)
    set_target_properties(SendToCmd PROPERTIES MACOSX_BUNDLE_ICON_FILE SendToCmd.icns)
endif()
if(CMAKE_CROSSCOMPILING AND SENDTOCMD_HOST_MOC)
    set_target_properties(SendToCmd PROPERTIES AUTOMOC_EXECUTABLE "${SENDTOCMD_HOST_MOC}")
    get_filename_component(SENDTOCMD_HOST_QT_TOOLS "${SENDTOCMD_HOST_MOC}" DIRECTORY)
    set_target_properties(SendToCmd PROPERTIES AUTORCC_EXECUTABLE "${SENDTOCMD_HOST_QT_TOOLS}/rcc")
endif()
target_link_libraries(SendToCmd PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
if(SENDTOCMD_STATIC_WINDOWS)
    target_link_options(SendToCmd PRIVATE -static -static-libgcc -static-libstdc++)
    if(SENDTOCMD_SIZE_OPTIMIZED)
        target_compile_options(SendToCmd PRIVATE -Os)
    endif()
endif()
install(TARGETS SendToCmd BUNDLE DESTINATION . RUNTIME DESTINATION bin)
if(WIN32)
    target_link_libraries(SendToCmd PRIVATE user32)
elseif(APPLE)
    target_link_libraries(SendToCmd PRIVATE "-framework AppKit" "-framework ApplicationServices")
else()
    target_link_libraries(SendToCmd PRIVATE X11::X11)
    if(TARGET X11::Xtst)
        target_link_libraries(SendToCmd PRIVATE X11::Xtst)
    else()
        target_link_libraries(SendToCmd PRIVATE Xtst)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(core_tests tests/core_tests.cpp src/core/SendValidator.cpp src/core/AutoSendEngine.cpp)
    target_link_libraries(core_tests PRIVATE Qt6::Core)
    add_test(NAME core_tests COMMAND core_tests)
    add_executable(editor_tests tests/editor_tests.cpp src/CommandEditor.cpp src/CommandEditor.h)
    target_link_libraries(editor_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME editor_tests COMMAND editor_tests)
    set_tests_properties(editor_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
