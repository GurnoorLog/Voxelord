cmake_minimum_required(VERSION 3.12)
project(VoxLord)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(VIEW_DISTANCE 32 CACHE STRING "View distance, in sections")
set(LOADING_WORKERS_COUNT 4 CACHE STRING "Number of worker threads used to load chunks")

file(GLOB_RECURSE sources_all src/*.cpp src/*.c src/*.h src/*.hpp)
file(GLOB_RECURSE external_sources external/*.cpp external/*.c external/*.h external/*.hpp)
file(GLOB_RECURSE data assets/*)

# Client target: everything except the dedicated server's main.
set(sources ${sources_all})
list(FILTER sources EXCLUDE REGEX ".*[/\\\\]main_server\\.cpp$")
# Server target: everything except the client's main.
set(server_sources ${sources_all})
list(FILTER server_sources EXCLUDE REGEX ".*[/\\\\]main\\.cpp$")

add_executable(voxlord ${sources} ${external_sources} ${data})
add_executable(voxlord-server ${server_sources} ${external_sources} ${data})

target_include_directories(voxlord PUBLIC src external)
target_include_directories(voxlord-server PUBLIC src external)
include_directories(src external)

foreach(target voxlord voxlord-server)
	target_compile_definitions(${target} PRIVATE
	                           CC_VIEW_DISTANCE=${VIEW_DISTANCE}
	                           CC_LOADING_WORKERS_COUNT=${LOADING_WORKERS_COUNT})
endforeach()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

find_package(SFML 2.6 COMPONENTS graphics network REQUIRED)
target_link_libraries(voxlord PRIVATE sfml-graphics sfml-network)
target_link_libraries(voxlord-server PRIVATE sfml-graphics sfml-network)

if(UNIX)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(voxlord PRIVATE Threads::Threads)
target_link_libraries(voxlord-server PRIVATE Threads::Threads)
endif()

find_package(OpenGL REQUIRED)

foreach(target voxlord voxlord-server)
	target_include_directories(${target} PUBLIC
	                           $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	                           $<INSTALL_INTERFACE:include>)
	target_link_libraries(${target} PUBLIC OpenGL::GL ${CMAKE_DL_LIBS})
endforeach()

if(MSVC)
  target_compile_options(voxlord PRIVATE /W3)
  target_compile_options(voxlord-server PRIVATE /W3)
else()
  target_compile_options(voxlord PRIVATE -Wall -Wextra)
  target_compile_options(voxlord-server PRIVATE -Wall -Wextra)
endif()