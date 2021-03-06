#include <unittest>
#include <runtime_assert>
#include <Technique.hpp>

namespace haste {

Technique::Technique(const shared<const Scene>& scene, size_t num_threads)
    : _scene(scene)
    , _threadpool(num_threads) {
}

Technique::~Technique() { }

double Technique::render(
    ImageView& view,
    RandomEngine& engine,
    size_t cameraId)
{
    auto& cameras = _scene->cameras();

    render_context_t context;
    context.view_to_world_mat3 = cameras.view_to_world_mat3(cameraId);
    context.world_to_view_mat3 = cameras.world_to_view_mat3(cameraId);
    context.camera_position = cameras.position(cameraId);
    context.resolution = vec2(view.width(), view.height());
    context.resolution_y_inv = 1.0f / context.resolution.y;
    context.focal_length_y = cameras.focal_length_y(cameraId, context.resolution.x / context.resolution.y);
    context.focal_factor_y = context.focal_length_y * context.focal_length_y * 0.25f;
    context.generator = &engine;

    if (!std::isfinite(_rendering_start_time)) {
        _rendering_start_time = high_resolution_time();
        _previous_frame_time = high_resolution_time();
    }

    size_t num_basic_rays = _scene->numNormalRays();
    size_t num_shadow_rays = _scene->numShadowRays();

    _adjust_helper_image(view);
    _preprocess(engine, _metadata.num_samples);
    _trace_paths(view, context, cameraId);
    double epsilon = _commit_images(view);

    double current = high_resolution_time();
    _frame_time = current - _previous_frame_time;
    _previous_frame_time = current;

    _metadata.technique = name();
    ++_metadata.num_samples;
    _metadata.num_basic_rays += _scene->numNormalRays() - num_basic_rays;
    _metadata.num_shadow_rays += _scene->numShadowRays() - num_shadow_rays;
    _metadata.num_tentative_rays += 0;

    _metadata.num_threads = _threadpool.num_threads();
    _metadata.resolution = ivec2(view.width(), view.height());
    _metadata.epsilon = epsilon;
    _metadata.total_time = current - _rendering_start_time;
    _metadata.average = glm::vec3(0.0f, 0.0f, 0.0f);

    return epsilon;
}

const metadata_t& Technique::metadata() const {
    return _metadata;
}

double Technique::frame_time() const {
    return _frame_time;
}

vec3 Technique::_traceEye(
    render_context_t& context,
    Ray ray)
{
    return vec3(1.0f, 0.0f, 1.0f);
}

void Technique::_preprocess(RandomEngine& engine, double num_samples) {

}

SurfacePoint Technique::_camera_surface(render_context_t& context) {
    SurfacePoint result;
    result._position = context.camera_position;
    result._tangent[0] = context.view_to_world_mat3[1];
    result._tangent[1] = -context.view_to_world_mat3[2];
    result._tangent[2] = context.view_to_world_mat3[0];
    result._materialId = -1;
    result.gnormal = -context.view_to_world_mat3[2];
    return result;
}

unittest() {
    vec3 position = vec3(1.f, 3.f, 2.f);
    vec3 direction = vec3(1.0f, 0.0f, 10.0f);
    vec3 up = vec3(0.0f, 1.0f, 0.0f);

    mat3 view_to_world = inverse(glm::lookAt(position, position + direction, up));

    vec3 direction_normalized = normalize(direction);
    vec3 tangent_normalized = normalize(cross(direction, up));

    mat3 tangent;
    tangent[0] = view_to_world[1];
    tangent[1] = view_to_world[2];
    tangent[2] = view_to_world[0];

    assert_almost_eq(tangent[0], vec3(0.0f, 1.0f, 0.0f));
    assert_almost_eq(tangent[1], -direction_normalized);
    assert_almost_eq(tangent[2], tangent_normalized);
}

vec3 Technique::_camera_direction(render_context_t& context) {
    return context.world_to_view_mat3[2];
}

unittest() {
    vec3 position = vec3(1.f, 1.f, 1.f);
    vec3 direction = vec3(1.0f, 2.0f, 3.0f);
    vec3 up = vec3(0.0f, 1.0f, 0.0f);

    mat3 view_to_world = inverse(glm::lookAt(position, position + direction, up));

    vec3 direction_normalized = normalize(direction);

    assert_almost_eq(view_to_world[2], -direction_normalized);
}

void Technique::_adjust_helper_image(ImageView& view) {
    size_t view_size = view.width() * view.height();

    if (_light_image.size() != view_size) {
        _light_image.resize(view_size, vec3(0.0f));
        _eye_image.resize(view_size, vec3(0.0f));
    }
}

void Technique::_trace_paths(
    ImageView& view,
    render_context_t& context,
    size_t cameraId) {
    exec2d(_threadpool, view.xWindow(), view.yWindow(), 32,
        [&](size_t x0, size_t x1, size_t y0, size_t y1) {
        render_context_t local_context = context;
        RandomEngine engine;
        local_context.generator = &engine;

        ImageView subview = view;

        size_t xBegin = view._xOffset + x0;
        size_t xEnd = view._xOffset + x1;

        size_t yBegin = view._yOffset + y0;
        size_t yEnd = view._yOffset + y1;

        subview._xOffset = xBegin;
        subview._xWindow = xEnd - xBegin;

        subview._yOffset = yBegin;
        subview._yWindow = yEnd - yBegin;

        _for_each_ray(subview, local_context);
    });
}

double Technique::_commit_images(ImageView& view) {
    double epsilon = 0.0f;

    exec_in_bands(_threadpool, view.xWindow(), view.yWindow(), 128,
        [&](size_t x0, size_t x1, size_t y0, size_t y1) {
        ImageView subview = view;

        size_t xBegin = view._xOffset + x0;
        size_t xEnd = view._xOffset + x1;

        size_t yBegin = view._yOffset + y0;
        size_t yEnd = view._yOffset + y1;

        subview._xOffset = xBegin;
        subview._xWindow = xEnd - xBegin;

        subview._yOffset = yBegin;
        subview._yWindow = yEnd - yBegin;

        double local_epsilon = 0.0f;

        for (size_t y = subview.yBegin(); y < subview.yEnd(); ++y) {
            dvec4* dst_begin = subview.data() + y * subview.width() + subview.xBegin();
            dvec4* dst_end = dst_begin + subview.xWindow();
            dvec3* light_itr = _light_image.data() + y * subview.width() + subview.xBegin();
            dvec3* eye_itr = _eye_image.data() + y * subview.width() + subview.xBegin();

            for (dvec4* dst_itr = dst_begin; dst_itr < dst_end; ++dst_itr) {
                dvec4 new_dst = *dst_itr + dvec4(*light_itr + *eye_itr, 1.0f);

                dvec3 delta = new_dst.rgb() / new_dst.a - dst_itr->rgb() / dst_itr->a;
                local_epsilon += l1Norm(delta * delta);

                *dst_itr = new_dst;

                *light_itr = dvec3(0.0f);
                *eye_itr = dvec3(0.0f);
                ++light_itr;
                ++eye_itr;
            }
        }

        std::unique_lock<std::mutex> lock(_light_mutex);
        epsilon += local_epsilon;
    });

    return sqrt(epsilon / (view.width() * view.height()));
}

vec3 Technique::_accumulate(
        render_context_t& context,
        vec3 direction,
        void* closure,
        vec3 (*callback)(void*)) {
    vec3 view_direction = context.world_to_view_mat3 * direction;

    vec2 position = pixel_position(
        view_direction,
        context.resolution,
        context.resolution_y_inv,
        context.focal_length_y);

    if (0 <= position.x &&
        position.x < context.resolution.x &&
        0 <= position.y &&
        position.y < context.resolution.y) {

        ivec2 iposition = ivec2(position);
        int width = int(context.resolution.x);

        vec3 result = callback(closure);
        std::unique_lock<std::mutex> lock(_light_mutex);
        _light_image[iposition.y * width + iposition.x] += result;

        return vec3(0.0f, 0.0f, 0.0f);
    }

    return vec3(0.0f, 0.0f, 0.0f);
}

void Technique::_for_each_ray(
    ImageView& view,
    render_context_t& context)
{
    const int xBegin = int(view.xBegin());
    const int xEnd = int(view.xEnd());
    const int rXBegin = int(xEnd - 1);
    const int rXEnd = int(xBegin - 1);
    const int yBegin = int(view.yBegin());
    const int yEnd = int(view.yEnd());

    runtime_assert(0 <= xBegin && xEnd <= (int)view.width());
    runtime_assert(0 <= yBegin && yEnd <= (int)view.height());

    auto shoot = [&](float x, float y) -> Ray {
        vec2 position = vec2(x + context.generator->sample(), y + context.generator->sample());

        vec3 direction = ray_direction(
            position,
            context.resolution,
            context.resolution_y_inv,
            context.focal_length_y);

        return { context.camera_position, context.view_to_world_mat3 * direction };
    };

    for (int y = yBegin; y < yEnd; ++y) {
        for (int x = xBegin; x < xEnd; ++x) {
            const Ray ray = shoot(float(x), float(y));
            context.pixel_position = vec2(x, y);
            _eye_image[y * view.width() + x] += _traceEye(context, ray);
        }

        ++y;

        if (y < yEnd) {
            for (int x = rXBegin; x > rXEnd; --x) {
                const Ray ray = shoot(float(x), float(y));
                context.pixel_position = vec2(x, y);
                _eye_image[y * view.width() + x] += _traceEye(context, ray);
            }
        }
    }
}

}
