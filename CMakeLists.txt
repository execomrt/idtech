cmake_minimum_required(VERSION 3.20)

project(idtech LANGUAGES CXX)

add_library(idtech
    src/bsp_explorer.cpp
    src/bsp_explorer.hpp
    src/bsp_loader.cpp
    src/bsp_loader.hpp
    src/bsp_quake1.cpp
    src/bsp_quake1.hpp
    src/bsp_renderer.cpp
    src/bsp_renderer.hpp
    src/bsp_types.hpp
    src/lightmap_packer.hpp
    src/math_types.hpp
    src/wad.cpp
    src/wad.hpp
)

add_library(idtech::idtech ALIAS idtech)

target_compile_features(idtech PUBLIC cxx_std_20)

target_include_directories(idtech
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(idtech PROPERTIES
    CXX_EXTENSIONS OFF
)

option(IDTECH_BUILD_LOADER_TEST "Build the BSP loader test application" ON)

if(IDTECH_BUILD_LOADER_TEST)
    add_executable(idtech_loader_test
        tests/loader_test.cpp
    )

    target_link_libraries(idtech_loader_test PRIVATE idtech::idtech)
    target_compile_features(idtech_loader_test PRIVATE cxx_std_20)
    target_compile_definitions(idtech_loader_test PRIVATE
        IDTECH_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    add_executable(idtech_wad_test
        tests/wad_test.cpp
    )

    target_link_libraries(idtech_wad_test PRIVATE idtech::idtech)
    target_compile_features(idtech_wad_test PRIVATE cxx_std_20)
    target_compile_definitions(idtech_wad_test PRIVATE
        IDTECH_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    add_executable(idtech_lightmap_packer_test
        tests/lightmap_packer_test.cpp
    )

    target_link_libraries(idtech_lightmap_packer_test PRIVATE idtech::idtech)
    target_compile_features(idtech_lightmap_packer_test PRIVATE cxx_std_20)

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT idtech_loader_test)
endif()
