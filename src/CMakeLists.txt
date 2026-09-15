# Define the shared library
add_library(BlockTypeSupportsBasicFMUSupport SHARED 
            RegisterBlockFactories.cpp
            LoggerInstance.cpp
            )


set(CMAKE_POSITION_INDEPENDENT_CODE ON)


find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(yaml-cpp-static STATIC IMPORTED)

set_target_properties(yaml-cpp-static PROPERTIES
    IMPORTED_LOCATION "/usr/lib64/libyaml-cpp.a"
    INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
)

find_package(fmt REQUIRED)


find_package(Eigen3 REQUIRED NO_MODULE)

# -------------------------------------------------------------------
# FMI Library
# -------------------------------------------------------------------

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(
    fmi_library
    GIT_REPOSITORY https://github.com/modelon-community/fmi-library.git
    GIT_TAG 3.0.4
)
FetchContent_GetProperties(fmi_library)
if(NOT fmi_library_POPULATED)
    FetchContent_Populate(fmi_library)
endif()

set(FMILIB_INSTALL_DIR "${CMAKE_BINARY_DIR}/fmi-library-install")
file(MAKE_DIRECTORY "${FMILIB_INSTALL_DIR}/include")
ExternalProject_Add(fmi_library_build
    SOURCE_DIR "${fmi_library_SOURCE_DIR}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/fmi-library-build"
    INSTALL_DIR "${FMILIB_INSTALL_DIR}"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=lib
        -DFMILIB_BUILD_TESTS=OFF
        -DFMILIB_GENERATE_DOXYGEN_DOC=OFF
        -DFMILIB_BUILD_SHARED_LIB=OFF
        -DFMILIB_BUILD_STATIC_LIB=ON
    BUILD_BYPRODUCTS "${FMILIB_INSTALL_DIR}/lib/libfmilib.a"
)


add_library(fmilib STATIC IMPORTED GLOBAL)
set_target_properties(fmilib PROPERTIES
    IMPORTED_LOCATION "${FMILIB_INSTALL_DIR}/lib/libfmilib.a"
    INTERFACE_INCLUDE_DIRECTORIES "${FMILIB_INSTALL_DIR}/include"
)
add_dependencies(fmilib fmi_library_build)


# -------------------------------------------------------------------
# Filesystem
# -------------------------------------------------------------------

# only if GCC version is 8 or lower, we need to link with stdc++fs for filesystem support

IF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    EXECUTE_PROCESS(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION
    )
    if (GCC_VERSION VERSION_LESS 9)
        target_link_libraries(BlockTypeSupportsBasicFMUSupport PRIVATE stdc++fs)
    endif()
endif()

# Include directories for header files
target_include_directories(BlockTypeSupportsBasicFMUSupport PUBLIC ${PySysLinkBase_INCLUDE_DIRS})




target_link_libraries(BlockTypeSupportsBasicFMUSupport
    PRIVATE
        PySysLinkBase::PySysLinkBase
        yaml-cpp-static
        spdlog::spdlog_header_only
        fmt::fmt
        Eigen3::Eigen
        fmilib 
        ${CMAKE_DL_LIBS}
)

set_target_properties(BlockTypeSupportsBasicFMUSupport PROPERTIES 
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}${INSTALLED_DYNAMIC_LIB_NAME}.so"
    OUTPUT_NAME "${INSTALLED_DYNAMIC_LIB_NAME}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(BlockTypeSupportsBasicFMUSupport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  # Include path for the build tree
        $<INSTALL_INTERFACE:include>                   # Include path for the installed tree
)

target_link_options(BlockTypeSupportsBasicFMUSupport PRIVATE
    -static-libstdc++
    -static-libgcc
)
