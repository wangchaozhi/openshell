cmake_minimum_required(VERSION 3.16)

project(OpenShell VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Widgets
    Network
    Concurrent
    LinguistTools
)

qt_standard_project_setup(REQUIRES 6.6)

option(OPENSHELL_BUILD_TESTS "Build OpenShell unit tests" OFF)
option(OPENSHELL_ENABLE_ECHO_BACKEND
    "Compile the offline Echo SSH backend into the main app (dev only)" OFF)

# --- libssh2 -------------------------------------------------------------
# 优先用系统 / vcpkg 提供的 libssh2；找不到就 FetchContent 拉源码。
find_package(Libssh2 CONFIG QUIET)
if(NOT Libssh2_FOUND)
    include(FetchContent)
    message(STATUS "OpenShell: libssh2 not found via find_package, fetching from upstream")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LIBSSH2_NO_DEPRECATED ON CACHE BOOL "" FORCE)
    set(ENABLE_ZLIB_COMPRESSION OFF CACHE BOOL "" FORCE)
    if(WIN32)
        set(CRYPTO_BACKEND "WinCNG" CACHE STRING "" FORCE)
    elseif(APPLE)
        set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
    else()
        set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
    endif()
    FetchContent_Declare(libssh2
        GIT_REPOSITORY https://github.com/libssh2/libssh2.git
        GIT_TAG        libssh2-1.11.1
    )
    FetchContent_MakeAvailable(libssh2)
    add_library(Libssh2::libssh2 ALIAS libssh2_static)
endif()

# --- Main executable -----------------------------------------------------

set(OPENSHELL_SOURCES
    main.cpp
    src/AppController.cpp
    src/AppController.h
    src/ConnectionCatalog.cpp
    src/ConnectionCatalog.h
    src/SessionController.cpp
    src/SessionController.h
    src/SettingsStore.cpp
    src/SettingsStore.h
    src/TranslationManager.cpp
    src/TranslationManager.h
    src/TrayController.cpp
    src/TrayController.h
    src/ssh/SshChannelWorker.cpp
    src/ssh/SshChannelWorker.h
    src/ssh/SshSession.cpp
    src/ssh/SshSession.h
    src/ssh/SshWorkerFactory.h
    src/ssh/SftpDirectoryLister.cpp
    src/ssh/SftpDirectoryLister.h
    src/ssh/Libssh2ChannelWorker.cpp
    src/ssh/Libssh2ChannelWorker.h
)

if(OPENSHELL_ENABLE_ECHO_BACKEND)
    list(APPEND OPENSHELL_SOURCES
        tests/fixtures/EchoChannelWorker.cpp
        tests/fixtures/EchoChannelWorker.h)
endif()

qt_add_executable(OpenShell ${OPENSHELL_SOURCES})

target_include_directories(OpenShell PRIVATE src)
if(OPENSHELL_ENABLE_ECHO_BACKEND)
    target_include_directories(OpenShell PRIVATE tests/fixtures)
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_ENABLE_ECHO_BACKEND=1)
endif()

qt_add_qml_module(OpenShell
    URI OpenShell
    QML_FILES
        Main.qml
        qml/MainWindow.qml
        qml/Sidebar.qml
        qml/ConnectionList.qml
        qml/ConnectionEditor.qml
        qml/SessionTabs.qml
        qml/TerminalView.qml
        qml/FileBrowser.qml
        qml/AccentCard.qml
)

qt_add_translations(OpenShell
    TS_FILES
        translations/OpenShell_zh_CN.ts
    RESOURCE_PREFIX
        /i18n
)

set_target_properties(OpenShell PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(OpenShell
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        Qt6::Network
        Qt6::Concurrent
        Libssh2::libssh2
)

install(TARGETS OpenShell
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY assets
    DESTINATION .
)

if(OPENSHELL_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()

    qt_add_executable(test_connection_catalog
        tests/test_connection_catalog.cpp
        src/ConnectionCatalog.cpp
        src/ConnectionCatalog.h
    )

    target_include_directories(test_connection_catalog PRIVATE src)

    target_link_libraries(test_connection_catalog
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME connection_catalog COMMAND test_connection_catalog)

    qt_add_executable(test_session_controller
        tests/test_session_controller.cpp
        tests/fixtures/EchoChannelWorker.cpp
        tests/fixtures/EchoChannelWorker.h
        src/ConnectionCatalog.cpp
        src/ConnectionCatalog.h
        src/SessionController.cpp
        src/SessionController.h
        src/ssh/SshChannelWorker.cpp
        src/ssh/SshChannelWorker.h
        src/ssh/Libssh2ChannelWorker.cpp
        src/ssh/Libssh2ChannelWorker.h
        src/ssh/SshSession.cpp
        src/ssh/SshSession.h
    )

    target_include_directories(test_session_controller PRIVATE src tests/fixtures)

    target_link_libraries(test_session_controller
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
            Libssh2::libssh2
    )

    add_test(NAME session_controller COMMAND test_session_controller)
endif()
