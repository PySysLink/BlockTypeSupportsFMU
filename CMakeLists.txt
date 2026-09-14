cmake_minimum_required(VERSION 3.11)

set(PROJECT_VERSION "0.1.0")
set(PROJECT_VERSION_UNDERSCORE "0_1_0")
set(PYSYSLINK_BASE_VERSION "0.1.0")

# Project name and version
project(BlockTypeSupportsBasicFMUSupport VERSION ${PROJECT_VERSION})

set(INSTALLED_DYNAMIC_LIB_NAME "BlockTypeSupportsBasicFMUSupport-${PROJECT_VERSION}")

set(PLUGIN_INSTALL_DIR lib/pysyslink_plugins/block_type_supports/basic_fmu_support_${PROJECT_VERSION_UNDERSCORE})


# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# Check if the user has already set CMAKE_INSTALL_PREFIX
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # Configure default installation paths based on the platform
    if(WIN32)
        set(CMAKE_INSTALL_PREFIX "C:/Program Files" CACHE PATH "Install path" FORCE)
    elseif(APPLE)
        set(CMAKE_INSTALL_PREFIX "/Applications" CACHE PATH "Install path" FORCE)
    else() # Assume Linux or other Unix-like systems
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path" FORCE)
    endif()
endif()

# Print the chosen installation path
message(STATUS "Default install path: ${CMAKE_INSTALL_PREFIX}")

# Locate PySysLinkBase
find_package(PySysLinkBase ${PYSYSLINK_BASE_VERSION} REQUIRED)

# Add subdirectories
add_subdirectory(src)
# add_subdirectory(Tests)



message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# Define installation rules for the project
install(DIRECTORY src/                # Install headers
    DESTINATION include/BlockTypeSupports/BasicFMUSupport
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS BlockTypeSupportsBasicFMUSupport
    EXPORT BlockTypeSupportsBasicFMUSupportTargets # Associate this target with the export
    ARCHIVE DESTINATION ${PLUGIN_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    RUNTIME DESTINATION bin
)


configure_file(
    ${CMAKE_SOURCE_DIR}/basic_fmu_support.pslkbtsp.yaml.in
    ${CMAKE_BINARY_DIR}/basic_fmu_support.${PROJECT_VERSION_UNDERSCORE}.pslkbtsp.yaml
    @ONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/basic_blocks_fmu.pslkblp.yaml.in
    ${CMAKE_BINARY_DIR}/basic_blocks_fmu.${PROJECT_VERSION_UNDERSCORE}.pslkblp.yaml
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/basic_fmu_support.${PROJECT_VERSION_UNDERSCORE}.pslkbtsp.yaml
    DESTINATION ${PLUGIN_INSTALL_DIR}
)

install(FILES ${CMAKE_BINARY_DIR}/basic_blocks_fmu.${PROJECT_VERSION_UNDERSCORE}.pslkblp.yaml
    DESTINATION ${PLUGIN_INSTALL_DIR}
)

include(CMakePackageConfigHelpers)

# Generate the package configuration file for BlockTypeSupportsBasicFMUSupport
install(EXPORT BlockTypeSupportsBasicFMUSupportTargets
    FILE BlockTypeSupportsBasicFMUSupportTargets.cmake
    NAMESPACE BlockTypeSupports::BasicFMUSupport::
    DESTINATION lib/cmake/BlockTypeSupportsBasicFMUSupport
)

# Set the include directories and libraries for the package
set(PACKAGE_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
set(PACKAGE_LIBRARIES "${CMAKE_INSTALL_PREFIX}/${PLUGIN_INSTALL_DIR}")


write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BlockTypeSupportsBasicFMUSupportConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Create and install the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/BlockTypeSupportsBasicFMUSupportConfig.cmake.in
    ${CMAKE_BINARY_DIR}/BlockTypeSupportsBasicFMUSupportConfig.cmake
    INSTALL_DESTINATION lib/cmake/BlockTypeSupportsBasicFMUSupport
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/BlockTypeSupportsBasicFMUSupportConfig.cmake
    FILES ${CMAKE_CURRENT_BINARY_DIR}/BlockTypeSupportsBasicFMUSupportConfigVersion.cmake
    DESTINATION lib/cmake/BlockTypeSupportsBasicFMUSupport
)


# Ensure the libraries are installed correctly
install(TARGETS BlockTypeSupportsBasicFMUSupport
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)


if(UNIX AND NOT APPLE) # Linux-specific
    install(CODE "execute_process(COMMAND ldconfig)")
endif()

