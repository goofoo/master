#include <DiffuseBSDF.hpp>
#include <SurfacePoint.hpp>

namespace haste {

DiffuseBSDF::DiffuseBSDF(const vec3& diffuse)
    : _diffuse(diffuse) {
}

const BSDFQuery DiffuseBSDF::query(
    const vec3& incident,
    const vec3& outgoing) const
{
    BSDFQuery query;

    query.throughput = incident.y > 0.0f && outgoing.y > 0.0f
        ? _diffuse * one_over_pi<float>()
        : vec3(0.0f);

    query.density = outgoing.y > 0.0f
        ? outgoing.y * one_over_pi<float>()
        : 0.0f;

    query.densityRev = incident.y > 0.0f
        ? incident.y * one_over_pi<float>()
        : 0.0f;

    return query;
}

const BSDFSample DiffuseBSDF::sample(
    RandomEngine& engine,
    const vec3& omega) const
{
    if (omega.y < 0.0f)
    {
        BSDFSample sample;
        sample._throughput = vec3(0.0f);
        sample._omega = -omega;
        sample._density = 1.0f;
        sample._densityRev = 1.0f;
        sample._specular = 0.0f;

        return sample;
    }
    else
    {
        auto hemisphere = sampleCosineHemisphere1(engine);

        BSDFSample sample;
        sample._throughput = _diffuse * one_over_pi<float>();
        sample._omega = hemisphere.omega();
        sample._density = hemisphere.density();
        sample._densityRev = abs(omega.y * one_over_pi<float>());
        sample._specular = 0.0f;

        return sample;
    }
}

const BSDFBoundedSample DiffuseBSDF::sampleBounded(
    RandomEngine& engine,
    const vec3& omega,
    const angular_bound_t& bound) const {
    auto distribution = lambertian_bounded_distribution_t(bound);

    BSDFBoundedSample result;
    result._omega = distribution.sample(engine);
    result._area = distribution.subarea();

    return result;
}

BSDFSample DiffuseBSDF::scatter(
    RandomEngine& engine,
    const SurfacePoint& point,
    const vec3& omega) const
{
    const float inv3 = 1.0f / 3.0f;

    float diffuseAvg = (_diffuse.x + _diffuse.y + _diffuse.z) * inv3;

    if (sampleUniform1(engine).value() < diffuseAvg) {
        auto hemisphere = sampleCosineHemisphere1(engine);

        BSDFSample sample;
        sample._throughput = _diffuse / diffuseAvg;
        sample._omega = point.toWorld(hemisphere.omega());
        sample._density = hemisphere.density();
        sample._densityRev = dot(point.normal(), omega) * one_over_pi<float>();
        sample._specular = 0.0f;

        return sample;
    }
    else {
        BSDFSample sample;
        sample._throughput = vec3(0.0f);
        sample._omega = vec3(0.0f);
        sample._density = 0;
        sample._densityRev = 0;
        sample._specular = 0.0f;

        return sample;
    }
}

}
