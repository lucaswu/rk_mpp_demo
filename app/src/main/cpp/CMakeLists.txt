# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("rk_mpp_test")

set(3RD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rd/)
include_directories( ${3RD_INCLUDE_DIR}/mpp/inc)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/../libs)

include_directories(${CMAKE_SOURCE_DIR})

message("1111=${3RD_INCLUDE_DIR}/mpp/inc}")

##MPP
add_library( mpp SHARED IMPORTED )
set_target_properties(mpp
        PROPERTIES IMPORTED_LOCATION
        ${LIB_DIR}/mpp/${ANDROID_ABI}/libmpp.so)

add_library( vpu SHARED IMPORTED )
set_target_properties(vpu
        PROPERTIES IMPORTED_LOCATION
        ${LIB_DIR}/mpp/${ANDROID_ABI}/libvpu.so)

add_library(${CMAKE_PROJECT_NAME} SHARED

        native-lib.cpp
        mpp/MppDecoder264.cpp
        utils/ZYLog.cpp
)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        mpp
        vpu)