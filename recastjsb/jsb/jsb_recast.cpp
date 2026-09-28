#include "jsb_recast.h"

#include "recastjsb_offmesh.h"
#include "recastjs.h"
#include "Recast.h"

#if __has_include("cocos/bindings/jswrapper/SeApi.h")
#include "cocos/bindings/jswrapper/SeApi.h"
#elif __has_include("bindings/jswrapper/SeApi.h")
#include "bindings/jswrapper/SeApi.h"
#else
#include "SeApi.h"
#endif

#include <cstring>
#include <vector>

namespace {

se::Class* js_class_Vec3 = nullptr;
se::Class* js_class_Triangle = nullptr;
se::Class* js_class_DebugNavMesh = nullptr;
se::Class* js_class_NavPath = nullptr;
se::Class* js_class_NavmeshData = nullptr;
se::Class* js_class_rcConfig = nullptr;
se::Class* js_class_OffMeshLinkConfig = nullptr;
se::Class* js_class_dtCrowdAgentParams = nullptr;
se::Class* js_class_dtNavMesh = nullptr;
se::Class* js_class_dtObstacleRef = nullptr;
se::Class* js_class_NavMesh = nullptr;
se::Class* js_class_Crowd = nullptr;

#define RECAST_OWNED_FINALIZE(Type)                                                 \
    static bool js_##Type##_finalize(se::State& s)                                  \
    {                                                                               \
        delete static_cast<Type*>(s.nativeThisObject());                            \
        return true;                                                                \
    }                                                                               \
    SE_BIND_FINALIZE_FUNC(js_##Type##_finalize)

RECAST_OWNED_FINALIZE(Vec3)
RECAST_OWNED_FINALIZE(Triangle)
RECAST_OWNED_FINALIZE(DebugNavMesh)
RECAST_OWNED_FINALIZE(NavPath)
RECAST_OWNED_FINALIZE(NavmeshData)
RECAST_OWNED_FINALIZE(rcConfig)
RECAST_OWNED_FINALIZE(RecastJsbOffMeshLinkConfig)
RECAST_OWNED_FINALIZE(dtCrowdAgentParams)
RECAST_OWNED_FINALIZE(NavMesh)
RECAST_OWNED_FINALIZE(Crowd)

static bool js_noop_finalize(se::State& /*s*/)
{
    return true;
}
SE_BIND_FINALIZE_FUNC(js_noop_finalize)

bool seval_to_number_array(const se::Value& value, std::vector<double>& out)
{
    if (!value.isObject())
    {
        return false;
    }

    se::Object* obj = value.toObject();
    if (obj->isTypedArray())
    {
        uint8_t* bytes = nullptr;
        size_t byteLen = 0;
        if (!obj->getTypedArrayData(&bytes, &byteLen) || !bytes)
        {
            return false;
        }

        se::Object::TypedArrayType type = obj->getTypedArrayType();
        switch (type)
        {
        case se::Object::TypedArrayType::FLOAT32: {
            const size_t n = byteLen / sizeof(float);
            const auto* src = reinterpret_cast<const float*>(bytes);
            out.assign(src, src + n);
            return true;
        }
        case se::Object::TypedArrayType::INT32:
        case se::Object::TypedArrayType::UINT32: {
            const size_t n = byteLen / sizeof(int32_t);
            const auto* src = reinterpret_cast<const int32_t*>(bytes);
            out.assign(src, src + n);
            return true;
        }
        case se::Object::TypedArrayType::INT16:
        case se::Object::TypedArrayType::UINT16: {
            const size_t n = byteLen / sizeof(uint16_t);
            const auto* src = reinterpret_cast<const uint16_t*>(bytes);
            out.assign(src, src + n);
            return true;
        }
        case se::Object::TypedArrayType::INT8:
        case se::Object::TypedArrayType::UINT8: {
            out.assign(bytes, bytes + byteLen);
            return true;
        }
        default:
            break;
        }
    }

    uint32_t len = 0;
    if (!obj->getArrayLength(&len))
    {
        return false;
    }
    out.resize(len);
    for (uint32_t i = 0; i < len; ++i)
    {
        se::Value item;
        if (!obj->getArrayElement(i, &item))
        {
            return false;
        }
        out[i] = item.toDouble();
    }
    return true;
}

template <typename T>
bool seval_to_vector(const se::Value& value, std::vector<T>& out)
{
    std::vector<double> numbers;
    if (!seval_to_number_array(value, numbers))
    {
        return false;
    }
    out.resize(numbers.size());
    for (size_t i = 0; i < numbers.size(); ++i)
    {
        out[i] = static_cast<T>(numbers[i]);
    }
    return true;
}

bool seval_to_bytes(const se::Value& value, std::vector<unsigned char>& out)
{
    if (!value.isObject())
    {
        return false;
    }
    se::Object* obj = value.toObject();
    if (obj->isTypedArray())
    {
        uint8_t* bytes = nullptr;
        size_t byteLen = 0;
        if (!obj->getTypedArrayData(&bytes, &byteLen) || !bytes)
        {
            return false;
        }
        out.assign(bytes, bytes + byteLen);
        return true;
    }
    return seval_to_vector<unsigned char>(value, out);
}

se::Object* wrap_native(se::Class* cls, void* ptr, bool /*owned*/)
{
    se::Object* obj = se::Object::createObjectWithClass(cls);
    obj->setPrivateData(ptr);
    return obj;
}

Vec3* seval_to_vec3(const se::Value& value)
{
    if (!value.isObject())
    {
        return nullptr;
    }
    return static_cast<Vec3*>(value.toObject()->getPrivateData());
}

rcConfig* seval_to_rcConfig(const se::Value& value)
{
    if (!value.isObject())
    {
        return nullptr;
    }
    return static_cast<rcConfig*>(value.toObject()->getPrivateData());
}

RecastJsbOffMeshLinkConfig* seval_to_jsb_offmesh(const se::Value& value)
{
    if (value.isNullOrUndefined() || !value.isObject())
    {
        return nullptr;
    }
    return static_cast<RecastJsbOffMeshLinkConfig*>(value.toObject()->getPrivateData());
}

OffMeshLinkConfig* seval_to_offmesh(const se::Value& value)
{
    RecastJsbOffMeshLinkConfig* cfg = seval_to_jsb_offmesh(value);
    return cfg ? &cfg->config : nullptr;
}

#define RECAST_PROP_FLOAT(Type, Field)                                              \
    static bool js_##Type##_get_##Field(se::State& s)                               \
    {                                                                               \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        s.rval().setFloat(self->Field);                                             \
        return true;                                                                \
    }                                                                               \
    static bool js_##Type##_set_##Field(se::State& s)                               \
    {                                                                               \
        const auto& args = s.args();                                                \
        if (args.empty()) return false;                                             \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        self->Field = args[0].toFloat();                                            \
        return true;                                                                \
    }                                                                               \
    SE_BIND_PROP_GET(js_##Type##_get_##Field)                                       \
    SE_BIND_PROP_SET(js_##Type##_set_##Field)

#define RECAST_PROP_INT(Type, Field)                                                \
    static bool js_##Type##_get_##Field(se::State& s)                               \
    {                                                                               \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        s.rval().setInt32(static_cast<int32_t>(self->Field));                       \
        return true;                                                                \
    }                                                                               \
    static bool js_##Type##_set_##Field(se::State& s)                               \
    {                                                                               \
        const auto& args = s.args();                                                \
        if (args.empty()) return false;                                             \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        self->Field = args[0].toInt32();                                            \
        return true;                                                                \
    }                                                                               \
    SE_BIND_PROP_GET(js_##Type##_get_##Field)                                       \
    SE_BIND_PROP_SET(js_##Type##_set_##Field)

#define RECAST_PROP_U8(Type, Field)                                                 \
    static bool js_##Type##_get_##Field(se::State& s)                               \
    {                                                                               \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        s.rval().setUint32(self->Field);                                            \
        return true;                                                                \
    }                                                                               \
    static bool js_##Type##_set_##Field(se::State& s)                               \
    {                                                                               \
        const auto& args = s.args();                                                \
        if (args.empty()) return false;                                             \
        auto* self = static_cast<Type*>(s.nativeThisObject());                      \
        self->Field = static_cast<unsigned char>(args[0].toUint32());               \
        return true;                                                                \
    }                                                                               \
    SE_BIND_PROP_GET(js_##Type##_get_##Field)                                       \
    SE_BIND_PROP_SET(js_##Type##_set_##Field)

RECAST_PROP_FLOAT(Vec3, x)
RECAST_PROP_FLOAT(Vec3, y)
RECAST_PROP_FLOAT(Vec3, z)

static bool js_Vec3_ctor(se::State& s)
{
    const auto& args = s.args();
    Vec3* v = nullptr;
    if (args.size() >= 3)
    {
        v = new Vec3(args[0].toFloat(), args[1].toFloat(), args[2].toFloat());
    }
    else
    {
        v = new Vec3();
    }
    s.thisObject()->setPrivateData(v);
    return true;
}
SE_BIND_CTOR(js_Vec3_ctor, js_class_Vec3, js_Vec3_finalize)

static bool js_Triangle_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(new Triangle());
    return true;
}
SE_BIND_CTOR(js_Triangle_ctor, js_class_Triangle, js_Triangle_finalize)

static bool js_Triangle_getPoint(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* self = static_cast<Triangle*>(s.nativeThisObject());
    auto* out = new Vec3(self->getPoint(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_Triangle_getPoint)

static bool js_DebugNavMesh_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(new DebugNavMesh());
    return true;
}
SE_BIND_CTOR(js_DebugNavMesh_ctor, js_class_DebugNavMesh, js_DebugNavMesh_finalize)

static bool js_DebugNavMesh_getTriangleCount(se::State& s)
{
    auto* self = static_cast<DebugNavMesh*>(s.nativeThisObject());
    s.rval().setInt32(self->getTriangleCount());
    return true;
}
SE_BIND_FUNC(js_DebugNavMesh_getTriangleCount)

static bool js_DebugNavMesh_getTriangle(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* self = static_cast<DebugNavMesh*>(s.nativeThisObject());
    auto* out = new Triangle(self->getTriangle(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Triangle, out, true));
    return true;
}
SE_BIND_FUNC(js_DebugNavMesh_getTriangle)

static bool js_NavPath_getPointCount(se::State& s)
{
    auto* self = static_cast<NavPath*>(s.nativeThisObject());
    s.rval().setInt32(self->getPointCount());
    return true;
}
SE_BIND_FUNC(js_NavPath_getPointCount)

static bool js_NavPath_getPoint(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* self = static_cast<NavPath*>(s.nativeThisObject());
    auto* out = new Vec3(self->getPoint(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_NavPath_getPoint)


static bool js_NavmeshData_ctor(se::State& s)
{
    auto* data = new NavmeshData();
    data->dataPointer = nullptr;
    data->size = 0;
    s.thisObject()->setPrivateData(data);
    return true;
}
SE_BIND_CTOR(js_NavmeshData_ctor, js_class_NavmeshData, js_NavmeshData_finalize)

RECAST_PROP_INT(NavmeshData, size)

static bool js_rcConfig_ctor(se::State& s)
{
    auto* cfg = new rcConfig();
    memset(cfg, 0, sizeof(rcConfig));
    s.thisObject()->setPrivateData(cfg);
    return true;
}
SE_BIND_CTOR(js_rcConfig_ctor, js_class_rcConfig, js_rcConfig_finalize)

RECAST_PROP_INT(rcConfig, width)
RECAST_PROP_INT(rcConfig, height)
RECAST_PROP_INT(rcConfig, tileSize)
RECAST_PROP_INT(rcConfig, borderSize)
RECAST_PROP_FLOAT(rcConfig, cs)
RECAST_PROP_FLOAT(rcConfig, ch)
RECAST_PROP_FLOAT(rcConfig, walkableSlopeAngle)
RECAST_PROP_INT(rcConfig, walkableHeight)
RECAST_PROP_INT(rcConfig, walkableClimb)
RECAST_PROP_INT(rcConfig, walkableRadius)
RECAST_PROP_INT(rcConfig, maxEdgeLen)
RECAST_PROP_FLOAT(rcConfig, maxSimplificationError)
RECAST_PROP_INT(rcConfig, minRegionArea)
RECAST_PROP_INT(rcConfig, mergeRegionArea)
RECAST_PROP_INT(rcConfig, maxVertsPerPoly)
RECAST_PROP_FLOAT(rcConfig, detailSampleDist)
RECAST_PROP_FLOAT(rcConfig, detailSampleMaxError)

static bool js_OffMeshLinkConfig_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(new RecastJsbOffMeshLinkConfig());
    return true;
}
SE_BIND_CTOR(js_OffMeshLinkConfig_ctor, js_class_OffMeshLinkConfig, js_RecastJsbOffMeshLinkConfig_finalize)

static bool js_OffMeshLinkConfig_get_offMeshConCount(se::State& s)
{
    auto* self = static_cast<RecastJsbOffMeshLinkConfig*>(s.nativeThisObject());
    s.rval().setInt32(self->config.offMeshConCount);
    return true;
}
SE_BIND_PROP_GET(js_OffMeshLinkConfig_get_offMeshConCount)

static bool js_OffMeshLinkConfig_set_offMeshConCount(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* self = static_cast<RecastJsbOffMeshLinkConfig*>(s.nativeThisObject());
    self->config.offMeshConCount = args[0].toInt32();
    return true;
}
SE_BIND_PROP_SET(js_OffMeshLinkConfig_set_offMeshConCount)

static bool js_OffMeshLinkConfig_GetInstance(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 7)
    {
        SE_REPORT_ERROR("OffMeshLinkConfig.GetInstance expects 7 arguments");
        return false;
    }

    std::vector<float> verts;
    std::vector<float> rad;
    std::vector<unsigned short> flags;
    std::vector<unsigned char> areas;
    std::vector<unsigned char> dir;
    std::vector<unsigned int> userID;
    if (!seval_to_vector<float>(args[0], verts) ||
        !seval_to_vector<float>(args[1], rad) ||
        !seval_to_vector<unsigned short>(args[2], flags) ||
        !seval_to_vector<unsigned char>(args[3], areas) ||
        !seval_to_vector<unsigned char>(args[4], dir) ||
        !seval_to_vector<unsigned int>(args[5], userID))
    {
        SE_REPORT_ERROR("OffMeshLinkConfig.GetInstance: invalid arrays");
        return false;
    }

    const int count = args[6].toInt32();
    if (count > 0)
    {
        if (verts.size() < static_cast<size_t>(count) * 6u ||
            rad.size() < static_cast<size_t>(count) ||
            flags.size() < static_cast<size_t>(count) ||
            areas.size() < static_cast<size_t>(count) ||
            dir.size() < static_cast<size_t>(count) ||
            userID.size() < static_cast<size_t>(count))
        {
            SE_REPORT_ERROR("OffMeshLinkConfig.GetInstance: array length mismatch");
            return false;
        }
    }

    RecastJsbOffMeshLinkConfig* cfg = RecastJsbOffMeshLinkConfig::create(
        verts.empty() ? nullptr : verts.data(),
        rad.empty() ? nullptr : rad.data(),
        flags.empty() ? nullptr : flags.data(),
        areas.empty() ? nullptr : areas.data(),
        dir.empty() ? nullptr : dir.data(),
        userID.empty() ? nullptr : userID.data(),
        count);
    s.rval().setObject(wrap_native(js_class_OffMeshLinkConfig, cfg, true));
    return true;
}
SE_BIND_FUNC(js_OffMeshLinkConfig_GetInstance)

static bool js_dtCrowdAgentParams_ctor(se::State& s)
{
    auto* params = new dtCrowdAgentParams();
    memset(params, 0, sizeof(dtCrowdAgentParams));
    s.thisObject()->setPrivateData(params);
    return true;
}
SE_BIND_CTOR(js_dtCrowdAgentParams_ctor, js_class_dtCrowdAgentParams, js_dtCrowdAgentParams_finalize)

RECAST_PROP_FLOAT(dtCrowdAgentParams, radius)
RECAST_PROP_FLOAT(dtCrowdAgentParams, height)
RECAST_PROP_FLOAT(dtCrowdAgentParams, maxAcceleration)
RECAST_PROP_FLOAT(dtCrowdAgentParams, maxSpeed)
RECAST_PROP_FLOAT(dtCrowdAgentParams, collisionQueryRange)
RECAST_PROP_FLOAT(dtCrowdAgentParams, pathOptimizationRange)
RECAST_PROP_FLOAT(dtCrowdAgentParams, separationWeight)
RECAST_PROP_U8(dtCrowdAgentParams, updateFlags)
RECAST_PROP_U8(dtCrowdAgentParams, obstacleAvoidanceType)
RECAST_PROP_U8(dtCrowdAgentParams, queryFilterType)

static bool js_dtNavMesh_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(nullptr);
    return true;
}
SE_BIND_CTOR(js_dtNavMesh_ctor, js_class_dtNavMesh, js_noop_finalize)

static bool js_dtObstacleRef_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(nullptr);
    return true;
}
SE_BIND_CTOR(js_dtObstacleRef_ctor, js_class_dtObstacleRef, js_noop_finalize)

static bool js_NavMesh_ctor(se::State& s)
{
    s.thisObject()->setPrivateData(new NavMesh());
    return true;
}
SE_BIND_CTOR(js_NavMesh_ctor, js_class_NavMesh, js_NavMesh_finalize)

static bool js_NavMesh_destroy(se::State& s)
{
    static_cast<NavMesh*>(s.nativeThisObject())->destroy();
    return true;
}
SE_BIND_FUNC(js_NavMesh_destroy)

static bool js_NavMesh_build(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 5)
    {
        SE_REPORT_ERROR("NavMesh.build expects at least 5 arguments");
        return false;
    }

    std::vector<float> positions;
    std::vector<int> indices;
    if (!seval_to_vector<float>(args[0], positions) || !seval_to_vector<int>(args[2], indices))
    {
        SE_REPORT_ERROR("NavMesh.build: invalid mesh arrays");
        return false;
    }

    rcConfig* cfg = seval_to_rcConfig(args[4]);
    if (!cfg)
    {
        SE_REPORT_ERROR("NavMesh.build: invalid rcConfig");
        return false;
    }

    OffMeshLinkConfig* offMesh = nullptr;
    if (args.size() >= 6)
    {
        offMesh = seval_to_offmesh(args[5]);
    }

    const int positionCount = args[1].toInt32();
    const int indexCount = args[3].toInt32();
    if (positionCount < 0 || indexCount < 0 ||
        static_cast<size_t>(positionCount) * 3u > positions.size() ||
        static_cast<size_t>(indexCount) > indices.size())
    {
        SE_REPORT_ERROR("NavMesh.build: count exceeds array length");
        return false;
    }
    static_cast<NavMesh*>(s.nativeThisObject())->build(
        positions.empty() ? nullptr : positions.data(),
        positionCount,
        indices.empty() ? nullptr : indices.data(),
        indexCount,
        *cfg,
        offMesh);
    return true;
}
SE_BIND_FUNC(js_NavMesh_build)

static bool js_NavMesh_buildFromNavmeshData(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;

    auto* self = static_cast<NavMesh*>(s.nativeThisObject());
    std::vector<unsigned char> bytes;
    if (seval_to_bytes(args[0], bytes))
    {
        NavmeshData data;
        data.dataPointer = bytes.data();
        data.size = static_cast<int>(bytes.size());
        self->buildFromNavmeshData(&data);
        return true;
    }

    if (args[0].isObject())
    {
        auto* data = static_cast<NavmeshData*>(args[0].toObject()->getPrivateData());
        if (data)
        {
            self->buildFromNavmeshData(data);
            return true;
        }
    }
    SE_REPORT_ERROR("NavMesh.buildFromNavmeshData: expected Uint8Array or NavmeshData");
    return false;
}
SE_BIND_FUNC(js_NavMesh_buildFromNavmeshData)

static bool js_NavMesh_getNavmeshData(se::State& s)
{
    auto* self = static_cast<NavMesh*>(s.nativeThisObject());
    NavmeshData data = self->getNavmeshData();
    se::Object* arr = se::Object::createTypedArray(se::Object::TypedArrayType::UINT8, data.dataPointer, static_cast<size_t>(data.size));
    s.rval().setObject(arr);
    self->freeNavmeshData(&data);
    return true;
}
SE_BIND_FUNC(js_NavMesh_getNavmeshData)

static bool js_NavMesh_freeNavmeshData(se::State& s)
{
    const auto& args = s.args();
    if (args.empty() || !args[0].isObject()) return false;
    auto* data = static_cast<NavmeshData*>(args[0].toObject()->getPrivateData());
    if (data)
    {
        static_cast<NavMesh*>(s.nativeThisObject())->freeNavmeshData(data);
    }
    return true;
}
SE_BIND_FUNC(js_NavMesh_freeNavmeshData)

static bool js_NavMesh_getDebugNavMesh(se::State& s)
{
    auto* out = new DebugNavMesh(static_cast<NavMesh*>(s.nativeThisObject())->getDebugNavMesh());
    s.rval().setObject(wrap_native(js_class_DebugNavMesh, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_getDebugNavMesh)

static bool js_NavMesh_getClosestPoint(se::State& s)
{
    const auto& args = s.args();
    Vec3* pos = args.empty() ? nullptr : seval_to_vec3(args[0]);
    if (!pos) return false;
    auto* out = new Vec3(static_cast<NavMesh*>(s.nativeThisObject())->getClosestPoint(*pos));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_getClosestPoint)

static bool js_NavMesh_getRandomPointAround(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* pos = seval_to_vec3(args[0]);
    if (!pos) return false;
    auto* out = new Vec3(static_cast<NavMesh*>(s.nativeThisObject())->getRandomPointAround(*pos, args[1].toFloat()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_getRandomPointAround)

static bool js_NavMesh_moveAlong(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* pos = seval_to_vec3(args[0]);
    Vec3* dest = seval_to_vec3(args[1]);
    if (!pos || !dest) return false;
    auto* out = new Vec3(static_cast<NavMesh*>(s.nativeThisObject())->moveAlong(*pos, *dest));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_moveAlong)

static bool js_NavMesh_getNavMesh(se::State& s)
{
    dtNavMesh* nav = static_cast<NavMesh*>(s.nativeThisObject())->getNavMesh();
    s.rval().setObject(wrap_native(js_class_dtNavMesh, nav, false));
    return true;
}
SE_BIND_FUNC(js_NavMesh_getNavMesh)

static bool js_NavMesh_computePath(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* start = seval_to_vec3(args[0]);
    Vec3* end = seval_to_vec3(args[1]);
    if (!start || !end) return false;
    auto* out = new NavPath(static_cast<NavMesh*>(s.nativeThisObject())->computePath(*start, *end));
    s.rval().setObject(wrap_native(js_class_NavPath, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_computePath)

static bool js_NavMesh_computePathSmooth(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* start = seval_to_vec3(args[0]);
    Vec3* end = seval_to_vec3(args[1]);
    if (!start || !end) return false;
    auto* out = new NavPath(static_cast<NavMesh*>(s.nativeThisObject())->computePathSmooth(*start, *end));
    s.rval().setObject(wrap_native(js_class_NavPath, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_computePathSmooth)

static bool js_NavMesh_setDefaultQueryExtent(se::State& s)
{
    const auto& args = s.args();
    Vec3* extent = args.empty() ? nullptr : seval_to_vec3(args[0]);
    if (!extent) return false;
    static_cast<NavMesh*>(s.nativeThisObject())->setDefaultQueryExtent(*extent);
    return true;
}
SE_BIND_FUNC(js_NavMesh_setDefaultQueryExtent)

static bool js_NavMesh_getDefaultQueryExtent(se::State& s)
{
    auto* out = new Vec3(static_cast<NavMesh*>(s.nativeThisObject())->getDefaultQueryExtent());
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_NavMesh_getDefaultQueryExtent)

static bool js_NavMesh_addCylinderObstacle(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 3) return false;
    Vec3* pos = seval_to_vec3(args[0]);
    if (!pos) return false;
    dtObstacleRef* ref = static_cast<NavMesh*>(s.nativeThisObject())->addCylinderObstacle(*pos, args[1].toFloat(), args[2].toFloat());
    s.rval().setObject(wrap_native(js_class_dtObstacleRef, ref, false));
    return true;
}
SE_BIND_FUNC(js_NavMesh_addCylinderObstacle)

static bool js_NavMesh_addBoxObstacle(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 3) return false;
    Vec3* pos = seval_to_vec3(args[0]);
    Vec3* extent = seval_to_vec3(args[1]);
    if (!pos || !extent) return false;
    dtObstacleRef* ref = static_cast<NavMesh*>(s.nativeThisObject())->addBoxObstacle(*pos, *extent, args[2].toFloat());
    s.rval().setObject(wrap_native(js_class_dtObstacleRef, ref, false));
    return true;
}
SE_BIND_FUNC(js_NavMesh_addBoxObstacle)

static bool js_NavMesh_removeObstacle(se::State& s)
{
    const auto& args = s.args();
    if (args.empty() || !args[0].isObject()) return false;
    auto* obstacle = static_cast<dtObstacleRef*>(args[0].toObject()->getPrivateData());
    static_cast<NavMesh*>(s.nativeThisObject())->removeObstacle(obstacle);
    return true;
}
SE_BIND_FUNC(js_NavMesh_removeObstacle)

static bool js_NavMesh_update(se::State& s)
{
    static_cast<NavMesh*>(s.nativeThisObject())->update();
    return true;
}
SE_BIND_FUNC(js_NavMesh_update)

static bool js_Crowd_ctor(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 3 || !args[2].isObject())
    {
        SE_REPORT_ERROR("Crowd constructor expects (maxAgents, maxAgentRadius, dtNavMesh)");
        return false;
    }
    auto* nav = static_cast<dtNavMesh*>(args[2].toObject()->getPrivateData());
    s.thisObject()->setPrivateData(new Crowd(args[0].toInt32(), args[1].toFloat(), nav));
    return true;
}
SE_BIND_CTOR(js_Crowd_ctor, js_class_Crowd, js_Crowd_finalize)

static bool js_Crowd_destroy(se::State& s)
{
    static_cast<Crowd*>(s.nativeThisObject())->destroy();
    return true;
}
SE_BIND_FUNC(js_Crowd_destroy)

static bool js_Crowd_addAgent(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* pos = seval_to_vec3(args[0]);
    auto* params = args[1].isObject() ? static_cast<dtCrowdAgentParams*>(args[1].toObject()->getPrivateData()) : nullptr;
    if (!pos || !params) return false;
    s.rval().setInt32(static_cast<Crowd*>(s.nativeThisObject())->addAgent(*pos, params));
    return true;
}
SE_BIND_FUNC(js_Crowd_addAgent)

static bool js_Crowd_removeAgent(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    static_cast<Crowd*>(s.nativeThisObject())->removeAgent(args[0].toInt32());
    return true;
}
SE_BIND_FUNC(js_Crowd_removeAgent)

static bool js_Crowd_update(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    static_cast<Crowd*>(s.nativeThisObject())->update(args[0].toFloat());
    return true;
}
SE_BIND_FUNC(js_Crowd_update)

static bool js_Crowd_getAgentPosition(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new Vec3(static_cast<Crowd*>(s.nativeThisObject())->getAgentPosition(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getAgentPosition)

static bool js_Crowd_getAgentVelocity(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new Vec3(static_cast<Crowd*>(s.nativeThisObject())->getAgentVelocity(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getAgentVelocity)

static bool js_Crowd_getAgentNextTargetPath(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new Vec3(static_cast<Crowd*>(s.nativeThisObject())->getAgentNextTargetPath(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getAgentNextTargetPath)

static bool js_Crowd_getAgentState(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    s.rval().setInt32(static_cast<Crowd*>(s.nativeThisObject())->getAgentState(args[0].toInt32()));
    return true;
}
SE_BIND_FUNC(js_Crowd_getAgentState)

static bool js_Crowd_overOffmeshConnection(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    s.rval().setBoolean(static_cast<Crowd*>(s.nativeThisObject())->overOffmeshConnection(args[0].toInt32()));
    return true;
}
SE_BIND_FUNC(js_Crowd_overOffmeshConnection)

static bool js_Crowd_agentGoto(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* dest = seval_to_vec3(args[1]);
    if (!dest) return false;
    static_cast<Crowd*>(s.nativeThisObject())->agentGoto(args[0].toInt32(), *dest);
    return true;
}
SE_BIND_FUNC(js_Crowd_agentGoto)

static bool js_Crowd_agentStop(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    static_cast<Crowd*>(s.nativeThisObject())->agentStop(args[0].toInt32());
    return true;
}
SE_BIND_FUNC(js_Crowd_agentStop)

static bool js_Crowd_agentTeleport(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2) return false;
    Vec3* dest = seval_to_vec3(args[1]);
    if (!dest) return false;
    static_cast<Crowd*>(s.nativeThisObject())->agentTeleport(args[0].toInt32(), *dest);
    return true;
}
SE_BIND_FUNC(js_Crowd_agentTeleport)

static bool js_Crowd_getAgentParameters(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new dtCrowdAgentParams(static_cast<Crowd*>(s.nativeThisObject())->getAgentParameters(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_dtCrowdAgentParams, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getAgentParameters)

static bool js_Crowd_setAgentParameters(se::State& s)
{
    const auto& args = s.args();
    if (args.size() < 2 || !args[1].isObject()) return false;
    auto* params = static_cast<dtCrowdAgentParams*>(args[1].toObject()->getPrivateData());
    if (!params) return false;
    static_cast<Crowd*>(s.nativeThisObject())->setAgentParameters(args[0].toInt32(), params);
    return true;
}
SE_BIND_FUNC(js_Crowd_setAgentParameters)

static bool js_Crowd_setDefaultQueryExtent(se::State& s)
{
    const auto& args = s.args();
    Vec3* extent = args.empty() ? nullptr : seval_to_vec3(args[0]);
    if (!extent) return false;
    static_cast<Crowd*>(s.nativeThisObject())->setDefaultQueryExtent(*extent);
    return true;
}
SE_BIND_FUNC(js_Crowd_setDefaultQueryExtent)

static bool js_Crowd_getDefaultQueryExtent(se::State& s)
{
    auto* out = new Vec3(static_cast<Crowd*>(s.nativeThisObject())->getDefaultQueryExtent());
    s.rval().setObject(wrap_native(js_class_Vec3, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getDefaultQueryExtent)

static bool js_Crowd_getCorners(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new NavPath(static_cast<Crowd*>(s.nativeThisObject())->getCorners(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_NavPath, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getCorners)

static bool js_Crowd_getPath(se::State& s)
{
    const auto& args = s.args();
    if (args.empty()) return false;
    auto* out = new NavPath(static_cast<Crowd*>(s.nativeThisObject())->getPath(args[0].toInt32()));
    s.rval().setObject(wrap_native(js_class_NavPath, out, true));
    return true;
}
SE_BIND_FUNC(js_Crowd_getPath)

} // namespace

static void recastjsb_patch_offmesh_prototype(se::Object* ns)
{
    se::Value ctorVal;
    if (!ns->getProperty("OffMeshLinkConfig", &ctorVal) || !ctorVal.isObject())
    {
        return;
    }
    se::Object* ctorObj = ctorVal.toObject();
    se::Value getInstanceVal;
    if (!ctorObj->getProperty("GetInstance", &getInstanceVal))
    {
        return;
    }
    se::Object* proto = ctorObj->getProto();
    if (proto)
    {
        proto->setProperty("GetInstance", getInstanceVal);
    }
}

bool register_all_recastjsb(se::Object* obj)
{
    se::HandleObject ns(se::Object::createPlainObject());

    js_class_Vec3 = se::Class::create("Vec3", ns.get(), nullptr, _SE(js_Vec3_ctor));
    js_class_Vec3->defineFinalizeFunction(_SE(js_Vec3_finalize));
    js_class_Vec3->defineProperty("x", _SE(js_Vec3_get_x), _SE(js_Vec3_set_x));
    js_class_Vec3->defineProperty("y", _SE(js_Vec3_get_y), _SE(js_Vec3_set_y));
    js_class_Vec3->defineProperty("z", _SE(js_Vec3_get_z), _SE(js_Vec3_set_z));
    js_class_Vec3->install();

    js_class_Triangle = se::Class::create("Triangle", ns.get(), nullptr, _SE(js_Triangle_ctor));
    js_class_Triangle->defineFinalizeFunction(_SE(js_Triangle_finalize));
    js_class_Triangle->defineFunction("getPoint", _SE(js_Triangle_getPoint));
    js_class_Triangle->install();

    js_class_DebugNavMesh = se::Class::create("DebugNavMesh", ns.get(), nullptr, _SE(js_DebugNavMesh_ctor));
    js_class_DebugNavMesh->defineFinalizeFunction(_SE(js_DebugNavMesh_finalize));
    js_class_DebugNavMesh->defineFunction("getTriangleCount", _SE(js_DebugNavMesh_getTriangleCount));
    js_class_DebugNavMesh->defineFunction("getTriangle", _SE(js_DebugNavMesh_getTriangle));
    js_class_DebugNavMesh->install();

    js_class_NavPath = se::Class::create("NavPath", ns.get(), nullptr, nullptr);
    js_class_NavPath->defineFinalizeFunction(_SE(js_NavPath_finalize));
    js_class_NavPath->defineFunction("getPointCount", _SE(js_NavPath_getPointCount));
    js_class_NavPath->defineFunction("getPoint", _SE(js_NavPath_getPoint));
    js_class_NavPath->install();

    js_class_NavmeshData = se::Class::create("NavmeshData", ns.get(), nullptr, _SE(js_NavmeshData_ctor));
    js_class_NavmeshData->defineFinalizeFunction(_SE(js_NavmeshData_finalize));
    js_class_NavmeshData->defineProperty("size", _SE(js_NavmeshData_get_size), _SE(js_NavmeshData_set_size));
    js_class_NavmeshData->install();

    js_class_rcConfig = se::Class::create("rcConfig", ns.get(), nullptr, _SE(js_rcConfig_ctor));
    js_class_rcConfig->defineFinalizeFunction(_SE(js_rcConfig_finalize));
    js_class_rcConfig->defineProperty("width", _SE(js_rcConfig_get_width), _SE(js_rcConfig_set_width));
    js_class_rcConfig->defineProperty("height", _SE(js_rcConfig_get_height), _SE(js_rcConfig_set_height));
    js_class_rcConfig->defineProperty("tileSize", _SE(js_rcConfig_get_tileSize), _SE(js_rcConfig_set_tileSize));
    js_class_rcConfig->defineProperty("borderSize", _SE(js_rcConfig_get_borderSize), _SE(js_rcConfig_set_borderSize));
    js_class_rcConfig->defineProperty("cs", _SE(js_rcConfig_get_cs), _SE(js_rcConfig_set_cs));
    js_class_rcConfig->defineProperty("ch", _SE(js_rcConfig_get_ch), _SE(js_rcConfig_set_ch));
    js_class_rcConfig->defineProperty("walkableSlopeAngle", _SE(js_rcConfig_get_walkableSlopeAngle), _SE(js_rcConfig_set_walkableSlopeAngle));
    js_class_rcConfig->defineProperty("walkableHeight", _SE(js_rcConfig_get_walkableHeight), _SE(js_rcConfig_set_walkableHeight));
    js_class_rcConfig->defineProperty("walkableClimb", _SE(js_rcConfig_get_walkableClimb), _SE(js_rcConfig_set_walkableClimb));
    js_class_rcConfig->defineProperty("walkableRadius", _SE(js_rcConfig_get_walkableRadius), _SE(js_rcConfig_set_walkableRadius));
    js_class_rcConfig->defineProperty("maxEdgeLen", _SE(js_rcConfig_get_maxEdgeLen), _SE(js_rcConfig_set_maxEdgeLen));
    js_class_rcConfig->defineProperty("maxSimplificationError", _SE(js_rcConfig_get_maxSimplificationError), _SE(js_rcConfig_set_maxSimplificationError));
    js_class_rcConfig->defineProperty("minRegionArea", _SE(js_rcConfig_get_minRegionArea), _SE(js_rcConfig_set_minRegionArea));
    js_class_rcConfig->defineProperty("mergeRegionArea", _SE(js_rcConfig_get_mergeRegionArea), _SE(js_rcConfig_set_mergeRegionArea));
    js_class_rcConfig->defineProperty("maxVertsPerPoly", _SE(js_rcConfig_get_maxVertsPerPoly), _SE(js_rcConfig_set_maxVertsPerPoly));
    js_class_rcConfig->defineProperty("detailSampleDist", _SE(js_rcConfig_get_detailSampleDist), _SE(js_rcConfig_set_detailSampleDist));
    js_class_rcConfig->defineProperty("detailSampleMaxError", _SE(js_rcConfig_get_detailSampleMaxError), _SE(js_rcConfig_set_detailSampleMaxError));
    js_class_rcConfig->install();

    js_class_OffMeshLinkConfig = se::Class::create("OffMeshLinkConfig", ns.get(), nullptr, _SE(js_OffMeshLinkConfig_ctor));
    js_class_OffMeshLinkConfig->defineFinalizeFunction(_SE(js_RecastJsbOffMeshLinkConfig_finalize));
    js_class_OffMeshLinkConfig->defineFunction("GetInstance", _SE(js_OffMeshLinkConfig_GetInstance));
    js_class_OffMeshLinkConfig->defineStaticFunction("GetInstance", _SE(js_OffMeshLinkConfig_GetInstance));
    js_class_OffMeshLinkConfig->defineProperty("offMeshConCount", _SE(js_OffMeshLinkConfig_get_offMeshConCount), _SE(js_OffMeshLinkConfig_set_offMeshConCount));
    js_class_OffMeshLinkConfig->install();
    recastjsb_patch_offmesh_prototype(ns.get());

    js_class_dtCrowdAgentParams = se::Class::create("dtCrowdAgentParams", ns.get(), nullptr, _SE(js_dtCrowdAgentParams_ctor));
    js_class_dtCrowdAgentParams->defineFinalizeFunction(_SE(js_dtCrowdAgentParams_finalize));
    js_class_dtCrowdAgentParams->defineProperty("radius", _SE(js_dtCrowdAgentParams_get_radius), _SE(js_dtCrowdAgentParams_set_radius));
    js_class_dtCrowdAgentParams->defineProperty("height", _SE(js_dtCrowdAgentParams_get_height), _SE(js_dtCrowdAgentParams_set_height));
    js_class_dtCrowdAgentParams->defineProperty("maxAcceleration", _SE(js_dtCrowdAgentParams_get_maxAcceleration), _SE(js_dtCrowdAgentParams_set_maxAcceleration));
    js_class_dtCrowdAgentParams->defineProperty("maxSpeed", _SE(js_dtCrowdAgentParams_get_maxSpeed), _SE(js_dtCrowdAgentParams_set_maxSpeed));
    js_class_dtCrowdAgentParams->defineProperty("collisionQueryRange", _SE(js_dtCrowdAgentParams_get_collisionQueryRange), _SE(js_dtCrowdAgentParams_set_collisionQueryRange));
    js_class_dtCrowdAgentParams->defineProperty("pathOptimizationRange", _SE(js_dtCrowdAgentParams_get_pathOptimizationRange), _SE(js_dtCrowdAgentParams_set_pathOptimizationRange));
    js_class_dtCrowdAgentParams->defineProperty("separationWeight", _SE(js_dtCrowdAgentParams_get_separationWeight), _SE(js_dtCrowdAgentParams_set_separationWeight));
    js_class_dtCrowdAgentParams->defineProperty("updateFlags", _SE(js_dtCrowdAgentParams_get_updateFlags), _SE(js_dtCrowdAgentParams_set_updateFlags));
    js_class_dtCrowdAgentParams->defineProperty("obstacleAvoidanceType", _SE(js_dtCrowdAgentParams_get_obstacleAvoidanceType), _SE(js_dtCrowdAgentParams_set_obstacleAvoidanceType));
    js_class_dtCrowdAgentParams->defineProperty("queryFilterType", _SE(js_dtCrowdAgentParams_get_queryFilterType), _SE(js_dtCrowdAgentParams_set_queryFilterType));
    js_class_dtCrowdAgentParams->install();

    js_class_dtNavMesh = se::Class::create("dtNavMesh", ns.get(), nullptr, _SE(js_dtNavMesh_ctor));
    js_class_dtNavMesh->defineFinalizeFunction(_SE(js_noop_finalize));
    js_class_dtNavMesh->install();

    js_class_dtObstacleRef = se::Class::create("dtObstacleRef", ns.get(), nullptr, _SE(js_dtObstacleRef_ctor));
    js_class_dtObstacleRef->defineFinalizeFunction(_SE(js_noop_finalize));
    js_class_dtObstacleRef->install();

    js_class_NavMesh = se::Class::create("NavMesh", ns.get(), nullptr, _SE(js_NavMesh_ctor));
    js_class_NavMesh->defineFinalizeFunction(_SE(js_NavMesh_finalize));
    js_class_NavMesh->defineFunction("destroy", _SE(js_NavMesh_destroy));
    js_class_NavMesh->defineFunction("build", _SE(js_NavMesh_build));
    js_class_NavMesh->defineFunction("buildFromNavmeshData", _SE(js_NavMesh_buildFromNavmeshData));
    js_class_NavMesh->defineFunction("getNavmeshData", _SE(js_NavMesh_getNavmeshData));
    js_class_NavMesh->defineFunction("freeNavmeshData", _SE(js_NavMesh_freeNavmeshData));
    js_class_NavMesh->defineFunction("getDebugNavMesh", _SE(js_NavMesh_getDebugNavMesh));
    js_class_NavMesh->defineFunction("getClosestPoint", _SE(js_NavMesh_getClosestPoint));
    js_class_NavMesh->defineFunction("getRandomPointAround", _SE(js_NavMesh_getRandomPointAround));
    js_class_NavMesh->defineFunction("moveAlong", _SE(js_NavMesh_moveAlong));
    js_class_NavMesh->defineFunction("getNavMesh", _SE(js_NavMesh_getNavMesh));
    js_class_NavMesh->defineFunction("computePath", _SE(js_NavMesh_computePath));
    js_class_NavMesh->defineFunction("computePathSmooth", _SE(js_NavMesh_computePathSmooth));
    js_class_NavMesh->defineFunction("setDefaultQueryExtent", _SE(js_NavMesh_setDefaultQueryExtent));
    js_class_NavMesh->defineFunction("getDefaultQueryExtent", _SE(js_NavMesh_getDefaultQueryExtent));
    js_class_NavMesh->defineFunction("addCylinderObstacle", _SE(js_NavMesh_addCylinderObstacle));
    js_class_NavMesh->defineFunction("addBoxObstacle", _SE(js_NavMesh_addBoxObstacle));
    js_class_NavMesh->defineFunction("removeObstacle", _SE(js_NavMesh_removeObstacle));
    js_class_NavMesh->defineFunction("update", _SE(js_NavMesh_update));
    js_class_NavMesh->install();

    js_class_Crowd = se::Class::create("Crowd", ns.get(), nullptr, _SE(js_Crowd_ctor));
    js_class_Crowd->defineFinalizeFunction(_SE(js_Crowd_finalize));
    js_class_Crowd->defineFunction("destroy", _SE(js_Crowd_destroy));
    js_class_Crowd->defineFunction("addAgent", _SE(js_Crowd_addAgent));
    js_class_Crowd->defineFunction("removeAgent", _SE(js_Crowd_removeAgent));
    js_class_Crowd->defineFunction("update", _SE(js_Crowd_update));
    js_class_Crowd->defineFunction("getAgentPosition", _SE(js_Crowd_getAgentPosition));
    js_class_Crowd->defineFunction("getAgentVelocity", _SE(js_Crowd_getAgentVelocity));
    js_class_Crowd->defineFunction("getAgentNextTargetPath", _SE(js_Crowd_getAgentNextTargetPath));
    js_class_Crowd->defineFunction("getAgentState", _SE(js_Crowd_getAgentState));
    js_class_Crowd->defineFunction("overOffmeshConnection", _SE(js_Crowd_overOffmeshConnection));
    js_class_Crowd->defineFunction("agentGoto", _SE(js_Crowd_agentGoto));
    js_class_Crowd->defineFunction("agentStop", _SE(js_Crowd_agentStop));
    js_class_Crowd->defineFunction("agentTeleport", _SE(js_Crowd_agentTeleport));
    js_class_Crowd->defineFunction("getAgentParameters", _SE(js_Crowd_getAgentParameters));
    js_class_Crowd->defineFunction("setAgentParameters", _SE(js_Crowd_setAgentParameters));
    js_class_Crowd->defineFunction("setDefaultQueryExtent", _SE(js_Crowd_setDefaultQueryExtent));
    js_class_Crowd->defineFunction("getDefaultQueryExtent", _SE(js_Crowd_getDefaultQueryExtent));
    js_class_Crowd->defineFunction("getCorners", _SE(js_Crowd_getCorners));
    js_class_Crowd->defineFunction("getPath", _SE(js_Crowd_getPath));
    js_class_Crowd->install();

    ns->setProperty("_isJSB", se::Value(true));
    obj->setProperty("Recast", se::Value(ns.get()));
    return true;
}

#if defined(CC_PLUGIN_ENTRY)
static bool jsb_register_recastjsb_plugin(se::Object* globalThis)
{
    return register_all_recastjsb(globalThis);
}
CC_PLUGIN_ENTRY(recastjsb, jsb_register_recastjsb_plugin)
#endif
