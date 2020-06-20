cmake_minimum_required(VERSION 3.8)
project(graphics VERSION 1.0 LANGUAGES CXX)

if(NOT ${CMAKE_TOOLCHAIN_FILE} MATCHES vcpkg.cmake)
    message(FATAL_ERROR "requires vcpkg.cmake for CMAKE_TOOLCHAIN_FILE")
endif()
message(STATUS "using vcpkg: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17134.0")

add_library(graphics
    src/main.cpp
    include/graphics.h
    src/vulkan.cpp
    src/vulkan_1.cpp
    src/vulkan_2.cpp
)

target_include_directories(graphics
PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
PRIVATE
    externals/include
)

target_link_libraries(graphics
PUBLIC
    ${CMAKE_DL_LIBS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
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

elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang)
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

elseif(MSVC)
    target_compile_options(graphics
    PUBLIC
        /std:c++latest
    PRIVATE
        /W4 
    )
endif()

if(WIN32)
    set_target_properties(graphics
    PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS True
    )
    target_link_libraries(graphics
    PUBLIC
        kernel32 synchronization
    )

elseif(APPLE)
    target_link_libraries(graphics
    PUBLIC
        c++
    )

else()
    target_link_libraries(graphics
    PUBLIC
        c++ m
    )

endif()

#
# https://github.com/microsoft/vcpkg 2020.05
#
find_package(Threads    REQUIRED)
find_package(coroutine CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan     REQUIRED)
# find_package(glslang CONFIG REQUIRED)
# find_package(spirv_cross_cpp CONFIG REQUIRED)
# find_package(spirv_cross_glsl CONFIG REQUIRED)
# find_package(spirv_cross_hlsl CONFIG REQUIRED)

target_link_libraries(graphics
PUBLIC
    coroutine_portable
    Threads::Threads 
    glm Vulkan::Vulkan
    # glslang::HLSL glslang::SPIRV # glslang::glslang glslang::OGLCompiler
    # spirv-cross-cpp spirv-cross-hlsl # spirv-cross-glsl
)

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

#
#   OpenGL ES
#
target_sources(graphics
PRIVATE
    src/opengl.h    src/opengl.cpp
    src/programs.h  src/programs.cpp
)
if(WIN32)
    target_sources(graphics
    PRIVATE
        src/opengl_es.h src/opengl_es.cpp
        src/opengl_windows.cpp
    )
    find_package(unofficial-angle   CONFIG REQUIRED)
    target_link_libraries(graphics
    PRIVATE
        unofficial::angle::libEGL
        unofficial::angle::libGLESv2
    )
    target_compile_definitions(graphics
    PUBLIC
        WIN32_LEAN_AND_MEAN NOMINMAX
    )

elseif(APPLE)
    target_sources(graphics
    PRIVATE
        src/opengl_darwin.cpp
    )
    target_link_libraries(graphics
    PUBLIC
        ${CMAKE_DL_LIBS} "-framework OpenGL"
    )
    target_compile_definitions(graphics
    PUBLIC
        GL_SILENCE_DEPRECATION
    )

endif()

install(TARGETS         graphics
        EXPORT          ${PROJECT_NAME}-config
        RUNTIME         DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        # INCLUDES        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        PUBLIC_HEADER   DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
)
install(EXPORT          ${PROJECT_NAME}-config
        DESTINATION     ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
set(VERSION_FILE_PATH   ${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}-config-version.cmake)
write_basic_package_version_file(${VERSION_FILE_PATH}
    VERSION             ${PROJECT_VERSION}
    COMPATIBILITY       SameMajorVersion
)
install(FILES           ${VERSION_FILE_PATH} 
        DESTINATION     ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}
)

if(NOT BUILD_TESTING)
    return()
endif()
enable_testing()
find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(graphics_test_suite
    test/test_main.cpp
    test/test_vulkan_glfw.cpp
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

if(WIN32)
    target_sources(graphics_test_suite
    PRIVATE
        test/test_opengl_es.cpp
        test/test_opengl_es_glfw.cpp
        test/test_opengl_es_windows.cpp
        test/test_opengl_glfw.cpp
    )
else()
    target_sources(graphics_test_suite
    PRIVATE
        test/test_opengl_glfw.cpp
    )
endif()

target_compile_definitions(graphics_test_suite
PRIVATE
    ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
    WASSET_DIR=L"${PROJECT_SOURCE_DIR}/assets"
)

add_test(NAME test_opengl COMMAND graphics_test_suite "[opengl]")
add_test(NAME test_vulkan COMMAND graphics_test_suite "[vulkan]")
