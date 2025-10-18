#ifndef VOXEL_TESTS_VOXEL_BUFFER_H
#define VOXEL_TESTS_VOXEL_BUFFER_H

namespace zylann::voxel::tests {

void test_voxel_buffer_create();
void test_voxel_buffer_metadata();
void test_voxel_buffer_metadata_gd();
void test_voxel_buffer_paste_masked();
void test_voxel_buffer_paste_masked_metadata();
void test_voxel_buffer_paste_masked_metadata_oob();
void test_voxel_buffer_set_channel_bytes();
void test_voxel_buffer_issue769();

} // namespace zylann::voxel::tests

#endif // VOXEL_TESTS_VOXEL_BUFFER_H
