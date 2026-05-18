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

# --- libvterm ------------------------------------------------------------
# 终端模型层。优先找系统 / vcpkg 提供的 libvterm；找不到则从 neovim 上游
# 拉源码（裸 C，无 CMake），用我们自己的目标编译。
set(OPENSHELL_VTERM_TARGET "" CACHE INTERNAL "Resolved libvterm target")
find_package(libvterm CONFIG QUIET)
if(libvterm_FOUND)
    set(OPENSHELL_VTERM_TARGET libvterm::libvterm)
else()
    find_path(LIBVTERM_INCLUDE_DIR vterm.h)
    find_library(LIBVTERM_LIBRARY NAMES vterm libvterm)
    if(LIBVTERM_INCLUDE_DIR AND LIBVTERM_LIBRARY)
        add_library(openshell_libvterm INTERFACE)
        target_include_directories(openshell_libvterm INTERFACE "${LIBVTERM_INCLUDE_DIR}")
        target_link_libraries(openshell_libvterm INTERFACE "${LIBVTERM_LIBRARY}")
        set(OPENSHELL_VTERM_TARGET openshell_libvterm)
    else()
        include(FetchContent)
        message(STATUS "OpenShell: libvterm not found via find_package, fetching from upstream")
        FetchContent_Declare(libvterm_src
            GIT_REPOSITORY https://github.com/neovim/libvterm.git
            GIT_TAG        v0.3.3
        )
        FetchContent_MakeAvailable(libvterm_src)
        set(_vterm_root "${libvterm_src_SOURCE_DIR}")

        # encoding.c #include "encoding/*.inc"，上游用 tbl2inc_c.pl 在构建期生成。
        # 我们既不能假设环境里有 Perl，也不愿在源码树写脚本，所以这里用纯 CMake
        # 复刻一遍同样的转换：把 src/encoding/*.tbl 转成同名 .inc。
        function(_openshell_libvterm_tbl_to_inc tbl_path inc_path)
            get_filename_component(_enc_name "${tbl_path}" NAME_WE)
            file(READ "${tbl_path}" _tbl_text)
            string(REPLACE "\r\n" "\n" _tbl_text "${_tbl_text}")
            string(REPLACE "\n" ";" _tbl_lines "${_tbl_text}")
            set(_body "")
            foreach(_line IN LISTS _tbl_lines)
                string(REGEX REPLACE "[ \t]*#.*" "" _line "${_line}")
                string(STRIP "${_line}" _line)
                if(_line STREQUAL "")
                    continue()
                endif()
                set(_idx_a "")
                set(_idx_b "")
                set(_cp_hex "")
                if(_line MATCHES "^([0-9]+)/([0-9]+)[ \t]*=[ \t]*U\\+([0-9A-Fa-f]+)$")
                    set(_idx_a "${CMAKE_MATCH_1}")
                    set(_idx_b "${CMAKE_MATCH_2}")
                    set(_cp_hex "0x${CMAKE_MATCH_3}")
                elseif(_line MATCHES "^([0-9]+)/([0-9]+)[ \t]*=[ \t]*\"([^\"]+)\"$")
                    set(_idx_a "${CMAKE_MATCH_1}")
                    set(_idx_b "${CMAKE_MATCH_2}")
                    set(_ch "${CMAKE_MATCH_3}")
                    string(HEX "${_ch}" _bytes_hex)
                    string(LENGTH "${_bytes_hex}" _hexlen)
                    set(_cp 0)
                    if(_hexlen EQUAL 2)
                        math(EXPR _cp "0x${_bytes_hex}")
                    elseif(_hexlen EQUAL 4)
                        string(SUBSTRING "${_bytes_hex}" 0 2 _b1)
                        string(SUBSTRING "${_bytes_hex}" 2 2 _b2)
                        math(EXPR _cp
                            "((0x${_b1} & 0x1F) << 6) | (0x${_b2} & 0x3F)")
                    elseif(_hexlen EQUAL 6)
                        string(SUBSTRING "${_bytes_hex}" 0 2 _b1)
                        string(SUBSTRING "${_bytes_hex}" 2 2 _b2)
                        string(SUBSTRING "${_bytes_hex}" 4 2 _b3)
                        math(EXPR _cp
                            "((0x${_b1} & 0x0F) << 12) | ((0x${_b2} & 0x3F) << 6) | (0x${_b3} & 0x3F)")
                    else()
                        continue()
                    endif()
                    math(EXPR _cp_hex "${_cp}" OUTPUT_FORMAT HEXADECIMAL)
                endif()
                if(NOT _idx_a STREQUAL "" AND NOT _cp_hex STREQUAL "")
                    math(EXPR _idx "${_idx_a} * 16 + ${_idx_b}" OUTPUT_FORMAT HEXADECIMAL)
                    string(APPEND _body "    [${_idx}] = ${_cp_hex},\n")
                endif()
            endforeach()
            file(WRITE "${inc_path}"
"static const struct StaticTableEncoding encoding_${_enc_name} = {
  { .decode = &decode_table },
  {
${_body}  }
};
")
        endfunction()
        file(GLOB _vterm_tbls "${_vterm_root}/src/encoding/*.tbl")
        foreach(_tbl IN LISTS _vterm_tbls)
            get_filename_component(_tbl_name "${_tbl}" NAME_WE)
            set(_inc "${_vterm_root}/src/encoding/${_tbl_name}.inc")
            if(NOT EXISTS "${_inc}" OR "${_tbl}" IS_NEWER_THAN "${_inc}")
                _openshell_libvterm_tbl_to_inc("${_tbl}" "${_inc}")
            endif()
        endforeach()

        file(GLOB _vterm_sources
            "${_vterm_root}/src/encoding.c"
            "${_vterm_root}/src/keyboard.c"
            "${_vterm_root}/src/mouse.c"
            "${_vterm_root}/src/parser.c"
            "${_vterm_root}/src/pen.c"
            "${_vterm_root}/src/screen.c"
            "${_vterm_root}/src/state.c"
            "${_vterm_root}/src/unicode.c"
            "${_vterm_root}/src/vterm.c"
        )
        add_library(openshell_libvterm STATIC ${_vterm_sources})
        target_include_directories(openshell_libvterm
            PUBLIC  "${_vterm_root}/include"
            PRIVATE "${_vterm_root}/src"
        )
        target_compile_definitions(openshell_libvterm PRIVATE
            HAVE_CONFIG_H=0
            VSNPRINTF=vsnprintf
            SNPRINTF=snprintf
        )
        if(MSVC)
            target_compile_options(openshell_libvterm PRIVATE /wd4244 /wd4267 /wd4018)
            target_compile_definitions(openshell_libvterm PRIVATE _CRT_SECURE_NO_WARNINGS)
        else()
            target_compile_options(openshell_libvterm PRIVATE -Wno-unused-parameter)
        endif()
        set(OPENSHELL_VTERM_TARGET openshell_libvterm)
    endif()
endif()
if(NOT OPENSHELL_VTERM_TARGET)
    message(FATAL_ERROR "OpenShell: failed to resolve libvterm")
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
    src/SystemMonitorController.cpp
    src/SystemMonitorController.h
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
    src/terminal/VtScreen.cpp
    src/terminal/VtScreen.h
    src/terminal/TerminalScreenItem.cpp
    src/terminal/TerminalScreenItem.h
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
        qml/ConnectionManagerView.qml
        qml/ConnectionEditor.qml
        qml/SessionTabs.qml
        qml/TerminalView.qml
        qml/SystemInfoView.qml
        qml/FileBrowser.qml
        qml/AccentCard.qml
)

qt_add_translations(OpenShell
    TS_FILES
        translations/OpenShell_zh_CN.ts
        translations/OpenShell_ja_JP.ts
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
        ${OPENSHELL_VTERM_TARGET}
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
        src/terminal/VtScreen.cpp
        src/terminal/VtScreen.h
    )

    target_include_directories(test_session_controller PRIVATE src tests/fixtures)

    target_link_libraries(test_session_controller
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Test
            Libssh2::libssh2
            ${OPENSHELL_VTERM_TARGET}
    )

    add_test(NAME session_controller COMMAND test_session_controller)
endif()
