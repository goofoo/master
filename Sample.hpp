#pragma once
#include <random>
#include <glm>

namespace haste {

struct RandomEngine {
public:
    RandomEngine();
    RandomEngine(RandomEngine&& that);

    float random1();
    vec2 random2();
    vec3 random3();
    vec4 random4();

private:
    std::minstd_rand engine;
    RandomEngine(const RandomEngine&) = delete;
    RandomEngine& operator=(const RandomEngine&) = delete;
};

struct UniformSample1 {
    float _value;

    const float a() const { return _value; }
    const float value() const { return _value; }
    const float density() const { return 1.0f; }
    const float densityInv() const { return 1.0f; }
};

struct UniformSample2 {
    vec2 _value;

    const float a() const { return _value.x; }
    const float b() const { return _value.y; }
    const vec2& value() const { return _value; }
    const float density() const { return 1.0f; }
    const float densityInv() const { return 1.0f; }
};

struct DiskSample1 {
    vec2 _point;

    const float theta() const;
    const float radius() const;
    const vec2 point() const { return _point; }
    const float density() const { return one_over_pi<float>(); }
    const float densityInv() const { return pi<float>(); }
};

struct HemisphereSample1 {
    vec3 _omega;

    const vec3 omega() const { return _omega; }
    const float density() const { return one_over_pi<float>() * 0.5f; }
    const float densityInv() const { return 2.0f * pi<float>(); }
};

struct CosineHemisphereSample1 {
    vec3 _omega;

    const vec3 omega() const { return _omega; }
    const float density() const { return _omega.y * one_over_pi<float>(); }
    const float densityInv() const { return 1.0f / _omega.y * pi<float>(); }
};

UniformSample1 sampleUniform1(RandomEngine& source);
UniformSample2 sampleUniform2(RandomEngine& source);
DiskSample1 sampleDisk1(RandomEngine& source);
HemisphereSample1 sampleHemisphere1(RandomEngine& source);
CosineHemisphereSample1 sampleCosineHemisphere1(RandomEngine& source);

}
