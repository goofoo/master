#pragma once
#include <cmath>
#include <ctgmath>
#include <runtime_assert>
#include <Prerequisites.hpp>
#include <ImageView.hpp>
#include <Scene.hpp>
#include <threadpool.hpp>
#include <mutex>

namespace haste {

struct render_context_t {
    mat3 view_to_world_mat3;
    mat3 world_to_view_mat3;
    vec3 camera_position;
    vec2 resolution;
    float resolution_y_inv;
    float focal_length_y;
    float focal_factor_y;
    int32_t camera_id;

    RandomEngine* generator;
    vec2 pixel_position;
};

class Technique {
public:
    Technique(const shared<const Scene>& scene, size_t num_threads);
    virtual ~Technique();

    virtual double render(
        ImageView& view,
        RandomEngine& engine,
        size_t cameraId);

    virtual string name() const = 0;

    const metadata_t& metadata() const;
    double frame_time() const;
protected:
    double _previous_frame_time = NAN;
    double _rendering_start_time = NAN;
    double _frame_time = NAN;
    metadata_t _metadata;
    shared<const Scene> _scene;
    std::vector<dvec3> _eye_image;
    std::vector<dvec3> _light_image;
    std::mutex _light_mutex;

    threadpool_t _threadpool;

    virtual vec3 _traceEye(render_context_t& context, Ray ray);
    virtual void _preprocess(RandomEngine& engine, double num_samples);
    static SurfacePoint _camera_surface(render_context_t& context);
    static vec3 _camera_direction(render_context_t& context);

    void _adjust_helper_image(ImageView& view);
    void _trace_paths(ImageView& view, render_context_t& context, size_t cameraId);
    double _commit_images(ImageView& view);

    template <class F>
    vec3 _accumulate(
        render_context_t& context,
        vec3 direction,
        F&& callback) {
        return _accumulate(
            context,
            direction,
            &callback,
            [](void* closure) -> vec3 { return (*(F*)closure)(); });
    }

    vec3 _accumulate(
        render_context_t& context,
        vec3 direction,
        void* closure,
        vec3 (*)(void*));

    void _for_each_ray(
        ImageView& view,
        render_context_t& context);

private:
    Technique(const Technique&) = delete;
    Technique& operator=(const Technique&) = delete;
};

}
