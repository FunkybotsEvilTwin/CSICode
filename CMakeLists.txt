cmake_minimum_required(VERSION 3.5)
project(reaper_csurf_integrator VERSION 7.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# path for external 3rd party library dependencies
include(FetchContent)
set(PROJECT_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

FetchContent_Declare(WDL
    GIT_REPOSITORY https://github.com/justinfrankel/WDL
    GIT_TAG        "origin/main" # or specify a tag or branch here
    SOURCE_DIR     "${PROJECT_LIB_DIR}/WDL"
    )

FetchContent_Declare(reaper-sdk
    GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk
    GIT_TAG        "origin/main" # or specify a tag or branch here
    SOURCE_DIR     "${PROJECT_LIB_DIR}/reaper-sdk"
    )

FetchContent_MakeAvailable(WDL reaper-sdk)

# symlink WDL to reaper-sdk
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            # source
            "${PROJECT_LIB_DIR}/WDL/WDL"
            # target
            "${PROJECT_LIB_DIR}/reaper-sdk/WDL"
)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
endif()

if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

FILE(GLOB header_paths
    ${PROJECT_LIB_DIR}/*/include
    ${PROJECT_LIB_DIR}/*/sdk
    )

add_library(reaper-sdk IMPORTED INTERFACE)
target_include_directories(reaper-sdk INTERFACE ${header_paths})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(WDL REQUIRED)

if(NOT WIN32)
  find_package(SWELL REQUIRED)
endif()

target_link_libraries(reaper-sdk INTERFACE WDL::WDL)

if(SWELL_FOUND)
  target_link_libraries(reaper-sdk INTERFACE SWELL::swell)

  # Transpile the Win32 resource file
  # find_package(PHP REQUIRED)
  # add_custom_command(
  #   OUTPUT  resource.rc_mac_dlg resource.rc_mac_menu
  #   COMMAND ${CMAKE_COMMAND} -E copy
  #     ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc ${CMAKE_CURRENT_BINARY_DIR}
  #   COMMAND ${PHP_EXECUTABLE} ${SWELL_RESGEN} --quiet resource.rc
  #   DEPENDS ${SWELL_RESGEN}
  #   MAIN_DEPENDENCY resource.rc
  # )
endif()

# if(WIN32)
#   add_link_options(
#     # Use relative path to the PDB file to avoid exposing the full path
#     /PDBALTPATH:%_PDB%
#   )
# endif()


set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/reaper_csurf_integrator)
set(WDL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/WDL/WDL)
set(RES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/reaper_csurf_integrator)

if(NOT WIN32)
    # Run the swell resource generation script
    execute_process(
        COMMAND perl ${WDL_PATH}/swell/swell_resgen.pl --quiet ${SRC_PATH}/res.rc
        WORKING_DIRECTORY ${SRC_PATH}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
endif()




if (UNIX)
    find_package(Perl REQUIRED)
    execute_process(COMMAND ${PERL_EXECUTABLE} ${WDL_PATH}/swell/swell_resgen.pl --quiet ${RES_PATH}/res.rc)
endif ()

set(TARGET reaper_csurf_integrator)
set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} SHARED
    # ${SRC_PATH}/control_surface_action_contexts.h
    # ${SRC_PATH}/control_surface_integrator.cpp
    # ${SRC_PATH}/control_surface_integrator.h
    # ${SRC_PATH}/control_surface_integrator_Reaper.h
    # ${SRC_PATH}/control_surface_integrator_ui.cpp
    # ${SRC_PATH}/control_surface_manager_actions.h
    # ${SRC_PATH}/control_surface_midi_widgets.h
    # ${SRC_PATH}/control_surface_OSC_widgets.h
    # ${SRC_PATH}/control_surface_Reaper_actions.h
    # ${SRC_PATH}/handy_functions.h
    # ${SRC_PATH}/main.cpp
    # ${SRC_PATH}/oscpkt.hh
    # ${SRC_PATH}/reaper_plugin.h
    # ${SRC_PATH}/reaper_plugin_functions.h
    # ${SRC_PATH}/resource.h
    # ${SRC_PATH}/udp.hh
    ${WDL_PATH}/win32_utf8.c
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME
    DESTINATION $ENV{REAPER_PATH}/UserPlugins
)

add_subdirectory(reaper_csurf_integrator) #TODO: change to src 
target_link_libraries(${PROJECT_NAME} PRIVATE reaper-sdk)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

if(WIN32)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  target_compile_options(
    ${PROJECT_NAME}
      PRIVATE
      /W3 /WX)
endif()
if(UNIX AND NOT APPLE)
  target_compile_options(${PROJECT_NAME} PRIVATE -fno-unsigned-char -include stddef.h)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(USER_CONFIG_DIR "$ENV{APPDATA}")
  elseif(APPLE)
    set(USER_CONFIG_DIR "~/Library/Application Support")
  else()
    set(USER_CONFIG_DIR "~/.config")
  endif()

  set(CMAKE_INSTALL_PREFIX "${USER_CONFIG_DIR}/REAPER" CACHE PATH
    "REAPER resource path where to install ReaPack" FORCE)
endif()

if(CMAKE_OSX_ARCHITECTURES)
  list(JOIN CMAKE_OSX_ARCHITECTURES "-" ARCH_NAME)
elseif(MSVC_CXX_ARCHITECTURE_ID)
  set(ARCH_NAME ${MSVC_CXX_ARCHITECTURE_ID})
else()
  set(ARCH_NAME ${CMAKE_SYSTEM_PROCESSOR})
endif()

STRING(TOLOWER "${ARCH_NAME}" ARCH_NAME)
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX "" # disable the "lib" prefix
  OUTPUT_NAME "${PROJECT_NAME}"
  RUNTIME_OUTPUT_DIRECTORY "${REAPER_RESOURCE_PATH}/UserPlugins"
)

set(REAPER_USER_PLUGINS "UserPlugins")
install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  RUNTIME DESTINATION "${REAPER_USER_PLUGINS}" # Windows .dll
  LIBRARY DESTINATION "${REAPER_USER_PLUGINS}" # Linux .so/macOS .dylib
)