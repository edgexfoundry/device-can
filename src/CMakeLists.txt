cmake_minimum_required(VERSION 3.0)
project(device_can LANGUAGES C)

# Package support
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Versioning
file (STRINGS ${CMAKE_SOURCE_DIR}/../VERSION CAN_DOT_VERSION)

set (IOT_VER 1.5)

find_package (IOT REQUIRED)
set (LINK_LIBRARIES ${LINK_LIBRARIES} ${IOT_LIBRARIES})
set (INCLUDE_DIRS ${INCLUDE_DIRS} ${IOT_INCLUDE_DIRS})

find_package (LIBCSDK REQUIRED)
if (NOT LIBCSDK_FOUND)
  message (WARNING "csdk library or header not found")
endif ()

# Set additional C compiler flags
if (DEFINED CMAKE_C_COMPILER_ID)
  if (CMAKE_C_COMPILER_ID MATCHES GNU)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -rdynamic -Wall -Wstrict-prototypes -Wmissing-prototypes")
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb -O0")
    set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -ggdb -O0")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -O2")
   endif ()
endif ()

file (GLOB C_FILES c/*.c)
find_library(EDGEX_CSDK_RELEASE_LIB NAMES csdk)
#find_library(IOT_LIBRARIES NAMES iot)
find_library (IOT_LIBRARY iot PATHS /opt/iotech/iot/${IOT_VER}/lib)
find_library(EXECINFO_LIBRARY NAMES execinfo)
add_executable(device-can ${C_FILES})
target_compile_definitions(device-can PRIVATE VERSION="${CAN_DOT_VERSION}")
target_include_directories (device-can PRIVATE .)
target_include_directories (device-can PRIVATE ${IOT_INCLUDE_DIRS})
target_link_libraries (device-can PUBLIC m PRIVATE ${EDGEX_CSDK_RELEASE_LIB})
target_link_libraries (device-can PUBLIC m PRIVATE ${IOT_LIBRARY})
target_link_libraries (device-can PRIVATE ${EXECINFO_LIBRARY})
install(TARGETS device-can DESTINATION bin)
