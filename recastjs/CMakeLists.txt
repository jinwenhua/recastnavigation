cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

FIND_PACKAGE(Python3)
set(PYTHON ${Python3_EXECUTABLE} CACHE STRING "Python path")
set(EMSCRIPTEN_ROOT $ENV{EMSDK}/upstream/emscripten CACHE STRING "Emscripten path")
set(CMAKE_TOOLCHAIN_FILE ${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake)
set(WEBIDL_BINDER_SCRIPT ${EMSCRIPTEN_ROOT}/tools/webidl_binder.py)
set(RECAST_IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/recast.idl)
set(RECASTAPI_HEADER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/recastjs.h)
set(RECAST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(RECAST_HEADER_FILE ${RECAST_ROOT}/Recast/Include/Recast.h)
set(NAVMESHQUERY_HEADER_FILE ${RECAST_ROOT}/Detour/Include/DetourNavMeshQuery.h)
set(RECAST_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(RECAST_ONLOAD_FILE ${CMAKE_CURRENT_SOURCE_DIR}/onload.js)
set(RECAST_EXPORT_POST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/export-post.js)

project(recastjs)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/ChunkyTriMesh.cpp
)

file(GLOB RECASTDETOUR_FILES
    ${RECAST_ROOT}/Detour/Source/DetourAlloc.cpp
    ${RECAST_ROOT}/Detour/Source/DetourAssert.cpp
    ${RECAST_ROOT}/Detour/Source/DetourCommon.cpp
    ${RECAST_ROOT}/Detour/Source/DetourNavMesh.cpp
    ${RECAST_ROOT}/Detour/Source/DetourNavMeshBuilder.cpp
    ${RECAST_ROOT}/Detour/Source/DetourNavMeshQuery.cpp
    ${RECAST_ROOT}/Detour/Source/DetourNode.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourCrowd.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourLocalBoundary.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourObstacleAvoidance.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourPathCorridor.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourPathQueue.cpp
    ${RECAST_ROOT}/DetourCrowd/Source/DetourProximityGrid.cpp
    ${RECAST_ROOT}/DetourTileCache/Source/DetourTileCache.cpp
    ${RECAST_ROOT}/DetourTileCache/Source/DetourTileCacheBuilder.cpp
    ${RECAST_ROOT}/Recast/Source/Recast.cpp
    ${RECAST_ROOT}/Recast/Source/RecastAlloc.cpp
    ${RECAST_ROOT}/Recast/Source/RecastArea.cpp
    ${RECAST_ROOT}/Recast/Source/RecastAssert.cpp
    ${RECAST_ROOT}/Recast/Source/RecastContour.cpp
    ${RECAST_ROOT}/Recast/Source/RecastFilter.cpp
    ${RECAST_ROOT}/Recast/Source/RecastLayers.cpp
    ${RECAST_ROOT}/Recast/Source/RecastMesh.cpp
    ${RECAST_ROOT}/Recast/Source/RecastMeshDetail.cpp
    ${RECAST_ROOT}/Recast/Source/RecastRasterization.cpp
    ${RECAST_ROOT}/Recast/Source/RecastRegion.cpp
    ${RECAST_ROOT}/RecastDemo/Contrib/fastlz/fastlz.c
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib
    ${RECAST_ROOT}/Detour/Include
    ${RECAST_ROOT}/DetourCrowd/Include
    ${RECAST_ROOT}/DetourTileCache/Include
    ${RECAST_ROOT}/Recast/Include
    ${RECAST_ROOT}/RecastDemo/Contrib/fastlz
)

SET(EXE_NAME "recast")

ADD_LIBRARY(${EXE_NAME} ${SRC_FILES} ${RECASTDETOUR_FILES})

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type")
set(ALLOW_MEMORY_GROWTH 1 CACHE STRING "Allow Memory Growth")

set(EMCC_ARGS
    -flto
    --post-js glue.js
    --post-js ${RECAST_ONLOAD_FILE}
    --extern-post-js ${RECAST_EXPORT_POST_FILE}
    -O3
    -s ALLOW_MEMORY_GROWTH=${ALLOW_MEMORY_GROWTH}
    -s ALLOW_TABLE_GROWTH=1
    -s EXPORTED_FUNCTIONS=["_malloc","_free"]
    -s EXPORTED_RUNTIME_METHODS=["UTF8ToString"]
    -s EXPORT_NAME="Recast"
    -s MODULARIZE=1
    -s NO_EXIT_RUNTIME=1
    -s NO_FILESYSTEM=1
)

set(EMCC_JS_ARGS ${EMCC_ARGS}
    -s AGGRESSIVE_VARIABLE_ELIMINATION=1
    -s ELIMINATE_DUPLICATE_FUNCTIONS=1
    -s SINGLE_FILE=1
    -s FILESYSTEM=0
    -s ENVIRONMENT=web
    -s WASM=1)

set(EMCC_GLUE_ARGS
    -c
    -I${RECAST_SRC_DIR}
    -I${CMAKE_CURRENT_SOURCE_DIR}/contrib
    -I${RECAST_ROOT}/Detour/Include
    -I${RECAST_ROOT}/DetourCrowd/Include
    -I${RECAST_ROOT}/DetourTileCache/Include
    -I${RECAST_ROOT}/Recast/Include
    -I${RECAST_ROOT}/RecastDemo/Contrib/fastlz
    -include${RECASTAPI_HEADER_FILE}
    -include${RECAST_HEADER_FILE}
    -include${NAVMESHQUERY_HEADER_FILE}
)

add_custom_command(
    OUTPUT glue.cpp glue.js
    BYPRODUCTS parser.out WebIDLGrammar.pkl
    COMMAND ${PYTHON} ${WEBIDL_BINDER_SCRIPT} ${RECAST_IDL_FILE} glue
    DEPENDS ${RECAST_IDL_FILE}
    COMMENT "Generating ${EXE_NAME} bindings"
    VERBATIM)
add_custom_command(
    OUTPUT glue.o
    COMMAND emcc glue.cpp ${EMCC_GLUE_ARGS} -o glue.o
    DEPENDS glue.cpp ${RECASTAPI_HEADER_FILE} ${RECAST_HEADER_FILE} ${NAVMESHQUERY_HEADER_FILE}
    COMMENT "Building ${EXE_NAME} bindings"
    VERBATIM)
add_custom_target(${EXE_NAME}-bindings ALL DEPENDS glue.js glue.o)

add_custom_command(
    OUTPUT ${EXE_NAME}.js
    COMMAND emcc glue.o lib${EXE_NAME}.a ${EMCC_JS_ARGS} -o ${EXE_NAME}.js
    DEPENDS ${EXE_NAME}-bindings ${EXE_NAME} ${RECAST_ONLOAD_FILE} ${RECAST_EXPORT_POST_FILE}
    COMMENT "Building ${EXE_NAME} javascript"
    VERBATIM)
add_custom_target(${EXE_NAME}-javascript ALL DEPENDS ${EXE_NAME}.js)
