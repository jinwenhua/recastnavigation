#include "recastjsb_offmesh.h"

RecastJsbOffMeshLinkConfig::RecastJsbOffMeshLinkConfig()
{
    config.offMeshConVerts = 0;
    config.offMeshConRad = 0;
    config.offMeshConFlags = 0;
    config.offMeshConAreas = 0;
    config.offMeshConDir = 0;
    config.offMeshConUserID = 0;
    config.offMeshConCount = 0;
}

void RecastJsbOffMeshLinkConfig::syncPointers()
{
    config.offMeshConVerts = ownedVerts.empty() ? 0 : ownedVerts.data();
    config.offMeshConRad = ownedRad.empty() ? 0 : ownedRad.data();
    config.offMeshConFlags = ownedFlags.empty() ? 0 : ownedFlags.data();
    config.offMeshConAreas = ownedAreas.empty() ? 0 : ownedAreas.data();
    config.offMeshConDir = ownedDir.empty() ? 0 : ownedDir.data();
    config.offMeshConUserID = ownedUserID.empty() ? 0 : ownedUserID.data();
}

RecastJsbOffMeshLinkConfig* RecastJsbOffMeshLinkConfig::create(const float* verts,
                                                               const float* rad,
                                                               const unsigned short* flags,
                                                               const unsigned char* areas,
                                                               const unsigned char* dir,
                                                               const unsigned int* userID,
                                                               int count)
{
    RecastJsbOffMeshLinkConfig* cfg = new RecastJsbOffMeshLinkConfig();
    cfg->config.offMeshConCount = count;
    if (count <= 0)
    {
        return cfg;
    }

    if (verts)
    {
        cfg->ownedVerts.assign(verts, verts + count * 6);
    }
    if (rad)
    {
        cfg->ownedRad.assign(rad, rad + count);
    }
    if (flags)
    {
        cfg->ownedFlags.assign(flags, flags + count);
    }
    if (areas)
    {
        cfg->ownedAreas.assign(areas, areas + count);
    }
    if (dir)
    {
        cfg->ownedDir.assign(dir, dir + count);
    }
    if (userID)
    {
        cfg->ownedUserID.assign(userID, userID + count);
    }
    cfg->syncPointers();
    return cfg;
}
