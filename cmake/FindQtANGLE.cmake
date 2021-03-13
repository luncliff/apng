# 
# Authors
#   - github.com/luncliff (luncliff@gmail.com)
#
# References
#   - https://cmake.org/cmake/help/latest/manual/cmake-qt.7.html
#   - https://wiki.qt.io/Qt_5_on_Windows_ANGLE_and_OpenGL
# 
# Tested 'Qt5_DIR's
#   - Anaconda3 Qt 5.9 - "C:\\Users\\luncl\\Anaconda3\\Library\\lib\\cmake\\Qt5"
#   - Qt 5.12.9 - "C:\\Qt\\Qt5.12.9\\5.12.9\\msvc2017_64"
#
cmake_minimum_required(VERSION 3.18)

find_package(Qt5 REQUIRED COMPONENTS OpenGL)
message(STATUS "Found Qt5::OpenGL ${Qt5_VERSION}")
foreach(dirpath ${Qt5OpenGL_INCLUDE_DIRS})
    message(STATUS " - ${dirpath}")
endforeach()
foreach(libpath ${Qt5OpenGL_LIBRARIES})
    if(TARGET ${libpath})
        get_target_property(Qt5OpenGL_LIB_LOCATION ${libpath} IMPORTED_IMPLIB_RELEASE)
        if(WIN32)
            get_target_property(Qt5OpenGL_BIN_LOCATION ${libpath} IMPORTED_LOCATION_RELEASE)
            message(STATUS " - ${Qt5OpenGL_LIB_LOCATION} -> ${Qt5OpenGL_BIN_LOCATION}")
        else()
            message(STATUS " - ${Qt5OpenGL_LIB_LOCATION}")
        endif()
    else()
        message(STATUS " - ${libpath}")
    endif()
endforeach()
get_target_property(QtANGLE_COMPILE_DEFINITIONS Qt5::OpenGL INTERFACE_COMPILE_DEFINITIONS)
message(STATUS " - ${QtANGLE_COMPILE_DEFINITIONS}")

if(NOT DEFINED QtANGLE_ROOT_DIR)
    # QtANGLE_CMAKE_DIR <-- msvc2017_64/lib/cmake/
    get_filename_component(QtANGLE_CMAKE_DIR   ${Qt5_DIR} DIRECTORY)
    # QtANGLE_LIBRARY_DIR <-- msvc2017_64/lib/
    get_filename_component(QtANGLE_LIBRARY_DIR ${QtANGLE_CMAKE_DIR} DIRECTORY)
    # QtANGLE_ROOT_DIR <-- msvc2017_64/
    get_filename_component(QtANGLE_ROOT_DIR    ${QtANGLE_LIBRARY_DIR} DIRECTORY)
endif()

find_path(QtANGLE_INCLUDE_DIR NAMES QtANGLE PATHS ${Qt5OpenGL_INCLUDE_DIRS} NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
if(QtANGLE_INCLUDE_DIR_FOUND)
    # If we found QtANGLE folder, use it without any concern.
    get_filename_component(QtANGLE_INCLUDE_DIR ${QtANGLE_INCLUDE_DIR}/QtANGLE ABSOLUTE)
elseif(Qt5_DIR MATCHES Anaconda3)
    # Anaconda Qt 5.9 has a different include dir
    # EGL 1.4 + OpenGL ES 3.0
    get_filename_component(QtANGLE_INCLUDE_DIR ${QtANGLE_ROOT_DIR}/include/qt/QtANGLE ABSOLUTE)
else()
    # Qt 5.12+
    # EGL 1.4 + OpenGL ES 3.1
    get_filename_component(QtANGLE_INCLUDE_DIR ${QtANGLE_ROOT_DIR}/include/QtANGLE ABSOLUTE)
endif()

find_library(QtANGLE_EGL_LIBRARY NAME libEGL PATHS ${QtANGLE_LIBRARY_DIR} REQUIRED NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(QtANGLE_GLES_LIBRARY NAME libGLESv2 PATHS ${QtANGLE_LIBRARY_DIR} REQUIRED NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
list(APPEND QtANGLE_LIBRARIES ${QtANGLE_EGL_LIBRARY} ${QtANGLE_GLES_LIBRARY})
if(WIN32)
    get_filename_component(QtANGLE_RUNTIME_DIR ${QtANGLE_LIBRARY_DIR}/../bin ABSOLUTE)
    get_filename_component(QtANGLE_EGL_LOCATION ${QtANGLE_RUNTIME_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}libEGL${CMAKE_SHARED_LIBRARY_SUFFIX} ABSOLUTE)
    get_filename_component(QtANGLE_EGL_PDB_LOCATION ${QtANGLE_RUNTIME_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}libEGL.pdb ABSOLUTE)
    if(NOT EXISTS ${QtANGLE_EGL_PDB_LOCATION})
        unset(QtANGLE_EGL_PDB_LOCATION)
    endif()
    get_filename_component(QtANGLE_GLES_LOCATION ${QtANGLE_RUNTIME_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}libGLESv2${CMAKE_SHARED_LIBRARY_SUFFIX} ABSOLUTE)
    get_filename_component(QtANGLE_GLES_PDB_LOCATION ${QtANGLE_RUNTIME_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}libGLESv2.pdb ABSOLUTE)
    if(NOT EXISTS ${QtANGLE_GLES_PDB_LOCATION})
        unset(QtANGLE_GLES_PDB_LOCATION)
    endif()
endif()

message(STATUS "Found QtANGLE ${Qt5_VERSION}")
message(STATUS " - ${QtANGLE_INCLUDE_DIR}")
message(STATUS " - ${QtANGLE_LIBRARY_DIR}")
message(STATUS "   - ${QtANGLE_EGL_LIBRARY}")
message(STATUS "   - ${QtANGLE_GLES_LIBRARY}")
if(WIN32)
message(STATUS " - ${QtANGLE_RUNTIME_DIR}")
message(STATUS "   - ${QtANGLE_EGL_LOCATION}")
if(DEFINED QtANGLE_EGL_PDB_LOCATION)
message(STATUS "   - ${QtANGLE_EGL_PDB_LOCATION}")
endif()
message(STATUS "   - ${QtANGLE_GLES_LOCATION}")
if(DEFINED QtANGLE_GLES_PDB_LOCATION)
message(STATUS "   - ${QtANGLE_GLES_PDB_LOCATION}")
endif()
endif()

if(WIN32)
    # libEGL.dll
    add_library(libEGL SHARED IMPORTED GLOBAL)
    add_library(Qt5::libEGL ALIAS libEGL)
    set_target_properties(libEGL
    PROPERTIES
        IMPORTED_IMPLIB "${QtANGLE_EGL_LIBRARY}"
        IMPORTED_LOCATION "${QtANGLE_EGL_LOCATION}"
    )
    target_include_directories(libEGL
    INTERFACE
        ${QtANGLE_INCLUDE_DIR}
    )
    # libGLESv2.dll
    add_library(libGLESv2 SHARED IMPORTED GLOBAL)
    add_library(Qt5::libGLESv2 ALIAS libGLESv2)
    set_target_properties(libGLESv2
    PROPERTIES
        IMPORTED_IMPLIB "${QtANGLE_GLES_LIBRARY}"
        IMPORTED_LOCATION "${QtANGLE_GLES_LOCATION}"
    )
    target_include_directories(libGLESv2
    INTERFACE
        ${QtANGLE_INCLUDE_DIR}
    )
    target_compile_definitions(libGLESv2
    INTERFACE
        ${QtANGLE_COMPILE_DEFINITIONS}
    )
endif()

set(QtANGLE_FOUND TRUE)