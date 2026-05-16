cmake_minimum_required(VERSION 3.16)

project(OpenShell VERSION 0.1 LANGUAGES CXX)

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

qt_add_executable(OpenShell
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
    src/ssh/EchoChannelWorker.cpp
    src/ssh/EchoChannelWorker.h
    src/ssh/SshSession.cpp
    src/ssh/SshSession.h
)

target_include_directories(OpenShell PRIVATE src)

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
)

install(TARGETS OpenShell
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY assets
    DESTINATION .
)

option(OPENSHELL_BUILD_TESTS "Build OpenShell unit tests" OFF)

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
        src/ConnectionCatalog.cpp
        src/ConnectionCatalog.h
        src/SessionController.cpp
        src/SessionController.h
        src/ssh/SshChannelWorker.cpp
        src/ssh/SshChannelWorker.h
        src/ssh/EchoChannelWorker.cpp
        src/ssh/EchoChannelWorker.h
        src/ssh/SshSession.cpp
        src/ssh/SshSession.h
    )

    target_include_directories(test_session_controller PRIVATE src)

    target_link_libraries(test_session_controller
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME session_controller COMMAND test_session_controller)
endif()
