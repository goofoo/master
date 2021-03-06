#pragma once
#include <fixed_vector.hpp>
#include <Technique.hpp>
#include <Beta.hpp>

namespace haste {

template <class Beta> class BPTBase : public Technique, protected Beta {
public:
    BPTBase(const shared<const Scene>& scene, float lights, float roulette, float beta, size_t num_threads);

    string name() const override;

private:
    struct LightVertex {
        SurfacePoint surface;
        vec3 omega;
        vec3 throughput;
        float specular;
        float a, A;
    };

    struct EyeVertex {
        SurfacePoint surface;
        vec3 omega;
        vec3 throughput;
        float specular;
        float c, C;
    };

    static const size_t _maxSubpath = 1024;
    using light_path_t = fixed_vector<LightVertex, _maxSubpath>;
    const float _roulette;
    const float _lights;

    vec3 _traceEye(render_context_t& context, Ray ray) override;
    void _traceLight(random_generator_t& generator, light_path_t& path);
    vec3 _connect(const EyeVertex& eye, const LightVertex& light);

    vec3 _connect_light(const EyeVertex& eye);
    vec3 _connect(const EyeVertex& eye, const light_path_t& path);
    vec3 _connect_eye(render_context_t& context, const EyeVertex& eye, const light_path_t& path);

    bool _russian_roulette(random_generator_t& generator) const;
};

typedef BPTBase<FixedBeta<0>> BPT0;
typedef BPTBase<FixedBeta<1>> BPT1;
typedef BPTBase<FixedBeta<2>> BPT2;

class BPTb : public BPTBase<VariableBeta> {
public:
    BPTb(const shared<const Scene>& scene, float lights, float roulette, float beta, size_t num_threads);
};

}
