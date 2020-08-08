cmake_minimum_required(VERSION 3.8)
project(graphics VERSION 1.0 LANGUAGES CXX)

if(NOT ${CMAKE_TOOLCHAIN_FILE} MATCHES vcpkg.cmake)
    message(FATAL_ERROR "requires vcpkg.cmake for CMAKE_TOOLCHAIN_FILE")
endif()
message(STATUS "using vcpkg: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

set(CMAKE_VS_WINRT_BY_DEFAULT true)

add_library(graphics SHARED
    src/main.cpp
    src/vulkan.cpp
    src/vulkan_1.h
    src/vulkan_1.cpp
)

set_target_properties(graphics
PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17134.0"
    WINDOWS_EXPORT_ALL_SYMBOLS  true
)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(graphics
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
PRIVATE
    ${TINYGLTF_INCLUDE_DIRS}
    externals/include
)

find_package(Threads    REQUIRED)
find_package(Vulkan     REQUIRED)
find_package(spdlog     CONFIG REQUIRED)
find_package(glm        CONFIG REQUIRED)
target_link_libraries(graphics
PUBLIC
    Threads::Threads 
    Vulkan::Vulkan glm
    spdlog::spdlog
)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
    if(WIN32)
        target_compile_options(graphics
        PUBLIC
            /std:c++latest
        PRIVATE
            /W4
        )

    else()
        target_compile_options(graphics
        PUBLIC
            -std=c++2a -stdlib=libc++
        PRIVATE
            -Wall -Wextra
        )

    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(graphics
    PUBLIC
        -std=gnu++20
    PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(graphics
    PUBLIC
        stdc++
    )

elseif(MSVC)
    target_compile_options(graphics
    PUBLIC
        /std:c++latest
    PRIVATE
        /W4 
    )
endif()

if(WIN32)
    target_compile_definitions(graphics
    PUBLIC
        WIN32_LEAN_AND_MEAN NOMINMAX
    )

elseif(CMAKE_SYSTEM_NAME MATCHES Linux)
    target_link_libraries(graphics
    PUBLIC
        stdc++ m
    )

elseif(APPLE OR UNIX)
    target_link_libraries(graphics
    PUBLIC
        c++
    )

endif()

find_package(glslang CONFIG REQUIRED)
find_program(glslc_path
    NAMES   glslc.exe glslc
    PATHS   ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools
    # NO_DEFAULT_PATH 
)
file(RELATIVE_PATH short_path ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET} ${glslc_path})
message(STATUS "using glslc: ${glslc_path}")
add_custom_target(compile_shaders_glsl
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/assets
    COMMAND     ${glslc_path} --version
    COMMAND     ${glslc_path} sample.vert -o sample_vert.spv
    COMMAND     ${glslc_path} sample.frag -o sample_frag.spv
)

enable_testing()
find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(graphics_test_suite
    test/test_main.cpp
    test/test_vulkan_device.cpp
    test/test_vulkan_surface_glfw.cpp
)
add_dependencies(graphics_test_suite
    compile_shaders_glsl
)

set_target_properties(graphics_test_suite
PROPERTIES
    CXX_STANDARD 20
)

target_include_directories(graphics_test_suite
PRIVATE
    externals/include
)

target_link_libraries(graphics_test_suite
PRIVATE
    graphics glfw Catch2::Catch2
)

if(MSVC)
    target_compile_options(graphics_test_suite
    PRIVATE
        /wd4477
    )
else()
    target_compile_options(graphics_test_suite
    PRIVATE
        -Wno-format
    )
endif()

target_compile_definitions(graphics_test_suite
PRIVATE
    ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
)

add_test(NAME test_glfw 
         COMMAND graphics_test_suite "[glfw]")
add_test(NAME test_vulkan_extensions
         COMMAND graphics_test_suite "[extension]")

# add_test(NAME test_vulkan COMMAND graphics_test_suite "[vulkan]")
