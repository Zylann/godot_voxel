#ifndef VOXEL_TESTS_H
#define VOXEL_TESTS_H

#include "../util/godot/macros.h"

namespace zylann {

namespace testing {
class TestOptions;
}

namespace voxel {

namespace tests {
void run_voxel_tests(const testing::TestOptions &options);
}

namespace noise_tests {
void run_noise_tests();
}

} // namespace voxel
} // namespace zylann

#endif // VOXEL_TESTS_H
