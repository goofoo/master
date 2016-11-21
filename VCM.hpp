#pragma once
#include <Technique.hpp>
#include <HashGrid3D.hpp>
#include <Beta.hpp>

namespace haste {

template <class Beta> class VCMBase : public Technique, protected Beta {
public:
    VCMBase(
        size_t minSubpath,
        float roulette,
        size_t numPhotons,
        size_t numGather,
        float maxRadius,
        size_t num_threads);

    void preprocess(
        const shared<const Scene>& scene,
        RandomEngine& engine,
        const function<void(string, float)>& progress,
        bool parallel);

    string name() const override;

private:
    struct LightVertex {
        SurfacePoint surface;
        vec3 omega;
        vec3 throughput;
        float vcSpecular;
        //float vmSpecular;
        float a, A, B;
    };

    struct LightPhoton {
        SurfacePoint surface;
        vec3 omega;
        vec3 throughput;
        float vcSpecular;
        //float vmSpecular;
        float A, B;
        float fCosTheta;
        float fDensity;
        float fGeometry;

        const vec3& position() const {
            return surface.position();
        }
    };

    struct EyeVertex {
        SurfacePoint surface;
        vec3 omega;
        vec3 throughput;
        float vcSpecular;
        float c, C;
    };

    static const size_t _maxSubpath = 1024;
    const size_t _numPhotons;
    const size_t _numGather;
    const float _maxRadius;
    const size_t _minSubpath;
    const float _roulette;
    const float _eta;

    v3::HashGrid3D<LightPhoton> _vertices;

    vec3 _traceEye(render_context_t& context, const Ray& ray) override;
    void _traceLight(RandomEngine& engine, size_t& size, LightVertex* path);
    void _traceLight(RandomEngine& engine, size_t& size, LightPhoton* path);
    vec3 _connect0(RandomEngine& engine, const EyeVertex& eye);
    vec3 _connect1(RandomEngine& engine, const EyeVertex& eye);
    vec3 _connect(const EyeVertex& eye, const LightVertex& light);

    vec3 _connect(
        RandomEngine& engine,
        const EyeVertex& eye,
        size_t size,
        const LightVertex* path);

    void _scatter(RandomEngine& engine);

    vec3 _gather(
        RandomEngine& engine,
        const EyeVertex& eye);

    vec3 _merge(
        const EyeVertex& eye,
        const LightPhoton& light,
        float radius);
};

typedef VCMBase<FixedBeta<0>> VCM0;
typedef VCMBase<FixedBeta<1>> VCM1;
typedef VCMBase<FixedBeta<2>> VCM2;

class VCMb : public VCMBase<VariableBeta> {
public:
    VCMb(
        size_t minSubpath,
        float roulette,
        size_t numPhotons,
        size_t numGather,
        float maxRadius,
        float beta,
        size_t num_threads);
};

}
