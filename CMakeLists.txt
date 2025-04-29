cmake_minimum_required(VERSION 3.5)

# Enable policy for INTERPROCEDURAL_OPTIMIZATION if supported
if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()

# ------------------------------------------------------------------------------
# Project Definition and Versioning
# ------------------------------------------------------------------------------

project(ControlSurfaceIntegrator VERSION 7.1.0 LANGUAGES C CXX)

# Attempt to extract latest Git tag as project version
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_LATEST_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
    string(REGEX REPLACE "[^0-9.]" "" GIT_LATEST_TAG ${GIT_LATEST_TAG})
    if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
      project(${PROJECT_NAME} VERSION ${GIT_LATEST_TAG} LANGUAGES C CXX)
    endif()
  endif()
endif()
message("Project version: ${PROJECT_VERSION}")

# ------------------------------------------------------------------------------
# Build Number and Commit Info (e.g., for CI)
# ------------------------------------------------------------------------------

if(DEFINED ENV{APPVEYOR})
  set(CMAKE_PROJECT_VERSION_TWEAK $ENV{APPVEYOR_BUILD_NUMBER})
  set(CMAKE_PROJECT_VERSION_COMMIT $ENV{APPVEYOR_REPO_COMMIT})
else()
  set(CMAKE_PROJECT_VERSION_TWEAK -1)
  set(CMAKE_PROJECT_VERSION_COMMIT a)
endif()
set(CMAKE_PROJECT_VERSION_COMMIT "\"${CMAKE_PROJECT_VERSION_COMMIT}\"")

# ------------------------------------------------------------------------------
# General Build Settings
# ------------------------------------------------------------------------------

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# External Dependencies: FetchContent
# ------------------------------------------------------------------------------

include(FetchContent)
set(PROJECT_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

FetchContent_Declare(reaper-sdk
  GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk
  GIT_TAG        origin/main
  SOURCE_DIR     "${PROJECT_LIB_DIR}/reaper-sdk"
)

FetchContent_Declare(WDL
  GIT_REPOSITORY https://github.com/justinfrankel/WDL
  GIT_TAG        origin/main
  SOURCE_DIR     "${PROJECT_LIB_DIR}/WDL"
)

FetchContent_Declare(GSL
  GIT_REPOSITORY "https://github.com/microsoft/GSL"
  GIT_TAG        origin/main
  SOURCE_DIR     "${PROJECT_LIB_DIR}/GSL"
)

FetchContent_MakeAvailable(reaper-sdk GSL WDL)

# ------------------------------------------------------------------------------
# Symlink WDL into reaper-sdk
# ------------------------------------------------------------------------------

execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          "${PROJECT_LIB_DIR}/WDL/WDL"
          "${PROJECT_LIB_DIR}/reaper-sdk/WDL"
)

# ------------------------------------------------------------------------------
# VCPKG Integration
# ------------------------------------------------------------------------------

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

# ------------------------------------------------------------------------------
# Header Paths and Compiler Flags
# ------------------------------------------------------------------------------

set(header_paths ${PROJECT_LIB_DIR}/reaper-sdk/sdk
  # add possible include directories for reaper plugin
)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${PROJECT_LIB_DIR}/*.h*)

foreach(header ${headers})
  if(WIN32)
    set_source_files_properties(${header} PROPERTIES COMPILE_FLAGS "/W0")
  else()
    set_source_files_properties(${header} PROPERTIES COMPILE_FLAGS "-w")
  endif()
endforeach()

# ------------------------------------------------------------------------------
# Setup Interface Library for reaper-sdk
# ------------------------------------------------------------------------------

add_library(reaper-sdk INTERFACE)
target_include_directories(reaper-sdk INTERFACE ${header_paths} ${CMAKE_BINARY_DIR})
target_link_libraries(reaper-sdk INTERFACE GSL)

# ------------------------------------------------------------------------------
# Find and Link WDL + SWELL
# ------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(WDL REQUIRED)
if(NOT WIN32)
  find_package(SWELL REQUIRED)
endif()

target_link_libraries(reaper-sdk INTERFACE WDL::WDL)
if(SWELL_FOUND)
  target_link_libraries(reaper-sdk INTERFACE SWELL::swell)
endif()

# ------------------------------------------------------------------------------
# Interprocedural Optimization (LTO)
# ------------------------------------------------------------------------------

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL     ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

if(VCPKG_TOOLCHAIN)
  set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
  set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()

# ------------------------------------------------------------------------------
# Source Files for Main Library
# ------------------------------------------------------------------------------

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/reaper_csurf_integrator)
set(WDL_PATH ${PROJECT_LIB_DIR}/WDL/WDL)

# ------------------------------------------------------------------------------
# SWELL Resource File Generation (non-Windows platforms only)
# ------------------------------------------------------------------------------

if(NOT WIN32)
  # Ensure Perl is available
  find_package(Perl REQUIRED)

  # Generate SWELL resources from .rc file using Perl script
  execute_process(
    COMMAND ${PERL_EXECUTABLE} ${WDL_PATH}/swell/swell_resgen.pl --quiet ${SRC_PATH}/res.rc
    WORKING_DIRECTORY ${SRC_PATH}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
  )

  # Optional: report result if debugging
  if(NOT result EQUAL 0)
    message(WARNING "SWELL resource generation failed with error: ${error_output}")
  endif()
endif()

add_library(${PROJECT_NAME} SHARED
  ${WDL_PATH}/win32_utf8.c
)

add_subdirectory(reaper_csurf_integrator) #TODO: move and change to src 
target_link_libraries(${PROJECT_NAME} PRIVATE reaper-sdk)


# ------------------------------------------------------------------------------
# Configure singing for macos 15+ 
# ------------------------------------------------------------------------------
if(APPLE)
  # Force building Universal2 binary: Intel + Apple Silicon
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
  message(STATUS "Building Universal2 binary for macOS (x86_64 + arm64)")

  # Set minimum macOS deployment target for best compatibility
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
  message(STATUS "Setting minimum deployment target to macOS 11.0 (Big Sur)")

  # Code Signing if identity is set
  if(DEFINED ENV{APLDEV_CODESIGN_IDENTITY})
    message(STATUS "Code signing will be applied with identity: $ENV{APLDEV_CODESIGN_IDENTITY}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND codesign
              --timestamp
              --options runtime
              --force
              --sign "$ENV{APLDEV_CODESIGN_IDENTITY}"
              $<TARGET_FILE:${PROJECT_NAME}>
      COMMENT "Code signing completed with Developer ID certificate."
      VERBATIM
    )
  else()
    message(WARNING "Skipping code signing: CSI_CODESIGN_IDENTITY environment variable not set.")
  endif()

  # Notarization if username, password, and team ID are set
  if(DEFINED ENV{APLDEV_NOTARY_USERNAME} AND DEFINED ENV{APLDEV_NOTARY_PASSWORD} AND DEFINED ENV{APLDEV_TEAM_ID})
    message(STATUS "Notarization will be submitted to Apple ID: $ENV{APLDEV_NOTARY_USERNAME}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND xcrun notarytool submit $<TARGET_FILE:${PROJECT_NAME}> 
              --apple-id "$ENV{APLDEV_NOTARY_USERNAME}" 
              --team-id "$ENV{APLDEV_TEAM_ID}" 
              --password "$ENV{APLDEV_NOTARY_PASSWORD}" 
              --wait
      COMMENT "Apple notarization process started and will wait for completion."
      VERBATIM
    )
  else()
    message(WARNING "Skipping notarization: CSI_NOTARY_USERNAME, CSI_NOTARY_PASSWORD, or CSI_TEAM_ID environment variables not set.")
  endif()
endif()

# ------------------------------------------------------------------------------
# Configure Generated Header
# ------------------------------------------------------------------------------

configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

# ------------------------------------------------------------------------------
# Platform-Specific Compiler Flags and Linker Options
# ------------------------------------------------------------------------------

if(WIN32)
  foreach(arg
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL
  )
    # Embed debug information in the object files
    string(REGEX REPLACE "/Z[iI]" "/Z7" ${arg} "${${arg}}")
  endforeach()

  add_compile_options(
    # Exception handling model
    /EHsc

    # Enhance optimized debugging
    $<$<NOT:$<CONFIG:Debug>>:/Zo>

    # Eliminate duplicate strings
    $<$<NOT:$<CONFIG:Debug>>:/GF>

    # Enable function-level linking
    $<$<NOT:$<CONFIG:Debug>>:/Gy>

    # Remove unreferenced COMDAT
    $<$<NOT:$<CONFIG:Debug>>:/Zc:inline>
  )

  if(NOT CMAKE_PROJECT_VERSION_TWEAK EQUAL "1")
    add_link_options(
      # Remove unreferenced code
      $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>

      # Remove duplicate sections
      $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>

      # Use relative path to the PDB file to avoid exposing the full path
      /PDBALTPATH:%_PDB%
    )
  endif()
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS -fno-unsigned-char -fstack-protector-strong -fdiagnostics-color)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

# Platform-specific compile warnings
if(WIN32)
  target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4996)
  target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
  if(UNIX AND NOT APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE -include stddef.h)
  endif()
endif()

# ------------------------------------------------------------------------------ 
# Additional Mac-specific: Universal Binary + Code Signing and Notarization
# ------------------------------------------------------------------------------

# INSTRUCTIONS:
# If you are building for distribution on macOS and want the output to be
# Code Signed and Notarized automatically:
#
# 1. Ensure your Developer ID Application certificate is installed in your Keychain.
#
# 2. Before building, set the following environment variables:
#
#    CSI_CODESIGN_IDENTITY   - Your full Developer ID Application identity, e.g.,
#                              "Developer ID Application: Your Name (TEAMID)"
#
#    CSI_NOTARY_USERNAME     - Your Apple ID email (for notarization).
#
#    CSI_NOTARY_PASSWORD     - App-specific password or saved keychain reference.
#
#    CSI_TEAM_ID             - Your 10-character Apple Developer Team ID.
#
# Example (temporary for session):
#   export CSI_CODESIGN_IDENTITY="Developer ID Application: John Doe (ABCDE12345)"
#   export CSI_NOTARY_USERNAME="your_email@example.com"
#   export CSI_NOTARY_PASSWORD="@keychain:CSI_Notarization"
#   export CSI_TEAM_ID="ABCDE12345"
#
# If these environment variables are NOT set, the build will complete,
# but signing and notarization will be skipped with a friendly message.

# ------------------------------------------------------------------------------ 
# Additional Mac-specific: Universal Binary + Code Signing and Notarization
# ------------------------------------------------------------------------------

if(APPLE)
  # Force building Universal2 binary: Intel + Apple Silicon
  set(CMAKE_OSX_ARCHITECTURE_BUILD "separate" CACHE STRING "" FORCE)
  message(STATUS "Building Universal2 binary for macOS (x86_64 + arm64)")

  # Code Signing if identity is set
  if(DEFINED ENV{CSI_CODESIGN_IDENTITY})
    message(STATUS "Code signing will be applied with identity: $ENV{CSI_CODESIGN_IDENTITY}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND codesign
              --timestamp
              --options runtime
              --force
              --sign "$ENV{CSI_CODESIGN_IDENTITY}"
              $<TARGET_FILE:${PROJECT_NAME}>
      COMMENT "Code signing completed with Developer ID certificate."
      VERBATIM
    )
  else()
    message(WARNING "Skipping code signing: CSI_CODESIGN_IDENTITY environment variable not set.")
  endif()

  # Notarization if username, password, and team ID are set
  if(DEFINED ENV{CSI_NOTARY_USERNAME} AND DEFINED ENV{CSI_NOTARY_PASSWORD} AND DEFINED ENV{CSI_TEAM_ID})
    message(STATUS "Notarization will be submitted to Apple ID: $ENV{CSI_NOTARY_USERNAME}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E tar "cfv" "$<TARGET_FILE:${PROJECT_NAME}>.zip" --format=zip "$<TARGET_FILE:${PROJECT_NAME}>"
      COMMAND xcrun notarytool submit "$<TARGET_FILE:${PROJECT_NAME}>.zip"
              --apple-id "$ENV{CSI_NOTARY_USERNAME}"
              --team-id "$ENV{CSI_TEAM_ID}"
              --password "$ENV{CSI_NOTARY_PASSWORD}"
              --wait
      COMMENT "Apple notarization process started and will wait for completion (via .zip archive)."
      VERBATIM
    )
  else()
    message(WARNING "Skipping notarization: CSI_NOTARY_USERNAME, CSI_NOTARY_PASSWORD, or CSI_TEAM_ID environment variables not set.")
  endif()
endif()

# ------------------------------------------------------------------------------
# Determine REAPER_USER_PLUGINS path
# ------------------------------------------------------------------------------

if(DEFINED ENV{REAPER_RESOURCE_PATH})
  file(TO_CMAKE_PATH "$ENV{REAPER_RESOURCE_PATH}" REAPER_RESOURCE_PATH)
  if(EXISTS "${REAPER_RESOURCE_PATH}")
    set(REAPER_USER_PLUGINS "${REAPER_RESOURCE_PATH}/UserPlugins")
    message(STATUS "Using REAPER_USER_PLUGINS from REAPER_RESOURCE_PATH: ${REAPER_USER_PLUGINS}")
  else()
    message(WARNING "REAPER_RESOURCE_PATH is set but path does not exist: ${REAPER_RESOURCE_PATH}")
  endif()
endif()

# Default fallback
if(NOT DEFINED REAPER_USER_PLUGINS)
  set(REAPER_USER_PLUGINS "UserPlugins")
  message(STATUS "Using default REAPER_USER_PLUGINS path: ${REAPER_USER_PLUGINS}")
endif()

# ------------------------------------------------------------------------------
# Installer Defaults and CPack Configuration
# ------------------------------------------------------------------------------

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(USER_CONFIG_DIR "$ENV{APPDATA}")
  elseif(APPLE)
    set(USER_CONFIG_DIR "~/Library/Application Support")
  else()
    set(USER_CONFIG_DIR "~/.config")
  endif()

  set(CMAKE_INSTALL_PREFIX "${USER_CONFIG_DIR}/REAPER" CACHE PATH "REAPER resource path where to install ReaPack" FORCE)
endif()

# Determine architecture string for output name
if(CMAKE_OSX_ARCHITECTURES)
  list(JOIN CMAKE_OSX_ARCHITECTURES "-" ARCH_NAME)
elseif(MSVC_CXX_ARCHITECTURE_ID)
  set(ARCH_NAME ${MSVC_CXX_ARCHITECTURE_ID})
else()
  set(ARCH_NAME ${CMAKE_SYSTEM_PROCESSOR})
endif()

string(TOLOWER "${ARCH_NAME}" ARCH_NAME)

# Set output name (no "lib" prefix) and architecture
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "reaper_csurf_integrator"
  # OUTPUT_NAME "reaper_${PROJECT_NAME}-${ARCH_NAME}"
)

# Install shared library
install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  RUNTIME DESTINATION "${REAPER_USER_PLUGINS}" # Windows .dll
  LIBRARY DESTINATION "${REAPER_USER_PLUGINS}" # Linux .so/macOS .dylib
)

# Install PDB files on Windows
if(WIN32)
  install(
    FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
    COMPONENT ${PROJECT_NAME}
    DESTINATION "${REAPER_USER_PLUGINS}"
  )
endif()

# Configure CPack
if(APPLE)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/Library/Application Support/REAPER")
  set(CPACK_COMPONENTS_ALL ${PROJECT_NAME})
  set(CPACK_GENERATOR productbuild)
elseif(WIN32)
  set(CPACK_GENERATOR NSIS)
else()
  set(CPACK_GENERATOR TXZ)
endif()

# Detect bitness
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(CPUBITS "64")
else()
  set(CPUBITS "32")
endif()

# Custom CPack naming
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-macOS")
endif()

# Add README and LICENSE to installer if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CMAKE_CURRENT_BINARY_DIR}/link_readme.txt")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_BINARY_DIR}/link_readme.txt")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/link_license.txt")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/link_license.txt")
endif()

set(CPACK_COMPONENT_${PROJECT_NAME}_REQUIRED ON)
set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_NSIS_CPUBITS ${CPUBITS})

include(CPack)
