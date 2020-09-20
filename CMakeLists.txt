cmake_minimum_required(VERSION 3.13)
project(graphics VERSION 1.0 LANGUAGES CXX)

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg.cmake)
    message(FATAL_ERROR "requires vcpkg.cmake for CMAKE_TOOLCHAIN_FILE")
endif()
message(STATUS "using vcpkg: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
# vcpkg install --triplet x64-windows angle directxtk

set(CMAKE_VS_WINRT_BY_DEFAULT true)

add_library(graphics SHARED
    src/main.cpp
    src/vulkan.cpp
    src/vulkan_1.h
    src/vulkan_1.cpp
    src/opengl_1.h
    src/opengl.cpp
    src/opengl_es.cpp
)

set_target_properties(graphics
PROPERTIES
    CXX_STANDARD 17
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17134.0"
    WINDOWS_EXPORT_ALL_SYMBOLS  true
)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(graphics
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include>
PRIVATE
    ${TINYGLTF_INCLUDE_DIRS}
    externals/include
)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_link_directories(graphics
    PUBLIC
        $<BUILD_INTERFACE:${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib>
    )
else()
    target_link_directories(graphics
    PUBLIC
        $<BUILD_INTERFACE:${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib>
    )
endif()

find_package(Threads    REQUIRED)
find_package(Vulkan     REQUIRED)
find_package(spdlog     CONFIG REQUIRED)
find_package(glm        CONFIG REQUIRED)
target_link_libraries(graphics
PUBLIC
    Threads::Threads 
    glm Vulkan::Vulkan spdlog::spdlog
)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
    if(WIN32)
        target_compile_options(graphics
        PUBLIC
            /std:c++17
        PRIVATE
            /W4
        )

    else()
        target_compile_options(graphics
        PUBLIC
            -std=c++20 -stdlib=libc++
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
        /std:c++17 /Zc:__cplusplus
    PRIVATE
        /W4 /bigobj /await /permissive-
    )
endif()

if(WIN32)
    target_compile_definitions(graphics
    PUBLIC
        WIN32_LEAN_AND_MEAN NOMINMAX
    )
    target_link_libraries(graphics
    PUBLIC
        windowsapp windowscodecs DirectXTK
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

if(DEFINED Qt5_DIR)
    # Anaconda Qt 5.9 - "C:\\Users\\luncl\\Anaconda3\\Library\\lib\\cmake\\Qt5"
    # Qt 5.12 - "C:\\Qt\\Qt5.12.9\\5.12.9\\msvc2017_64"
    find_package(Qt5 REQUIRED
    COMPONENTS
        Core Widgets OpenGL
    )
    get_filename_component(angle_path           ${Qt5_DIR} DIRECTORY ) # msvc2017_64/lib/cmake/
    get_filename_component(angle_lib_dir        ${angle_path} DIRECTORY ) # msvc2017_64/lib/
    get_filename_component(angle_path           ${angle_lib_dir} DIRECTORY ) # msvc2017_64/
    if(Qt5_DIR MATCHES Anaconda3)
        # Anaconda Qt 5.9 has a different include dir
        # EGL 1.4 + OpenGL ES 3.0
        get_filename_component(angle_include_dir    ${angle_path}/include/qt/QtANGLE ABSOLUTE)
    else()
        # Qt 5.12
        # EGL 1.4 + OpenGL ES 3.1
        get_filename_component(angle_include_dir    ${angle_path}/include/QtANGLE ABSOLUTE)
    endif()
    unset(angle_path)
    message(STATUS "using QtANGLE:")
    message(STATUS " - ${angle_include_dir}")
    message(STATUS " - ${angle_lib_dir}")

    target_include_directories(graphics
    PUBLIC
        ${angle_include_dir}
    )
    target_link_directories(graphics
    PUBLIC
        ${angle_lib_dir}
    )
    target_link_libraries(graphics
    PUBLIC
        libEGL libGLESv2
    )
else()
    message(STATUS "using unofficial-angle")
    find_package(unofficial-angle CONFIG REQUIRED)
    target_link_libraries(graphics
    PUBLIC
        unofficial::angle::libEGL unofficial::angle::libGLESv2 
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
    COMMAND     ${glslc_path} bypass.frag -o sample_frag.spv
    COMMAND     ${glslc_path} bypass.frag         -o bypass_frag.spv
    COMMAND     ${glslc_path} sample_uniform.vert -o sample_uniform_vert.spv
)

enable_testing()
find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(graphics_test_suite
    test/test_main.cpp
    # test/test_vulkan_device.cpp
    # test/test_vulkan_surface_glfw.cpp
    # test/test_vulkan_pipeline.cpp
    # test/test_vulkan_descriptor_set.cpp
    test/test_opengl_es_windows.cpp
    test/test_opengl_es_glfw.cpp
    test/test_directx.cpp
)
add_dependencies(graphics_test_suite
    compile_shaders_glsl
)

set_target_properties(graphics_test_suite
PROPERTIES
    CXX_STANDARD 17
)

target_include_directories(graphics_test_suite
PRIVATE
    externals/include
)

target_link_libraries(graphics_test_suite
PUBLIC
    graphics glfw Catch2::Catch2
)

if(MSVC)
    target_compile_options(graphics_test_suite
    PRIVATE
        /wd4477 /utf-8
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

# add_test(NAME test_vulkan COMMAND graphics_test_suite "[vulkan]")
