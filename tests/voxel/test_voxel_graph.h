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
#ifdef VOXEL_ENABLE_GPU
void test_voxel_graph_issue461();
#endif
void test_voxel_graph_fuzzing();
#ifdef VOXEL_ENABLE_FAST_NOISE_2
void test_voxel_graph_issue427();
#ifdef TOOLS_ENABLED
void test_voxel_graph_hash();
#endif
#endif
#ifdef VOXEL_ENABLE_GPU
void test_voxel_graph_issue471();
#endif
void test_voxel_graph_unused_single_texture_output();
void test_voxel_graph_spots2d_optimized_execution_map();
void test_voxel_graph_unused_inner_output();
void test_voxel_graph_function_execute();
void test_voxel_graph_image();
void test_voxel_graph_many_weight_outputs();
void test_image_range_grid();
void test_voxel_graph_many_subdivisions();
void test_voxel_graph_non_square_image();
void test_voxel_graph_4_default_weights();
void test_voxel_graph_empty_image();
void test_voxel_graph_constant_reduction();
void test_voxel_graph_multiple_function_instances();
void test_voxel_graph_issue783();
void test_voxel_graph_broad_block();
void test_voxel_graph_set_default_input_by_name();
void test_voxel_graph_get_io_indices();

} // namespace zylann::voxel::tests

#endif // VOXEL_TEST_VOXEL_GRAPH_H
