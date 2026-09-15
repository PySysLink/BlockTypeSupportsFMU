cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

project(BlockTypeSupportsFMU LANGUAGES C CXX)

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

add_executable(fmu_test src/fmu_test.cpp)
target_compile_features(fmu_test PRIVATE cxx_std_17)
target_compile_definitions(fmu_test PRIVATE
    DEFAULT_FMU_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test/reference_fmus/BouncingBall.fmu"
)
target_link_libraries(fmu_test PRIVATE fmilib ${CMAKE_DL_LIBS})

if(NOT DEFINED FMU_TEST_FILE)
    set(FMU_TEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/reference_fmus/BouncingBall.fmu")
endif()

add_custom_target(run_fmu_test
    COMMAND fmu_test "${FMU_TEST_FILE}"
    DEPENDS fmu_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)