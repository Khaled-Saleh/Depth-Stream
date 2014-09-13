project (OpenNI_Server_Stream)
cmake_minimum_required(VERSION 2.8)

# CONFIGURE OPENCV
find_package (OpenCV REQUIRED)

# CONFIGURE OPENNI2
find_library(OPENNI2_LIBRARY
	NAMES OpenNI2
	PATHS "/home/khaled/Downloads/OpenNI2/OpenNI-Linux-x86-2.3/Redist"
)
find_path(OPENNI2_INCLUDE_DIR OpenNI.h
	PATHS "/home/khaled/Downloads/OpenNI2/OpenNI-Linux-x86-2.3/Include/"
)
#link_directories(${OPENNI2_LIBRARY})
include_directories(${OPENNI2_INCLUDE_DIR})

# CREATE EXECUTABLE
add_executable (OpenNI_Server_Stream OpenNI_Server_Stream.cpp)
target_link_libraries(OpenNI_Server_Stream ${OPENNI2_LIBRARY} ${OpenCV_LIBS} pthread m)
