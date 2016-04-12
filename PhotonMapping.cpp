#include <GLFW/glfw3.h>
#include <PhotonMapping.hpp>

namespace haste {

PhotonMapping::PhotonMapping(size_t numPhotons)
    : _numPhotons(numPhotons)
    , _numPhotonsInv(1.f / numPhotons) { }

void PhotonMapping::hardReset() {
    softReset();
    _stage = _Scatter;
    _totalPower = _scene->lights.queryTotalPower();
}

void PhotonMapping::updateInteractive(double timeQuantum) {
    double startTime = glfwGetTime();
    size_t startRays = _scene->numRays();

    switch (_stage) {
        case _Scatter:
            _scatterPhotonsInteractive(timeQuantum);
            break;
        case _Build:
            _buildPhotonMapInteractive(timeQuantum);
            break;
        case _BuildDone:
            _stage = _Gather;
            break;
        case _Gather:
            _gatherPhotonsInteractive(timeQuantum);
            break;
    }

    _renderTime += glfwGetTime() - startTime;
    _numRays += _scene->numRays() - startRays;
}

string PhotonMapping::stageName() const {
    switch(_stage) {
        case _Scatter: return "Scattering photons";
        case _Build:
        case _BuildDone: return "Building photon map";
        case _Gather: return "Gathering photons";
    }
}

double PhotonMapping::stageProgress() const {
    switch(_stage) {
        case _Scatter: return double(_numEmitted) / _numPhotons;
        case _Build: return 0.0;
        case _BuildDone: return 1.0;
        case _Gather: return Technique::stageProgress();
    }
}

void PhotonMapping::_scatterPhotonsInteractive(double timeQuantum) {
    if (_auxiliary.empty()) {
        _numEmitted = 0;
    }

    const size_t batchSize = 1000;
    double startTime = glfwGetTime();

    while (_numEmitted < _numPhotons && glfwGetTime() - startTime < timeQuantum) {
        const size_t begin = _numEmitted;
        const size_t end = min(_numPhotons, begin + batchSize);
        const size_t stored = _auxiliary.size();

        _scatterPhotons(begin, end);
        _renderPhotons(stored, _auxiliary.size());
        _numEmitted = end;
    }

    if (_numEmitted == _numPhotons) {
        _stage = _Build;
    }
}

void PhotonMapping::_scatterPhotons(size_t begin, size_t end) {
    const float scaleFactor = _totalPower * _numPhotonsInv;

    for (size_t i = begin; i < end; ++i) {
        Photon photon = _scene->lights.emit();
        photon.power *= scaleFactor;

        for (size_t j = 0; ; ++j) {
            RayIsect isect = _scene->intersect(
                photon.position,
                photon.direction);

            if (!isect.isPresent() || !_scene->isMesh(isect)) {
                break;
            }

            photon.position = isect.position;
            photon.direction = -photon.direction;

            _auxiliary.push_back(photon);

            SurfacePoint point = _scene->querySurface(isect);

            if(!_scene->materials.scatter(photon, point)) {
                break;
            }
        }
    }
}

void PhotonMapping::_renderPhotons(size_t begin, size_t end) {
    float f5width = 0.5f * float(_width);
    float f5height = 0.5f * float(_height);
    mat4 proj = _camera->proj(_width, _height) * inverse(_camera->view);
    const float scaleFactor = 1.f / (_totalPower * _numPhotonsInv);

    for (size_t i = begin; i < end; ++i) {
        vec4 h = proj * vec4(_auxiliary[i].position, 1.0);
        vec3 c = _auxiliary[i].power * scaleFactor;
        vec3 v = h.xyz() / h.w;

        if (-1.0f <= v.z && v.z <= +1.0f) {
            int x = int((v.x + 1.0f) * f5width + 0.5f);
            int y = int((v.y + 1.0f) * f5height + 0.5f);

            for (int j = y - 0; j <= y + 0; ++j) {
                for (int i = x - 0; i <= x + 0; ++i) {
                    if (0 <= i && i < _width && 0 <= j && j < _height) {
                        _image[j * _width + i] += vec4(c, 1.0f);
                        _image[j * _width + i].w = 1.0f;
                    }
                }
            }
        }
    }
}

void PhotonMapping::_buildPhotonMapInteractive(double timeQuantum) {
    _photons = PhotonMap(std::move(_auxiliary));
    _auxiliary = vector<Photon>();
    _stage = _BuildDone;
    softReset();
}

void PhotonMapping::_gatherPhotonsInteractive(double timeQuantum) {

}

}
