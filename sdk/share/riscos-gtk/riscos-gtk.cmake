# riscos-gtk devkit: CMake toolchain file. Source the devkit's env.sh, then
#   cmake -DCMAKE_TOOLCHAIN_FILE=$RISCOS_GTK_SDK/share/riscos-gtk/riscos-gtk.cmake ..
# and find GTK with pkg-config (pkg_check_modules(GTK REQUIRED gtk+-3.0)),
# linking ${GTK_STATIC_LIBRARIES}. MIT licence.
if(NOT DEFINED ENV{RISCOS_GTK_SDK})
  message(FATAL_ERROR "source the riscos-gtk devkit's env.sh first")
endif()
set(CMAKE_SYSTEM_NAME GNU)   # as GCCSDK's own toolchain file: makes UNIX true
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   $ENV{RISCOS_GTK_SDK}/bin/arm-riscos-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER $ENV{RISCOS_GTK_SDK}/bin/arm-riscos-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH $ENV{RISCOS_GTK_PREFIX} $ENV{RISCOS_TOOLCHAIN})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_EXE_LINKER_FLAGS_INIT -static)
set(BUILD_SHARED_LIBS OFF)
