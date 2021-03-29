cmake_minimum_required(VERSION 3.13)
project(graphics VERSION 1.0 LANGUAGES CXX)

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg.cmake)
    message(FATAL_ERROR "Requires vcpkg.cmake for CMAKE_TOOLCHAIN_FILE")
endif()
message(STATUS "Using vcpkg: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

set(CMAKE_VS_WINRT_BY_DEFAULT true)
# see https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.17763.0")
endif()

add_library(graphics
    include/graphics.h
    src/main.cpp
    src/context.cpp
    src/programs.cpp
    src/pbo.cpp
    # src/opengl_1.h
    # src/opengl.cpp
    # src/opengl_es.cpp
)

set_target_properties(graphics
PROPERTIES
    CXX_STANDARD 17
    PUBLIC_HEADER include/graphics.h
)

find_package(spdlog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED) # find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_package(directxtk CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)

target_include_directories(graphics
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
PRIVATE
    src
    externals/include
)

# if(CMAKE_BUILD_TYPE MATCHES Debug)
#     target_link_directories(graphics
#     PUBLIC
#         $<BUILD_INTERFACE:${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib>
#     )
# else()
#     target_link_directories(graphics
#     PUBLIC
#         $<BUILD_INTERFACE:${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib>
#     )
# endif()

target_link_libraries(graphics
PUBLIC
    Microsoft.GSL::GSL spdlog::spdlog
    Microsoft::DirectXTK Microsoft::DirectXTex
    windowsapp windowscodecs
)

target_link_options(graphics
PRIVATE
    /ERRORREPORT:SEND
    # /APPCONTAINER # for UWP
)

target_compile_options(graphics
PUBLIC
    /std:c++17 /Zc:__cplusplus
PRIVATE
    /W4 /bigobj /await /errorReport:send
)

target_compile_definitions(graphics
PUBLIC
    WIN32_LEAN_AND_MEAN NOMINMAX
    GL_GLEXT_PROTOTYPES EGL_EGLEXT_PROTOTYPES
)
if(BUILD_SHARED_LIBS) # control dllexport/import
    target_compile_definitions(graphics
    PRIVATE
        _WINDLL
    )
else()
    target_compile_definitions(graphics
    PRIVATE
        FORCE_STATIC_LINK
    )
endif()

if(DEFINED ENV{Qt5_DIR} OR DEFINED Qt5_DIR)
    list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
    find_package(QtANGLE REQUIRED)
    message(STATUS "Using QtANGLE: ${QtANGLE_RUNTIME_DIR}")
    target_link_libraries(graphics
    PUBLIC
        Qt5::libEGL Qt5::libGLESv2
    )
    file(COPY ${QtANGLE_EGL_LOCATION} ${QtANGLE_EGL_PDB_LOCATION} ${QtANGLE_GLES_LOCATION} ${QtANGLE_GLES_PDB_LOCATION} DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
    file(COPY ${QtANGLE_EGL_LOCATION} ${QtANGLE_EGL_PDB_LOCATION} ${QtANGLE_GLES_LOCATION} ${QtANGLE_GLES_PDB_LOCATION} DESTINATION ${CMAKE_BINARY_DIR})
    message(STATUS "Copied QtANGLE: ${CMAKE_BINARY_DIR}, ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

else()
    message(STATUS "Using ANGLE: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    find_package(unofficial-angle CONFIG REQUIRED)
    target_link_libraries(graphics
    PUBLIC
        unofficial::angle::libEGL unofficial::angle::libGLESv2 
    )

endif()

find_package(Vulkan)
if(Vulkan_FOUND)
    target_link_libraries(graphics
    PUBLIC
        Vulkan::Vulkan
    )
    # find_package(glslang CONFIG REQUIRED)
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
endif()

install(TARGETS  graphics
        EXPORT   ${PROJECT_NAME}-config
        RUNTIME  DESTINATION bin
        LIBRARY  DESTINATION lib
        ARCHIVE  DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)
install(EXPORT   ${PROJECT_NAME}-config
        DESTINATION share/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
set(VERSION_FILE_PATH   ${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}-config-version.cmake)
write_basic_package_version_file(${VERSION_FILE_PATH}
    VERSION             ${PROJECT_VERSION}
    COMPATIBILITY       SameMajorVersion
)
install(FILES           ${VERSION_FILE_PATH} 
        DESTINATION     share/${PROJECT_NAME}
)
if(QtANGLE_FOUND)
    install(FILES   ${QtANGLE_EGL_LOCATION} ${QtANGLE_EGL_PDB_LOCATION} ${QtANGLE_GLES_LOCATION} ${QtANGLE_GLES_PDB_LOCATION} 
                    ${QtANGLE_EGL_LOCATION} ${QtANGLE_EGL_PDB_LOCATION} ${QtANGLE_GLES_LOCATION} ${QtANGLE_GLES_PDB_LOCATION}
            DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    )
endif()

enable_testing()
find_package(glfw3 3.3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(graphics_test_suite
    test/test_main.cpp
    test/test_gltf.cpp
    test/test_directx.cpp
    test/test_opengl_es.cpp
    # test/test_vulkan_device.cpp
    # test/test_vulkan_surface_glfw.cpp
    # test/test_vulkan_pipeline.cpp
    # test/test_vulkan_descriptor_set.cpp
)
if(QtANGLE_FOUND)
    target_sources(graphics_test_suite
    PRIVATE
        test/test_qt5.cpp
    )
endif()
# add_dependencies(graphics_test_suite compile_shaders_glsl)

set_target_properties(graphics_test_suite
PROPERTIES
    CXX_STANDARD 17
)

target_include_directories(graphics_test_suite
PRIVATE
    externals/include
)

target_link_libraries(graphics_test_suite
PRIVATE
    graphics glfw Catch2::Catch2 nlohmann_json::nlohmann_json
)

target_compile_options(graphics_test_suite
PRIVATE
    /wd4477 /utf-8
)

target_compile_definitions(graphics_test_suite
PRIVATE
    ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
    CATCH_CONFIG_BENCHMARK
    # CATCH_CONFIG_FAST_COMPILE
)

add_test(NAME test_egl COMMAND graphics_test_suite "[egl]")
add_test(NAME test_windows COMMAND graphics_test_suite "[windows]")
add_test(NAME test_directx COMMAND graphics_test_suite "[directx]")
if(Vulkan_FOUND)
    add_test(NAME test_vulkan COMMAND graphics_test_suite "[vulkan]")
endif()

install(TARGETS  graphics_test_suite
        RUNTIME  DESTINATION bin
)
