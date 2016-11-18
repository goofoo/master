#include <xmmintrin.h>
#include <pmmintrin.h>
#include <runtime_assert>

#include <unittest.hpp>

#include <Application.hpp>

#include <threadpool.hpp>
#include <iostream>
#include <chrono>
#include <atomic>
#include <set>
#include <mutex>

using namespace std;
using namespace haste;

unittest() {
    std::cout << "Test test!" << std::endl;
}

unittest("named test") {
    std::cout << "Test test2!" << std::endl;
}


int main(int argc, char **argv) {
    run_all_tests();

    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    Options options = parseArgs(argc, argv);
    auto status = displayHelpIfNecessary(options, "0.0.1");

    runtime_assert(sin(half_pi<float>()) == 1.0f);
    runtime_assert(asin(1.0f) == half_pi<float>());

    if (status.first) {
        return status.second;
    }

    if (options.action == Options::AVG) {
        printAVG(options.input0);
    }
    else if (options.action == Options::RMS) {
        printRMS(options.input0, options.input1);
    }
    else {
        Application application(options);

        if (!options.batch) {
            return application.run(options.width, options.height, options.caption());
        }
        else {
            return application.runBatch(options.width, options.height);
        }
    }

    return 0;
}
