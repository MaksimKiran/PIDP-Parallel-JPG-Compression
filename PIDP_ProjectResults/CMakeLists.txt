#cmake_minimum_required(VERSION 4.1)
#project(PIDP_ProjectResults)
#
#set(CMAKE_CXX_STANDARD 20)
#
#add_executable(PIDP_ProjectResults main.cpp)

#cmake_minimum_required(VERSION 3.20)
#project(mpi_test C)
#
#set(CMAKE_C_STANDARD 11)
#
## MS-MPI paths
#set(MPI_INCLUDE_PATH "C:/Program Files (x86)/Microsoft SDKs/MPI/Include")
#set(MPI_LIB_PATH "C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64")
#
#include_directories(${MPI_INCLUDE_PATH})
#link_directories(${MPI_LIB_PATH})
#
#add_executable(mpi_test main.cpp)
#target_link_libraries(mpi_test msmpi)



cmake_minimum_required(VERSION 3.20)
project(PIDP_ProjectResults LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- MS-MPI paths ----
set(MPI_INCLUDE_PATH "C:/Program Files (x86)/Microsoft SDKs/MPI/Include")
set(MPI_LIB_PATH     "C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64")

if(NOT EXISTS "${MPI_INCLUDE_PATH}/mpi.h")
    message(FATAL_ERROR "MPI header not found. Check MS-MPI installation.")
endif()

include_directories(${MPI_INCLUDE_PATH})
link_directories(${MPI_LIB_PATH})

# ---- libjpeg-turbo64 paths ----
set(JPEG_INCLUDE_DIR "C:/libjpeg-turbo64/include")
set(JPEG_LIB "C:/libjpeg-turbo64/lib/jpeg-static.lib")

if(NOT EXISTS "${JPEG_INCLUDE_DIR}/jpeglib.h")
    message(FATAL_ERROR "libjpeg-turbo64 header not found. Check JPEG_INCLUDE_DIR path.")
endif()

include_directories(${JPEG_INCLUDE_DIR})

# ---- Add executable ----
add_executable(PIDP_ProjectResults main.cpp)

# ---- Link libraries ----
target_link_libraries(PIDP_ProjectResults
        msmpi           # MS-MPI
        ${JPEG_LIB}     # libjpeg-turbo64
)


