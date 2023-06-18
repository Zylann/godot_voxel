#include "tests.h"
#include "../util/profiling.h"
#include "test_block_serializer.h"
#include "test_box3i.h"
#include "test_container_funcs.h"
#include "test_curve_range.h"
#include "test_detail_rendering_gpu.h"
#include "test_edition_funcs.h"
#include "test_expression_parser.h"
#include "test_flat_map.h"
#include "test_island_finder.h"
#include "test_mesh_sdf.h"
#include "test_octree.h"
#include "test_region_file.h"
#include "test_slot_map.h"
#include "test_spatial_lock.h"
#include "test_storage_funcs.h"
#include "test_threaded_task_runner.h"
#include "test_voxel_buffer.h"
#include "test_voxel_data_map.h"
#include "test_voxel_graph.h"
#include "test_voxel_instancer.h"
#include "test_voxel_mesher_cubes.h"
#include "testing.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "fast_noise_2/test_fast_noise_2.h"
#endif

#include <core/string/print_string.h>

namespace zylann::voxel::tests {

#define VOXEL_TEST(fname)                                                                                              \
	{                                                                                                                  \
		print_line(String("Running {0}").format(varray(#fname)));                                                      \
		ZN_PROFILE_SCOPE_NAMED(#fname);                                                                                \
		fname();                                                                                                       \
	}

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	using namespace zylann::tests;

	VOXEL_TEST(test_box3i_intersects);
	VOXEL_TEST(test_box3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_copy);
	VOXEL_TEST(test_encode_weights_packed_u16);
	VOXEL_TEST(test_copy_3d_region_zxy);
	VOXEL_TEST(test_voxel_graph_invalid_connection);
	VOXEL_TEST(test_voxel_graph_generator_default_graph_compilation);
	VOXEL_TEST(test_voxel_graph_sphere_on_plane);
	VOXEL_TEST(test_voxel_graph_clamp_simplification);
	VOXEL_TEST(test_voxel_graph_generator_expressions);
	VOXEL_TEST(test_voxel_graph_generator_expressions_2);
	VOXEL_TEST(test_voxel_graph_generator_texturing);
	VOXEL_TEST(test_voxel_graph_equivalence_merging);
	VOXEL_TEST(test_voxel_graph_generate_block_with_input_sdf);
	VOXEL_TEST(test_voxel_graph_functions_pass_through);
	VOXEL_TEST(test_voxel_graph_functions_nested_pass_through);
	VOXEL_TEST(test_voxel_graph_functions_autoconnect);
	VOXEL_TEST(test_voxel_graph_functions_io_mismatch);
	VOXEL_TEST(test_voxel_graph_functions_misc);
	VOXEL_TEST(test_voxel_graph_issue461);
	VOXEL_TEST(test_voxel_graph_fuzzing);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_voxel_graph_issue427);
#ifdef TOOLS_ENABLED
	VOXEL_TEST(test_voxel_graph_hash);
#endif
#endif
	VOXEL_TEST(test_voxel_graph_issue471);
	VOXEL_TEST(test_voxel_graph_unused_single_texture_output);
	VOXEL_TEST(test_voxel_graph_spots2d_optimized_execution_map);
	VOXEL_TEST(test_voxel_graph_unused_inner_output);
	VOXEL_TEST(test_island_finder);
	VOXEL_TEST(test_unordered_remove_if);
	VOXEL_TEST(test_instance_data_serialization);
	VOXEL_TEST(test_transform_3d_array_zxy);
	VOXEL_TEST(test_octree_update);
	VOXEL_TEST(test_octree_find_in_box);
	VOXEL_TEST(test_get_curve_monotonic_sections);
	VOXEL_TEST(test_voxel_buffer_create);
	VOXEL_TEST(test_block_serializer);
	VOXEL_TEST(test_block_serializer_stream_peer);
	VOXEL_TEST(test_region_file);
	VOXEL_TEST(test_voxel_stream_region_files);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_fast_noise_2_basic);
	VOXEL_TEST(test_fast_noise_2_empty_encoded_node_tree);
#endif
	VOXEL_TEST(test_run_blocky_random_tick);
	VOXEL_TEST(test_flat_map);
	VOXEL_TEST(test_expression_parser);
	VOXEL_TEST(test_voxel_buffer_metadata);
	VOXEL_TEST(test_voxel_buffer_metadata_gd);
	VOXEL_TEST(test_voxel_mesher_cubes);
	VOXEL_TEST(test_threaded_task_runner_misc);
	VOXEL_TEST(test_threaded_task_runner_debug_names);
	VOXEL_TEST(test_task_priority_values);
	VOXEL_TEST(test_voxel_mesh_sdf_issue463);
	VOXEL_TEST(test_normalmap_render_gpu);
	VOXEL_TEST(test_slot_map);
	VOXEL_TEST(test_box_blur);
	VOXEL_TEST(test_threaded_task_postponing);
	VOXEL_TEST(test_spatial_lock_misc);
	VOXEL_TEST(test_spatial_lock_spam);
	VOXEL_TEST(test_spatial_lock_dependent_map_chunks);

	print_line("------------ Voxel tests end -------------");
}

} // namespace zylann::voxel::tests
