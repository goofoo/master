#include <runtime_assert>
#include <Scene.hpp>
#include <streamops.hpp>
#include <cstring>

namespace haste {

Scene::Scene(
    Cameras&& cameras,
    Materials&& materials,
    vector<Mesh>&& meshes,
    AreaLights&& areaLights,
    const bounding_sphere_t& bounding_sphere)
    : _cameras(cameras)
    , meshes(move(meshes))
    , lights(move(areaLights))
    , materials(move(materials))
    , _bounding_sphere(bounding_sphere)
{
    rtcScene = nullptr;

    _numIntersectRays = 0;
    _numOccludedRays = 0;
}

unsigned makeRTCMesh(RTCScene rtcScene, size_t i, const vector<Mesh>& meshes) {
    unsigned geomID = rtcNewTriangleMesh(
        rtcScene,
        RTC_GEOMETRY_STATIC,
        meshes[i].indices.size() / 3,
        meshes[i].vertices.size(),
        1);

    vec4* vbuffer = (vec4*) rtcMapBuffer(rtcScene, geomID, RTC_VERTEX_BUFFER);

    for (size_t j = 0; j < meshes[i].vertices.size(); ++j) {
        vbuffer[j].x = meshes[i].vertices[j].x;
        vbuffer[j].y = meshes[i].vertices[j].y;
        vbuffer[j].z = meshes[i].vertices[j].z;
        vbuffer[j].w = 1;
    }

    rtcUnmapBuffer(rtcScene, geomID, RTC_VERTEX_BUFFER);

    int* triangles = (int*) rtcMapBuffer(rtcScene, geomID, RTC_INDEX_BUFFER);
    std::memcpy(
        triangles,
        meshes[i].indices.data(),
        meshes[i].indices.size() * sizeof(int));

    rtcUnmapBuffer(rtcScene, geomID, RTC_INDEX_BUFFER);

    return geomID;
}

void updateRTCScene(RTCScene& rtcScene, RTCDevice device, const Scene& scene) {
    if (rtcScene) {
        rtcDeleteScene(rtcScene);
    }

    rtcScene = rtcDeviceNewScene(
        device,
        RTC_SCENE_STATIC | RTC_SCENE_HIGH_QUALITY,
        RTC_INTERSECT1);

    if (rtcScene == nullptr) {
        throw std::runtime_error("Cannot create RTCScene.");
    }

    unsigned geomID = newMesh(rtcScene, scene.lights);
    runtime_assert(geomID == 0, "Area lights have to get 0 primID.");

    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        unsigned geomID = makeRTCMesh(rtcScene, i, scene.meshes);
        runtime_assert(geomID == i + 1, "Geometry ID doesn't correspond to mesh index.");
    }

    rtcCommit(rtcScene);
}

void Scene::buildAccelStructs(RTCDevice device) {
    if (rtcScene == nullptr) {
        updateRTCScene(rtcScene, device, *this);
        lights.init(this, _bounding_sphere);
    }
}

const BSDF& Scene::queryBSDF(const SurfacePoint& surface) const {
    runtime_assert(surface.materialId() + materials.lights_offset < int32_t(materials.bsdfs.size()));
    return *materials.bsdfs[surface.materialId() + materials.lights_offset].get();
}

SurfacePoint Scene::querySurface(const RayIsect& isect) const {
    if (!isect.isPresent()) {
        SurfacePoint surface;
        surface._materialId = INT32_MIN;
        return surface;
    }
    else if (isect.isLight()) {
        SurfacePoint point;

        point._position = (vec3&)isect.org + (vec3&)isect.dir * isect.tfar;
        point._tangent = lights.light_to_world_mat3(isect.primId());
        point._materialId = int32_t(isect.primId()) - materials.lights_offset;
        point.gnormal = point._tangent[1];

        return point;
    }
    else {
        runtime_assert(isect.meshId() < meshes.size());

        const float w = 1.f - isect.u - isect.v;
        auto& mesh = meshes[isect.meshId()];

        const mat3& t0 = mesh.tangents[mesh.indices[isect.primID * 3 + 0]];
        const mat3& t1 = mesh.tangents[mesh.indices[isect.primID * 3 + 1]];
        const mat3& t2 = mesh.tangents[mesh.indices[isect.primID * 3 + 2]];

        SurfacePoint point;
        point._position = (vec3&)isect.org + (vec3&)isect.dir * isect.tfar;

        point._tangent = w * t0 + isect.u * t1 + isect.v * t2;

        point._tangent[1] = normalize(point._tangent[1]);

        point._tangent[0]
            = point._tangent[0]
            - dot(point._tangent[0], point._tangent[1]) * point._tangent[1];

        point._tangent[0] = normalize(point._tangent[0]);

        point._tangent[2]
            = point._tangent[2]
            - dot(point._tangent[2], point._tangent[1]) * point._tangent[1]
            - dot(point._tangent[2], point._tangent[0]) * point._tangent[0];

        point._tangent[2] = normalize(point._tangent[2]);

        point.gnormal = isect.gnormal();

        point._tangent[1]
            = point._tangent[1]
            * (dot(isect.omega(), point._tangent[1]) < 0.0f ? -1.0f : 1.0f);

        point.gnormal
            = point.gnormal
            * (dot(isect.omega(), point.gnormal) < 0.0f ? -1.0f : 1.0f);

        point._materialId = mesh.materialID;

        return point;
    }
}

vec3 Scene::queryRadiance(
    const SurfacePoint& surface,
    const vec3& direction) const {
    return lights.queryRadiance(
        _material_id_to_light_id(surface.materialId()), direction);
}

const LSDFQuery Scene::queryLSDF(
    const SurfacePoint& surface,
    const vec3& omega) const
{
    runtime_assert(surface.materialId() < 0);
    return lights.queryLSDF(surface.materialId() + materials.lights_offset, omega);
}

const BSDFSample Scene::sampleBSDF(
    RandomEngine& engine,
    const SurfacePoint& surface,
    const vec3& omega) const
{
    runtime_assert(surface.materialId() + materials.lights_offset < int32_t(materials.bsdfs.size()));

    auto bsdf = materials.bsdfs[surface.materialId() + materials.lights_offset].get();
    return bsdf->sample(engine, surface, omega);
}

const BSDFQuery Scene::queryBSDF(
    const SurfacePoint& surface,
    const vec3& incident,
    const vec3& outgoing) const
{
    runtime_assert(surface.materialId() + materials.lights_offset < int32_t(materials.bsdfs.size()));

    auto bsdf = materials.bsdfs[surface.materialId() + materials.lights_offset].get();
    return bsdf->query(surface, incident, outgoing);
}

float Scene::occluded(
    const SurfacePoint& origin,
    const SurfacePoint& target) const
{
    vec3 adjusted_origin = origin.position() + origin.normal() * 0.001f;
    vec3 adjusted_target = target.position() + target.normal() * 0.001f;

    RTCRay rtcRay;
    (*(vec3*)rtcRay.org) = adjusted_origin;
    (*(vec3*)rtcRay.dir) = adjusted_target - adjusted_origin;
    rtcRay.tnear = 0.0f;
    rtcRay.tfar =  1.0f;
    rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.mask = RayIsect::occluderMask();
    rtcRay.time = 0.f;
    rtcOccluded(rtcScene, rtcRay);

    ++_numOccludedRays;

    return rtcRay.geomID == 0 ? 0.f : 1.f;
}

SurfacePoint Scene::intersect(
    const SurfacePoint& surface,
    vec3 direction,
    float tfar) const {
    RayIsect rtcRay;
    (*(vec3*)rtcRay.org) = surface.position();
    (*(vec3*)rtcRay.dir) = direction;
    rtcRay.tnear = 0.0005f;
    rtcRay.tfar = tfar;
    rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.mask = 0xFFFFFFFF;
    rtcRay.time = 0.f;
    rtcIntersect(rtcScene, rtcRay);

    ++_numIntersectRays;

    return querySurface(rtcRay);
}

const size_t Scene::numNormalRays() const {
    return _numIntersectRays;
}

const size_t Scene::numShadowRays() const {
    return _numOccludedRays;
}

const size_t Scene::numRays() const {
    return _numIntersectRays + _numOccludedRays;
}

const LightSample Scene::sampleLight(
        RandomEngine& engine) const
{
    return lights.sample(engine);
}

int32_t Scene::_material_id_to_light_id(int32_t id) const {
    return id + materials.lights_offset;
}

int32_t Scene::_light_id_to_material_id(int32_t id) const {
    return id - materials.lights_offset;
}

}
