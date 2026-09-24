cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)
include(CTest)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Svg)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Svg)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )

  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    if(Qt6_VERSION VERSION_LESS "6.9.0")
      find_package(Qt6 COMPONENTS GuiPrivate)
      if(TARGET Qt6::GuiPrivate)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::GuiPrivate)
      endif()
    endif()
  endif()
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/capture-source-share.cpp
    src/capture-source-share.hpp
    src/shorts-dock.cpp
    src/shorts-dock.hpp
    src/vertical-production-dock.cpp
    src/vertical-production-dock.hpp
    src/vertical-scenes-dock.cpp
    src/vertical-scenes-dock.hpp
    src/vertical-sources-dock.cpp
    src/vertical-sources-dock.hpp
    src/vertical-transitions-dock.cpp
    src/vertical-transitions-dock.hpp
    src/plugin-settings.hpp
    src/stream-destination.hpp
    src/credential-store.cpp
    src/credential-store.hpp
    src/platform-logo.cpp
    src/platform-logo.hpp
    src/platform-selector.cpp
    src/platform-selector.hpp
    src/settings-dialog.cpp
    src/settings-dialog.hpp
    src/vertical-outputs.cpp
    src/vertical-outputs.hpp
    src/recording-automation.cpp
    src/recording-automation.hpp
    src/audio-mixer-panel.cpp
    src/audio-mixer-panel.hpp
    src/qt-display.cpp
    src/qt-display.hpp
    src/display-helpers.hpp
    src/vsp-resources.qrc
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Unit tests (Qt-only / no OBS link). Enabled via BUILD_TESTING (CTest) or VSP_BUILD_TESTS.
option(VSP_BUILD_TESTS "Build Vertical Shorts unit tests (also enabled when BUILD_TESTING=ON)" OFF)
if(BUILD_TESTING OR VSP_BUILD_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Core)
  add_executable(test_settings tests/test_settings.cpp)
  target_include_directories(test_settings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(test_settings PRIVATE Qt6::Core)
  target_compile_definitions(test_settings PRIVATE VSP_SETTINGS_TEST=1)
  add_test(NAME settings_validation COMMAND test_settings)

  add_executable(test_stream_destination tests/test_stream_destination.cpp)
  target_include_directories(test_stream_destination PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_stream_destination PRIVATE Qt6::Core)
  # Normalize path separators so the compile definition is a valid C string on Windows.
  string(REPLACE "\\" "/" _vsp_src_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(test_stream_destination PRIVATE VSP_SETTINGS_TEST=1 "VSP_SOURCE_DIR=\"${_vsp_src_dir}\"")
  add_test(NAME stream_destination COMMAND test_stream_destination)

  add_executable(test_mixer_model tests/test_mixer_model.cpp)
  target_include_directories(test_mixer_model PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(test_mixer_model PRIVATE Qt6::Core)
  target_compile_definitions(test_mixer_model PRIVATE VSP_SETTINGS_TEST=1)
  add_test(NAME mixer_model COMMAND test_mixer_model)

  add_executable(test_credential_store tests/test_credential_store.cpp src/credential-store.cpp)
  target_include_directories(test_credential_store PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(test_credential_store PRIVATE Qt6::Core)
  add_test(NAME credential_store COMMAND test_credential_store)

  add_custom_target(vsp_tests DEPENDS test_settings test_stream_destination test_mixer_model test_credential_store)

  # Keep test binaries out of plugin install/package trees.
  set_target_properties(test_settings test_stream_destination test_mixer_model test_credential_store PROPERTIES EXCLUDE_FROM_ALL FALSE)
endif()
