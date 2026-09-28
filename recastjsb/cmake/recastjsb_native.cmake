# Include from a Cocos Creator native project, then call:
#   recastjsb_add_native(<engine-or-app-target>)
#
# Register in jsb_module_register.cpp (see jsb/jsb_module_register.inl).

set(RECASTJSB_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(RECASTJSB_ROOT ${RECASTJSB_CMAKE_DIR}/..)
set(RECASTJS_ROOT ${RECASTJSB_ROOT}/../recastjs)
set(RECAST_ROOT ${RECASTJSB_ROOT}/..)

set(RECASTJSB_NATIVE_SOURCES
    ${RECASTJS_ROOT}/src/recastjs.cpp
    ${RECASTJS_ROOT}/contrib/ChunkyTriMesh.cpp
    ${RECASTJSB_ROOT}/src/recastjsb_offmesh.cpp
    ${RECASTJSB_ROOT}/jsb/jsb_recast.cpp
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

set(RECASTJSB_NATIVE_INCLUDES
    ${RECASTJSB_ROOT}/jsb
    ${RECASTJSB_ROOT}/include
    ${RECASTJS_ROOT}/src
    ${RECASTJS_ROOT}/contrib
    ${RECAST_ROOT}/Detour/Include
    ${RECAST_ROOT}/DetourCrowd/Include
    ${RECAST_ROOT}/DetourTileCache/Include
    ${RECAST_ROOT}/Recast/Include
    ${RECAST_ROOT}/RecastDemo/Contrib/fastlz
)

function(recastjsb_add_native target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "recastjsb_add_native: target '${target}' does not exist")
    endif()
    target_sources(${target} PRIVATE ${RECASTJSB_NATIVE_SOURCES})
    target_include_directories(${target} PRIVATE ${RECASTJSB_NATIVE_INCLUDES})
endfunction()
