#pragma once
#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "DetourNavMeshBuilder.h"
#include "DetourCrowd.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "fastlz.h"
#include <algorithm>
#include <vector>
#include <iostream>
#include <list>

class dtNavMeshQuery;
class dtNavMesh;
class MeshLoader;
class NavMesh;
struct rcPolyMesh;
class rcPolyMeshDetail;
struct rcConfig;
struct NavMeshintermediates;
struct TileCacheData;

struct Vec3 
{
    Vec3() {}
    Vec3(float v) : x(v), y(v), z(v) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    void isMinOf(const Vec3& v)
    {
        x = std::min(x, v.x);
        y = std::min(y, v.y);
        z = std::min(z, v.z);
    }
    void isMaxOf(const Vec3& v)
    {
        x = std::max(x, v.x);
        y = std::max(y, v.y);
        z = std::max(z, v.z);
    }
    float operator [](int index) {
        return ((float*)&x)[index];
    }
    float x, y, z;
};

struct Triangle 
{
    Triangle(){}
    const Vec3& getPoint(long n)
    {
        if (n < 2)
        {
            return mPoint[n];
        }
        return mPoint[2];
    }
    Vec3 mPoint[3];
};

struct DebugNavMesh 
{
    int getTriangleCount() { return int(mTriangles.size()); }
    const Triangle& getTriangle(int n)
    {
        if (n < int(mTriangles.size()))
        {
            return mTriangles[n];
        }
        return mTriangles.back();
    }
    std::vector<Triangle> mTriangles;
};

struct NavPath
{
    int getPointCount() { return int(mPoints.size()); }
    const Vec3& getPoint(int n)
    {
        if (n < int(mPoints.size()))
        {
            return mPoints[n];
        }
        return mPoints.back();
    }
    std::vector<Vec3> mPoints;
};

struct NavmeshData
{
    void* dataPointer;
    int size;
};

struct RecastFastLZCompressor : public dtTileCacheCompressor
{
    virtual int maxCompressedSize(const int bufferSize)
    {
        return (int)(bufferSize* 1.05f);
    }
    
    virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
                              unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
    {
        *compressedSize = fastlz_compress((const void *const)buffer, bufferSize, compressed);
        return DT_SUCCESS;
    }
    
    virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
                                unsigned char* buffer, const int maxBufferSize, int* bufferSize)
    {
        *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
        return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
    }
};

struct RecastLinearAllocator : public dtTileCacheAlloc
{
    unsigned char* buffer;
    size_t capacity;
    size_t top;
    size_t high;
    
    RecastLinearAllocator(const size_t cap) : buffer(0), capacity(0), top(0), high(0)
    {
        resize(cap);
    }
    
    ~RecastLinearAllocator()
    {
        dtFree(buffer);
    }

    void resize(const size_t cap)
    {
        if (buffer) dtFree(buffer);
        buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
        capacity = cap;
    }
    
    virtual void reset()
    {
        high = dtMax(high, top);
        top = 0;
    }
    
    virtual void* alloc(const size_t size)
    {
        if (!buffer)
            return 0;
        if (top+size > capacity)
            return 0;
        unsigned char* mem = &buffer[top];
        top += size;
        return mem;
    }
    
    virtual void free(void* /*ptr*/)
    {
        // Empty
    }
};

struct OffMeshLinkConfig
{
    OffMeshLinkConfig()
        : offMeshConVerts(0)
        , offMeshConRad(0)
        , offMeshConFlags(0)
        , offMeshConAreas(0)
        , offMeshConDir(0)
        , offMeshConUserID(0)
        , offMeshConCount(0)
    {
    }

    float* offMeshConVerts;
    float* offMeshConRad;
    unsigned short* offMeshConFlags;
    unsigned char* offMeshConAreas;
    unsigned char* offMeshConDir;
    unsigned int* offMeshConUserID;
    int offMeshConCount;
};

inline void applyOffMeshLinkConfig(dtNavMeshCreateParams& params, const OffMeshLinkConfig* cfg)
{
    if (!cfg || cfg->offMeshConCount <= 0)
    {
        params.offMeshConVerts = 0;
        params.offMeshConRad = 0;
        params.offMeshConDir = 0;
        params.offMeshConAreas = 0;
        params.offMeshConFlags = 0;
        params.offMeshConUserID = 0;
        params.offMeshConCount = 0;
        return;
    }

    params.offMeshConVerts = cfg->offMeshConVerts;
    params.offMeshConRad = cfg->offMeshConRad;
    params.offMeshConDir = cfg->offMeshConDir;
    params.offMeshConAreas = cfg->offMeshConAreas;
    params.offMeshConFlags = cfg->offMeshConFlags;
    params.offMeshConUserID = cfg->offMeshConUserID;
    params.offMeshConCount = cfg->offMeshConCount;
}

struct RecastMeshProcess : public dtTileCacheMeshProcess
{
    const OffMeshLinkConfig* m_offMesh;

    inline RecastMeshProcess() : m_offMesh(0)
    {
    }

    virtual void process(struct dtNavMeshCreateParams* params,
                         unsigned char* polyAreas, unsigned short* polyFlags)
    {
        // Update poly flags from areas.
        for (int i = 0; i < params->polyCount; ++i)
        {
            polyAreas[i] = 0;
            polyFlags[i] = 1; //SAMPLE_POLYFLAGS_WALK
        }

        applyOffMeshLinkConfig(*params, m_offMesh);
    }
};

class NavMesh
{
public:
    NavMesh() : m_navQuery(0)
        , m_navMesh(0)
        , m_tileCache(0)
        , m_pmesh(0)
        , m_dmesh(0)
        , m_navData(0)
        , m_defaultQueryExtent(1.f)
        , m_talloc(32000)
    {

    }
    void destroy();
    void build(const float* positions, const int positionCount, const int* indices, const int indexCount, const rcConfig& config, OffMeshLinkConfig* offMeshConfig);
    void buildFromNavmeshData(NavmeshData* navmeshData);
    NavmeshData getNavmeshData() const;
    void freeNavmeshData(NavmeshData* navmeshData);

    DebugNavMesh getDebugNavMesh();
    Vec3 getClosestPoint(const Vec3& position);
    Vec3 getRandomPointAround(const Vec3& position, float maxRadius);
    Vec3 moveAlong(const Vec3& position, const Vec3& destination);
    dtNavMesh* getNavMesh() 
    { 
        return m_navMesh; 
    }
    NavPath computePath(const Vec3& start, const Vec3& end) const;
    NavPath computePathSmooth(const Vec3& start, const Vec3& end) const;
    void setDefaultQueryExtent(const Vec3& extent)
    {
        m_defaultQueryExtent = extent;
    }
    Vec3 getDefaultQueryExtent() const
    {
        return m_defaultQueryExtent;
    }

    dtObstacleRef* addCylinderObstacle(const Vec3& position, float radius, float height);
    dtObstacleRef* addBoxObstacle(const Vec3& position, const Vec3& extent, float angle);
    void removeObstacle(dtObstacleRef* obstacle);
    void update();

    dtTileCache* m_tileCache;
    dtNavMeshQuery* m_navQuery;
protected:

    std::list<dtObstacleRef> m_obstacles;
    dtNavMesh* m_navMesh;
    
    rcPolyMesh* m_pmesh;
    rcPolyMeshDetail* m_dmesh;
    unsigned char* m_navData;
    Vec3 m_defaultQueryExtent;

    RecastLinearAllocator m_talloc;
    RecastFastLZCompressor m_tcomp;
    RecastMeshProcess m_tmproc;

    void navMeshPoly(DebugNavMesh& debugNavMesh, const dtNavMesh& mesh, dtPolyRef ref);
    void navMeshPolysWithFlags(DebugNavMesh& debugNavMesh, const dtNavMesh& mesh, const unsigned short polyFlags);
    bool computeTiledNavMesh(const std::vector<float>& verts, const std::vector<int>& tris, rcConfig& cfg, NavMeshintermediates& intermediates, const std::vector<unsigned char>& triareas);
    int rasterizeTileLayers(const int tx, const int ty, const rcConfig& cfg, TileCacheData* tiles, const int maxTiles, NavMeshintermediates& intermediates, const std::vector<unsigned char>& triareas, const std::vector<float>& verts);
};

class Crowd
{
public:
    Crowd(const int maxAgents, const float maxAgentRadius, dtNavMesh* nav);
    void destroy();
    int addAgent(const Vec3& pos, const dtCrowdAgentParams* params);
    void removeAgent(const int idx);
    void update(const float dt);
    Vec3 getAgentPosition(int idx);
    Vec3 getAgentVelocity(int idx);
    Vec3 getAgentNextTargetPath(int idx);
    int getAgentState(int idx);
    bool overOffmeshConnection(int idx);
    void agentGoto(int idx, const Vec3& destination);
    void agentStop(int idx);
    void agentTeleport(int idx, const Vec3& destination);
    dtCrowdAgentParams getAgentParameters(const int idx);
    void setAgentParameters(const int idx, const dtCrowdAgentParams* params);
    void setDefaultQueryExtent(const Vec3& extent)
    {
        m_defaultQueryExtent = extent;
    }
    Vec3 getDefaultQueryExtent() const
    {
        return m_defaultQueryExtent;
    }
    NavPath getCorners(const int idx);
    NavPath getPath(const int idx);

protected:

    dtCrowd *m_crowd;
    Vec3 m_defaultQueryExtent;
};
