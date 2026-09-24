# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  set(arch ${CMAKE_VS_PLATFORM_NAME})
  set(platform windows-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  # OBS 32+ ships full sources (frontend included) as a release asset.
  set(obs-studio_filename "OBS-Studio-VERSION-Sources.tar.gz")
  set(obs-studio_destination "obs-studio-VERSION-sources")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()
endfunction()

_check_dependencies_windows()
