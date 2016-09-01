#include <PT.hpp>

namespace haste {

PathTracing::PathTracing(size_t minSubpath, float roulette)
    : _minSubpath(minSubpath), _roulette(roulette) { }

void PathTracing::render(
    ImageView& view,
    RandomEngine& engine,
    size_t cameraId)
{
    auto trace = [&](RandomEngine& engine, Ray ray) -> vec3 {
        return this->trace(engine, ray);
    };

    for_each_ray(view, engine, _scene->cameras(), cameraId, trace);
}

vec3 PathTracing::trace(RandomEngine& engine, Ray ray) {
    vec3 throughput = vec3(1.0f);
    vec3 radiance = vec3(0.0f);
    float specular = 0.0f;
    int bounce = 0;

    while (true) {
        auto isect = _scene->intersect(ray.origin, ray.direction);

        while (isect.isLight()) {
            if (bounce == 0 || specular == 1.0f) {
                radiance +=
                    throughput *
                    _scene->queryRadiance(isect, -ray.direction);
            }

            ray.origin = isect.position();
            isect = _scene->intersect(ray.origin, ray.direction);
        }

        if (!isect.isPresent()) {
            break;
        }

        auto& bsdf = _scene->queryBSDF(isect);
        SurfacePoint point = _scene->querySurface(isect);

        vec3 lightSample = _scene->sampleDirectLightMixed(
            engine,
            point,
            -ray.direction,
            bsdf);

        radiance += lightSample * throughput;

        auto bsdfSample = bsdf.sample(
            engine,
            point,
            -ray.direction);

        specular = glm::max(specular, bsdfSample.specular()) * bsdfSample.specular();

        throughput *= bsdfSample.throughput() *
            abs(dot(point.normal(), bsdfSample.omega())) /
            bsdfSample.density();

        ray.direction = bsdfSample.omega();
        ray.origin = isect.position();

        float prob = bounce > _minSubpath ? _roulette : 1.0f;

        if (prob < _scene->sampler.sample()) {
            break;
        }
        else {
            throughput /= prob;
        }

        ++bounce;
    }

    return radiance;
}

string PathTracing::name() const {
    return "Path Tracing";
}

}
