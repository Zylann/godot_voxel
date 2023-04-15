#ifndef VOXEL_TEST_VOXEL_GRAPH_H
#define VOXEL_TEST_VOXEL_GRAPH_H

namespace zylann::voxel::tests {

void test_voxel_graph_invalid_connection();
void test_voxel_graph_generator_default_graph_compilation();
void test_voxel_graph_sphere_on_plane();
void test_voxel_graph_clamp_simplification();
void test_voxel_graph_generator_expressions();
void test_voxel_graph_generator_expressions_2();
void test_voxel_graph_generator_texturing();
void test_voxel_graph_equivalence_merging();
void test_voxel_graph_generate_block_with_input_sdf();
void test_voxel_graph_functions_pass_through();
void test_voxel_graph_functions_nested_pass_through();
void test_voxel_graph_functions_autoconnect();
void test_voxel_graph_functions_io_mismatch();
void test_voxel_graph_functions_misc();
void test_voxel_graph_issue461();
void test_voxel_graph_fuzzing();
#ifdef VOXEL_ENABLE_FAST_NOISE_2
void test_voxel_graph_issue427();
#ifdef TOOLS_ENABLED
void test_voxel_graph_hash();
#endif
#endif
void test_voxel_graph_issue471();
void test_voxel_graph_unused_single_texture_output();
void test_voxel_graph_spots2d_optimized_execution_map();
void test_voxel_graph_unused_inner_output();

} // namespace zylann::voxel::tests

#endif // VOXEL_TEST_VOXEL_GRAPH_H
