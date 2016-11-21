#include <Viewer.hpp>
#include <thread>
#include <chrono>

namespace haste {

Viewer::Viewer(const vector<vec4>& data, size_t width, size_t height)
    : Technique(1) {
    _data = data;
    _width = width;
    _height = height;
}

void Viewer::render(
    ImageView& view,
    RandomEngine& engine,
    size_t cameraId)
{
    view.copyFrom(_data, _width, _height);
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(33));
}

string Viewer::name() const {
    return "Viewer";
}

}
