#include <iostream>
#include <VCM.hpp>
#include <Edge.hpp>

namespace haste {

VCM::VCM(
    size_t numPhotons,
    size_t numGather,
    float maxRadius,
    size_t minSubpath,
    float roulette)
    : _numPhotons(numPhotons)
    , _numGather(numGather)
    , _maxRadius(maxRadius)
    , _minSubpath(minSubpath)
    , _roulette(roulette)
    , _eta(_numPhotons * pi<float>() * _maxRadius * _maxRadius)
{ }

void VCM::preprocess(
    const shared<const Scene>& scene,
    RandomEngine& engine,
    const function<void(string, float)>& progress,
    bool parallel)
{
    Technique::preprocess(scene, engine, progress, parallel);

    _scatter(engine);
}

string VCM::name() const {
    return "Vertex Connection and Merging";
}

vec3 VCM::_trace(RandomEngine& engine, const Ray& ray) {
    LightVertex light[_maxSubpath];
    size_t lSize = 0;

    _trace(engine, lSize, light);

    vec3 radiance = vec3(0.0f);
    EyeVertex eye[2];
    size_t itr = 0, prv = 1;

    RayIsect isect = _scene->intersect(ray.origin, ray.direction);

    while (isect.isLight()) {
        radiance += _scene->queryRadiance(isect);
        isect = _scene->intersect(isect.position(), ray.direction);
    }

    if (!isect.isPresent()) {
        return radiance;
    }

    eye[itr].surface = _scene->querySurface(isect);
    eye[itr]._omega = -ray.direction;
    eye[itr].throughput = vec3(1.0f);
    eye[itr].specular = 1.0f;
    eye[itr].c = -_eta;
    eye[itr].C = 0;

    size_t eSize = 2;

    radiance += _connect(engine, eSize, eye[itr], lSize, light);
    radiance += _gather0(engine, ray.origin, eye[itr]);
    std::swap(itr, prv);

    float roulette = eSize < _minSubpath ? 1.0f : _roulette;
    float uniform = sampleUniform1(engine).value();

    while (uniform < roulette) {
        auto bsdf = _scene->sampleBSDF(engine, eye[prv].surface, eye[prv].omega());

        isect = _scene->intersectMesh(eye[prv].position(), bsdf.omega());

        if (!isect.isPresent()) {
            return radiance;
        }

        eye[itr].surface = _scene->querySurface(isect);
        eye[itr]._omega = -bsdf.omega();

        auto edge = Edge(eye[prv], eye[itr]);

        eye[itr].throughput =
            eye[prv].throughput *
            bsdf.throughput() *
            edge.bCosTheta /
            (bsdf.density() * roulette);

        eye[itr].specular = eye[prv].specular * bsdf.specular();
        eye[itr].c = 1.0f / (edge.fGeometry * bsdf.density());

        if (eSize == 2) {
            eye[itr].C = 0.0f;
        }
        else {
            eye[itr].C =
                (eye[prv].C * bsdf.densityRev() + eye[prv].c + _eta) *
                edge.bGeometry *
                eye[itr].c;
        }

        ++eSize;
        radiance += _connect(engine, eSize, eye[itr], lSize, light);
        radiance += _gather(engine, eye[prv], bsdf.query(), eye[itr]);
        std::swap(itr, prv);

        roulette = eSize < _minSubpath ? 1.0f : _roulette;
        uniform = sampleUniform1(engine).value();
    }

    return radiance;
}

void VCM::_trace(RandomEngine& engine, size_t& size, LightVertex* path) {
    size_t itr = 0, prv = 0;

    LightSampleEx light = _scene->sampleLight(engine);

    RayIsect isect = _scene->intersectMesh(light.position(), light.omega());

    if (!isect.isPresent()) {
        size = 0;
        return;
    }

    auto edge = Edge(light, isect);

    path[itr].surface = _scene->querySurface(isect);
    path[itr]._omega = -light.omega();
    path[itr].throughput = light.radiance() * edge.bCosTheta / light.density();
    path[itr].a = 1.0f / (edge.fGeometry * light.omegaDensity());
    path[itr].A = edge.bGeometry * path[itr].a / light.areaDensity();
    path[itr].B = 0.f;

    prv = itr;
    ++itr;

    size_t lSize = 2;
    float roulette = lSize < _minSubpath ? 1.0f : _roulette;
    float uniform = sampleUniform1(engine).value();

    while (uniform < roulette) {
        auto bsdf = _scene->sampleBSDF(engine, path[prv].surface, path[prv].omega());

        isect = _scene->intersectMesh(path[prv].position(), bsdf.omega());

        if (!isect.isPresent()) {
            break;
        }

        path[itr].surface = _scene->querySurface(isect);
        path[itr]._omega = -bsdf.omega();

        edge = Edge(path[prv], path[itr]);

        path[itr].throughput =
            path[prv].throughput *
            bsdf.throughput() *
            edge.bCosTheta /
            (bsdf.density() * roulette);

        path[itr].a = 1.0f / (edge.fGeometry * bsdf.density());
        path[itr].A = (path[prv].A * bsdf.densityRev() + path[prv].a) * edge.bGeometry * path[itr].a;
        path[itr].B = (path[prv].B * bsdf.densityRev() + _eta) * edge.bGeometry * path[itr].a;

        if (bsdf.specular() > 0.0f) {
            path[prv] = path[itr];
        }
        else {
            prv = itr;
            ++itr;
        }

        ++lSize;
        roulette = lSize < _minSubpath ? 1.0f : _roulette;
        uniform = sampleUniform1(engine).value();
    }

    auto bsdf = _scene->sampleBSDF(engine, path[prv].surface, path[prv].omega());

    if (bsdf.specular() > 0.0f) {
        size = prv;
    }
    else {
        size = prv + 1;
    }
}

vec3 VCM::_connect(const EyeVertex& eye, const LightVertex& light) {
    vec3 omega = normalize(eye.position() - light.position());

    auto lightBSDF = _scene->queryBSDFEx(light.surface, light.omega(), omega);
    auto eyeBSDF = _scene->queryBSDFEx(eye.surface, -omega, eye.omega());

    auto edge = Edge(light, eye, omega);

    float Ap = (light.A * lightBSDF.densityRev() + light.a) * edge.bGeometry * eyeBSDF.densityRev();
    float Bp = light.B * lightBSDF.densityRev() * edge.bGeometry * eyeBSDF.densityRev();
    float Cp = (eye.C * eyeBSDF.density() + eye.c + _eta) * edge.fGeometry * lightBSDF.density();

    float weightInv = Ap + Bp + Cp + _eta * edge.bGeometry * eyeBSDF.densityRev() + 1.0f;

    return
        _scene->occluded(eye.position(), light.position()) *
        light.throughput *
        lightBSDF.throughput() *
        eye.throughput *
        eyeBSDF.throughput() *
        edge.bCosTheta *
        edge.fGeometry /
        weightInv;
}

vec3 VCM::_connect0(RandomEngine& engine, size_t eyeSize, const EyeVertex& eye) {
    vec3 radiance = vec3(0.0f);

    auto bsdf = _scene->sampleBSDF(engine, eye.surface, eye.omega());
    RayIsect isect = _scene->intersect(eye.position(), bsdf.omega());

    while (isect.isLight())
    {
        if (eye.specular * bsdf.specular() > 0.0f)
        {
            radiance +=
                _scene->queryRadiance(isect, -bsdf.omega()) *
                eye.throughput *
                bsdf.throughput() *
                abs(dot(eye.gnormal(), bsdf.omega())) /
                (bsdf.density());
        }
        else
        {
            auto lsdf = _scene->queryLSDF(isect, -bsdf.omega());
            auto edge = Edge(eye, isect, bsdf.omega());

            float weightInv = 0.f;

            if (eyeSize == 2) {
                weightInv = lsdf.areaDensity() / (edge.fGeometry * bsdf.density()) + 1.0f;
            }
            else {
                float c = 1.0f / (edge.fGeometry * bsdf.density());
                float C = (eye.C * bsdf.densityRev() + eye.c + _eta) * edge.bGeometry * c;
                float Cp = (C * lsdf.omegaDensity() + c + _eta) * lsdf.areaDensity();

                weightInv = Cp + 1;
            }

            radiance +=
                lsdf.radiance() *
                eye.throughput *
                bsdf.throughput() *
                edge.bCosTheta /
                (bsdf.density() * weightInv);
        }

        isect = _scene->intersect(isect.position(), bsdf.omega());
    }

    return radiance;
}

vec3 VCM::_connect1(RandomEngine& engine, size_t eyeSize, const EyeVertex& eye) {
    LightSampleEx light = _scene->sampleLightEx(engine, eye.position());

    auto bsdf = _scene->queryBSDFEx(eye.surface, -light.omega(), eye.omega());
    auto edge = Edge(light, eye, light.omega());

    float weightInv = 0.f;

    if (eyeSize == 2) {
        weightInv = (bsdf.densityRev() * edge.bGeometry) / light.areaDensity() + 1.0f;
    }
    else {
        float Ap = bsdf.densityRev() * edge.bGeometry / light.areaDensity();
        float Bp = 0.0f;
        float Cp = (eye.C * bsdf.density() + eye.c + _eta) * edge.fGeometry * light.omegaDensity();

        weightInv = Ap + Bp + Cp + 1.0f;
    }

    return
        light.radiance() *
        eye.throughput *
        bsdf.throughput() *
        edge.bCosTheta *
        edge.fGeometry /
        (light.areaDensity() * weightInv);
}

vec3 VCM::_connect(
    RandomEngine& engine,
    size_t eyeSize,
    const EyeVertex& eye,
    size_t lightSize,
    const LightVertex* path)
{
    vec3 radiance = _connect0(engine, eyeSize, eye) + _connect1(engine, eyeSize, eye);

    for (size_t i = 0; i < lightSize; ++i) {
        radiance += _connect(eye, path[i]);
    }

    return radiance;
}

void VCM::_scatter(RandomEngine& engine)
{
    vector<LightVertex> vertices;

    size_t itr = 0;

    for (size_t i = 0; i < _numPhotons; ++i) {
        vertices.resize(itr + _maxSubpath);
        size_t size = 0;
        _trace(engine, size, vertices.data() + itr);
        itr += size;
    }

    vertices.resize(itr);

    _vertices = KDTree3D<LightVertex>(move(vertices));
}

vec3 VCM::_gather0(
    RandomEngine& engine,
    const vec3& position,
    const EyeVertex& tentative)
{
    EyeVertex eye;
    eye.surface._position = position;
    eye.surface._tangent[0] = -tentative.omega();
    eye.surface._tangent[1] = -tentative.omega();
    eye.surface._tangent[2] = -tentative.omega();
    eye._omega = -tentative.omega();
    eye.throughput = vec3(1.0f);
    eye.c = 0.0f;
    eye.C = 0.0f;

    BSDFQuery query;
    query._throughput = vec3(1.0f);
    query._density = 1.0f;
    query._densityRev = 1.0f;

    return _gather(engine, eye, query, tentative);
}

vec3 VCM::_gather(
    RandomEngine& engine,
    const EyeVertex& eye,
    const BSDFQuery& eyeBSDF,
    const EyeVertex& tentative)
{
    LightVertex light[_maxSubpath];

    size_t gathered = _vertices.query_k(
        light,
        tentative.position(),
        _numGather,
        _maxRadius);

    vec3 radiance = vec3(0.0f);

    for (size_t i = 0; i < gathered; ++i) {
        radiance += _merge(eye, eyeBSDF, light[i], _maxRadius);
    }

    return radiance / float(_numPhotons);
}

vec3 VCM::_merge(
    const EyeVertex& eye,
    const BSDFQuery& eyeBSDF,
    const LightVertex& light,
    float radius)
{
    vec3 omega = normalize(eye.position() - light.position());

    auto lightBSDF = _scene->queryBSDFEx(light.surface, light.omega(), omega);
    /* auto eyeBSDF = _scene->queryBSDFEx(eye.surface, -omega, eye.omega()); */

    auto edge = Edge(light, eye, omega);

    float Ap = (light.A * lightBSDF.densityRev() + light.a) * edge.bGeometry * eyeBSDF.densityRev();
    float Bp = light.B * lightBSDF.densityRev() * edge.bGeometry * eyeBSDF.densityRev();
    float Cp = (eye.C * eyeBSDF.density() + eye.c + _eta) * edge.fGeometry * lightBSDF.density();

    float weightInv = Ap + Bp + Cp + _eta * edge.bGeometry * eyeBSDF.densityRev() + 1.0f;
    weightInv /= _eta;

    return
        _scene->occluded(eye.position(), light.position()) *
        light.throughput *
        lightBSDF.throughput() *
        eye.throughput *
        eyeBSDF.throughput() *
        edge.bCosTheta *
        edge.fGeometry /
        (weightInv * pi<float>() * radius * radius);
}

}
