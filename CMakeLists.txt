cmake_minimum_required(VERSION 3.8)
project(engine VERSION 1.0 LANGUAGES CXX)

add_library(engine SHARED
    include/engine.hpp
    src/engine.cpp
    src/gles_with_angle.h
    src/gles_with_angle.cpp
)

target_include_directories(engine
PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(engine
    PUBLIC
        -std=gnu++17
    PRIVATE
        -Wall -Wenginetra
    )
    target_link_libraries(engine
    PUBLIC
        stdc++
    )

elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang)
    if(WIN32)
        target_compile_options(engine
        PUBLIC
            /std:c++latest
        PRIVATE
            /W4
        )

    else()
        target_compile_options(engine
        PUBLIC
            -std=c++2a -stdlib=libc++
        PRIVATE
            -Wall -Wenginetra
        )

    endif()

elseif(MSVC)
    target_compile_options(engine
    PUBLIC
        /std:c++latest
    PRIVATE
        /W4 
    )
endif()

if(WIN32)
    set_target_properties(engine
    PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS True
        VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17134.0"
    )
    target_sources(engine
    PRIVATE
        src/engine_windows.cpp
    )
    target_link_libraries(engine
    PUBLIC
        kernel32 synchronization
    )

elseif(APPLE)
    target_sources(engine
    PRIVATE
        src/engine_unistd.cpp
    )
    target_link_libraries(engine
    PUBLIC
        c++
    )

else()
    target_sources(engine
    PRIVATE
        src/engine_unistd.cpp
    )
    target_link_libraries(engine
    PUBLIC
        c++ m
    )

endif()

# vcpkg
find_package(Threads   REQUIRED)
find_package(unofficial-angle   CONFIG REQUIRED)
find_package(coroutine CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(engine
PUBLIC
    Threads::Threads 
    glfw glm
    unofficial::angle::libEGL unofficial::angle::libGLESv2
    unofficial::angle::angle_common unofficial::angle::libANGLE
    coroutine_portable
)

# install(FILES       include/engine.hpp
#         DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
# )
install(TARGETS         engine
        EXPORT          ${PROJECT_NAME}-config
        RUNTIME         DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        INCLUDES        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        PUBLIC_HEADER   DESTINATION ${CMAKE_INSTALL_PREFIX}/include
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

find_package(Catch2 CONFIG REQUIRED)

add_executable(engine_test_suite
    test/test_engine.cpp
    test/test_gles_with_angle.cpp
    # test/test_gles_with_angle_dx11.cpp
)

target_link_libraries(engine_test_suite
PRIVATE
    engine Catch2::Catch2
)

if(MSVC)
    target_compile_options(engine_test_suite
    PRIVATE
        /wd4477
    )
else()
    target_compile_options(engine_test_suite
    PRIVATE
        -Wno-format
    )
endif()
