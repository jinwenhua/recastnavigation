#pragma once

#include "recastjs.h"

#include <vector>

struct RecastJsbOffMeshLinkConfig
{
    OffMeshLinkConfig config;

    std::vector<float> ownedVerts;
    std::vector<float> ownedRad;
    std::vector<unsigned short> ownedFlags;
    std::vector<unsigned char> ownedAreas;
    std::vector<unsigned char> ownedDir;
    std::vector<unsigned int> ownedUserID;

    RecastJsbOffMeshLinkConfig();

    void syncPointers();

    static RecastJsbOffMeshLinkConfig* create(const float* verts,
                                              const float* rad,
                                              const unsigned short* flags,
                                              const unsigned char* areas,
                                              const unsigned char* dir,
                                              const unsigned int* userID,
                                              int count);
};
