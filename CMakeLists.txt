cmake_minimum_required(VERSION 3.16)

project(OpenShell VERSION 0.1 LANGUAGES C CXX)

if(IOS)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Network
    Concurrent
    LinguistTools
)
if(NOT ANDROID AND NOT IOS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

option(OPENSHELL_BUILD_TESTS "Build OpenShell unit tests" OFF)
option(OPENSHELL_ENABLE_ECHO_BACKEND
    "Compile the offline Echo SSH backend into the main app (dev only)" OFF)

# --- libssh2 -------------------------------------------------------------
# 优先用系统 / vcpkg 提供的 libssh2；找不到就 FetchContent 拉源码。
if(NOT ANDROID AND NOT IOS AND NOT APPLE)
    find_package(Libssh2 CONFIG QUIET)
endif()
if(NOT Libssh2_FOUND)
    include(FetchContent)
    message(STATUS "OpenShell: libssh2 not found via find_package, fetching from upstream")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LIBSSH2_NO_DEPRECATED ON CACHE BOOL "" FORCE)
    set(ENABLE_ZLIB_COMPRESSION OFF CACHE BOOL "" FORCE)
    if(ANDROID OR IOS OR APPLE)
        message(STATUS "OpenShell: bundled build uses mbedTLS for libssh2")
        set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(mbedtls
            GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
            GIT_TAG        v3.6.2
        )
        FetchContent_MakeAvailable(mbedtls)
        set(CRYPTO_BACKEND "mbedTLS" CACHE STRING "" FORCE)
        set(MBEDTLS_INCLUDE_DIR "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
        set(MBEDCRYPTO_LIBRARY "${mbedtls_BINARY_DIR}/library/${CMAKE_STATIC_LIBRARY_PREFIX}mbedcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "" FORCE)
    elseif(WIN32)
        set(CRYPTO_BACKEND "WinCNG" CACHE STRING "" FORCE)
    else()
        set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
    endif()
    FetchContent_Declare(libssh2
        GIT_REPOSITORY https://github.com/libssh2/libssh2.git
        GIT_TAG        libssh2-1.11.1
    )
    FetchContent_MakeAvailable(libssh2)
    if((ANDROID OR IOS OR APPLE) AND TARGET libssh2_static AND TARGET mbedcrypto)
        add_dependencies(libssh2_static mbedcrypto)
    endif()
    add_library(Libssh2::libssh2 ALIAS libssh2_static)
endif()

# --- libvterm ------------------------------------------------------------
# 终端模型层。优先找系统 / vcpkg 提供的 libvterm；找不到则从 neovim 上游
# 拉源码（裸 C，无 CMake），用我们自己的目标编译。
set(OPENSHELL_VTERM_TARGET "" CACHE INTERNAL "Resolved libvterm target")
if(NOT ANDROID AND NOT IOS)
    find_package(libvterm CONFIG QUIET)
endif()
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

# --- QtKeychain ----------------------------------------------------------
# 凭据进系统钥匙串（Windows 凭据管理器 / macOS Keychain / Linux libsecret）。
# 只在桌面端启用；移动端凭据留在各自 App 私有沙箱里的 JSON。统一走 FetchContent，
# 这样 BUILD_INTERFACE 的头路径固定，源码里直接 #include <keychain.h>。
set(OPENSHELL_KEYCHAIN_TARGET "" CACHE INTERNAL "Resolved QtKeychain target")
if(NOT ANDROID AND NOT IOS)
    include(FetchContent)
    message(STATUS "OpenShell: fetching QtKeychain from upstream")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
    FetchContent_Declare(qtkeychain
        GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
        GIT_TAG        0.14.3
    )
    FetchContent_MakeAvailable(qtkeychain)
    if(TARGET qt6keychain)
        set(OPENSHELL_KEYCHAIN_TARGET qt6keychain)
    elseif(TARGET Qt6Keychain::Qt6Keychain)
        set(OPENSHELL_KEYCHAIN_TARGET Qt6Keychain::Qt6Keychain)
    else()
        message(FATAL_ERROR "OpenShell: failed to resolve QtKeychain target")
    endif()
    # QtKeychain 只给 install 配了头路径，FetchContent 消费时拿不到，
    # 这里手动补上：源码目录有 keychain.h，构建目录有生成的 qkeychain_export.h。
    set(OPENSHELL_KEYCHAIN_INCLUDE_DIRS
        "${qtkeychain_SOURCE_DIR}" "${qtkeychain_BINARY_DIR}"
        CACHE INTERNAL "QtKeychain include dirs")
endif()

# --- Main executable -----------------------------------------------------

include(cmake/OpenShellSources.cmake)

if(NOT ANDROID AND NOT IOS)
    list(APPEND OPENSHELL_APP_SOURCES ${OPENSHELL_DESKTOP_SOURCES})
endif()

if(OPENSHELL_ENABLE_ECHO_BACKEND)
    list(APPEND OPENSHELL_APP_SOURCES ${OPENSHELL_ECHO_BACKEND_SOURCES})
endif()

qt_add_executable(OpenShell ${OPENSHELL_APP_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_APP_SOURCES})

target_include_directories(OpenShell PRIVATE src)
if(ANDROID OR IOS)
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_MOBILE=1)
endif()
if(OPENSHELL_ENABLE_ECHO_BACKEND)
    target_include_directories(OpenShell PRIVATE tests/fixtures)
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_ENABLE_ECHO_BACKEND=1)
endif()

set(OPENSHELL_PLATFORM_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/platform")
set(OPENSHELL_WINDOWS_ICON "${OPENSHELL_PLATFORM_ICON_DIR}/windows/OpenShell.ico")
set(OPENSHELL_MACOS_ICON "${OPENSHELL_PLATFORM_ICON_DIR}/macos/OpenShell.icns")
set(OPENSHELL_ANDROID_PACKAGE_DIR "${OPENSHELL_PLATFORM_ICON_DIR}/android")
set(OPENSHELL_IOS_ICON_DIR "${OPENSHELL_PLATFORM_ICON_DIR}/ios")

if(WIN32 AND EXISTS "${OPENSHELL_WINDOWS_ICON}")
    set(OPENSHELL_WINDOWS_RC "${CMAKE_CURRENT_BINARY_DIR}/OpenShell.rc")
    file(WRITE "${OPENSHELL_WINDOWS_RC}" "IDI_ICON1 ICON \"${OPENSHELL_WINDOWS_ICON}\"\n")
    target_sources(OpenShell PRIVATE "${OPENSHELL_WINDOWS_RC}")
endif()

if(APPLE AND NOT IOS AND EXISTS "${OPENSHELL_MACOS_ICON}")
    target_sources(OpenShell PRIVATE "${OPENSHELL_MACOS_ICON}")
    set_source_files_properties("${OPENSHELL_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(OpenShell PROPERTIES
        MACOSX_BUNDLE_ICON_FILE OpenShell.icns)
endif()

if(ANDROID AND EXISTS "${OPENSHELL_ANDROID_PACKAGE_DIR}/AndroidManifest.xml")
    set_target_properties(OpenShell PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${OPENSHELL_ANDROID_PACKAGE_DIR}")
endif()

if(IOS AND EXISTS "${OPENSHELL_IOS_ICON_DIR}")
    file(GLOB OPENSHELL_IOS_ICONS CONFIGURE_DEPENDS
        "${OPENSHELL_IOS_ICON_DIR}/*.png")
    if(OPENSHELL_IOS_ICONS)
        target_sources(OpenShell PRIVATE ${OPENSHELL_IOS_ICONS})
        set_source_files_properties(${OPENSHELL_IOS_ICONS} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)
        set(OPENSHELL_IOS_ICON_PLIST_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/SetOpenShellIosIcons.cmake")
        file(WRITE "${OPENSHELL_IOS_ICON_PLIST_SCRIPT}" [=[
if(NOT DEFINED PLIST)
    message(FATAL_ERROR "PLIST is required")
endif()
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Delete :CFBundleIconFile" "${PLIST}" ERROR_QUIET)
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon60x60@2x.png" "${PLIST}")
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Delete :CFBundleIconFiles" "${PLIST}" ERROR_QUIET)
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleIconFiles array" "${PLIST}" RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Failed to add CFBundleIconFiles to ${PLIST}")
endif()
set(_icons
    AppIcon20x20@2x.png
    AppIcon20x20@3x.png
    AppIcon29x29@2x.png
    AppIcon29x29@3x.png
    AppIcon40x40@2x.png
    AppIcon40x40@3x.png
    AppIcon60x60@2x.png
    AppIcon60x60@3x.png
    AppIcon76x76.png
    AppIcon76x76@2x.png
    AppIcon83.5x83.5@2x.png
)
set(_index 0)
foreach(_icon IN LISTS _icons)
    execute_process(
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleIconFiles:${_index} string ${_icon}" "${PLIST}"
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Failed to add ${_icon} to ${PLIST}")
    endif()
    math(EXPR _index "${_index} + 1")
endforeach()
]=])
        add_custom_command(TARGET OpenShell POST_BUILD
            COMMAND ${CMAKE_COMMAND} -DPLIST=$<TARGET_BUNDLE_DIR:OpenShell>/Info.plist -P "${OPENSHELL_IOS_ICON_PLIST_SCRIPT}"
            VERBATIM)
    endif()
endif()

qt_add_qml_module(OpenShell
    URI OpenShell
    QML_FILES
        ${OPENSHELL_QML_FILES}
    RESOURCES
        ${OPENSHELL_QML_RESOURCES}
)

qt_add_translations(OpenShell
    TS_FILES
        translations/OpenShell_zh_CN.ts
        translations/OpenShell_ja_JP.ts
        translations/OpenShell_ko_KR.ts
        translations/OpenShell_de_DE.ts
    RESOURCE_PREFIX
        /i18n
)

set_target_properties(OpenShell PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.wangchaozhi.openshell
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
        Qt6::Network
        Qt6::Concurrent
        Libssh2::libssh2
        ${OPENSHELL_VTERM_TARGET}
)
if(NOT ANDROID AND NOT IOS)
    target_link_libraries(OpenShell PRIVATE Qt6::Widgets ${OPENSHELL_KEYCHAIN_TARGET})
    target_include_directories(OpenShell PRIVATE ${OPENSHELL_KEYCHAIN_INCLUDE_DIRS})
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_USE_KEYCHAIN=1)
endif()

# Platform-specific CredentialStore impl. Desktop uses QtKeychain via
# src/CredentialStore.cpp (already in OPENSHELL_CORE_SOURCES). Mobile gets a
# native-backed impl swapped in here; OPENSHELL_USE_KEYCHAIN is defined so the
# ConnectionCatalog uses the secret-store code path on mobile too.
if(IOS)
    target_sources(OpenShell PRIVATE src/CredentialStoreIos.mm)
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_USE_KEYCHAIN=1)
    set_source_files_properties(src/CredentialStoreIos.mm PROPERTIES
        LANGUAGE OBJCXX
        COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(OpenShell PRIVATE "-framework Security" "-framework Foundation")
elseif(ANDROID)
    target_sources(OpenShell PRIVATE src/CredentialStoreAndroid.cpp)
    target_compile_definitions(OpenShell PRIVATE OPENSHELL_USE_KEYCHAIN=1)
endif()

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

    qt_add_executable(test_connection_catalog ${OPENSHELL_TEST_CONNECTION_CATALOG_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_TEST_CONNECTION_CATALOG_SOURCES})

    target_include_directories(test_connection_catalog PRIVATE src)

    target_link_libraries(test_connection_catalog
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME connection_catalog COMMAND test_connection_catalog)

    qt_add_executable(test_vt_screen ${OPENSHELL_TEST_VT_SCREEN_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_TEST_VT_SCREEN_SOURCES})

    target_include_directories(test_vt_screen PRIVATE src)

    target_link_libraries(test_vt_screen
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            ${OPENSHELL_VTERM_TARGET}
    )

    add_test(NAME vt_screen COMMAND test_vt_screen)

    qt_add_executable(test_session_controller ${OPENSHELL_TEST_SESSION_CONTROLLER_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_TEST_SESSION_CONTROLLER_SOURCES})

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

    qt_add_executable(test_sftp_transfer ${OPENSHELL_TEST_SFTP_TRANSFER_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_TEST_SFTP_TRANSFER_SOURCES})
    target_include_directories(test_sftp_transfer PRIVATE src)
    target_link_libraries(test_sftp_transfer
        PRIVATE
            Qt6::Core
            Qt6::Test
            Libssh2::libssh2
    )
    add_test(NAME sftp_transfer COMMAND test_sftp_transfer)

    qt_add_executable(test_sftp_connection_pool ${OPENSHELL_TEST_SFTP_CONNECTION_POOL_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${OPENSHELL_TEST_SFTP_CONNECTION_POOL_SOURCES})
    target_include_directories(test_sftp_connection_pool PRIVATE src)
    target_link_libraries(test_sftp_connection_pool
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
            Libssh2::libssh2
    )
    add_test(NAME sftp_connection_pool COMMAND test_sftp_connection_pool)
endif()
