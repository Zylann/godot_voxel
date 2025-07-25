#include "tests.h"
#include "../util/profiling.h"
#include "../util/testing/test_options.h"

#include "util/test_box3i.h"
#include "util/test_container_funcs.h"
#include "util/test_expression_parser.h"
#include "util/test_flat_map.h"
#include "util/test_island_finder.h"
#include "util/test_math_funcs.h"
#include "util/test_noise.h"
#include "util/test_slot_map.h"
#include "util/test_spatial_lock.h"
#include "util/test_string_funcs.h"
#include "util/test_threaded_task_runner.h"

#include "voxel/test_block_serializer.h"
#include "voxel/test_curve_range.h"
#include "voxel/test_edition_funcs.h"
#include "voxel/test_octree.h"
#include "voxel/test_raycast.h"
#include "voxel/test_region_file.h"
#include "voxel/test_storage_funcs.h"
#include "voxel/test_voxel_buffer.h"
#include "voxel/test_voxel_data_map.h"
#include "voxel/test_voxel_graph.h"
#include "voxel/test_voxel_instancer.h"
#include "voxel/test_voxel_mesher_cubes.h"

#ifdef VOXEL_ENABLE_SMOOTH_MESHING
#include "voxel/test_transvoxel.h"
#ifdef VOXEL_ENABLE_GPU
#include "voxel/test_detail_rendering_gpu.h"
#endif
#endif

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "fast_noise_2/test_fast_noise_2.h"
#endif

#ifdef VOXEL_ENABLE_SQLITE
#include "voxel/test_stream_sqlite.h"
#endif

#ifdef VOXEL_ENABLE_MESH_SDF
#include "voxel/test_mesh_sdf.h"
#endif

namespace zylann::voxel::tests {

#define VOXEL_TEST(fname)                                                                                              \
	if (options.can_run_print(#fname)) {                                                                               \
		ZN_PROFILE_SCOPE_NAMED(#fname);                                                                                \
		fname();                                                                                                       \
	}

void run_voxel_tests(const testing::TestOptions &options) {
	print_line("------------ Voxel tests begin -------------");

	using namespace zylann::tests;

	VOXEL_TEST(test_wrap);
	VOXEL_TEST(test_int32_to_string_base10);
	VOXEL_TEST(test_string_base10_to_int32);
	VOXEL_TEST(test_voxel_buffer_metadata);
	VOXEL_TEST(test_voxel_buffer_metadata_gd);
	VOXEL_TEST(test_voxel_buffer_paste_masked);
	VOXEL_TEST(test_voxel_buffer_paste_masked_metadata);
	VOXEL_TEST(test_voxel_buffer_paste_masked_metadata_oob);
	VOXEL_TEST(test_image_range_grid);
	VOXEL_TEST(test_box3i_intersects);
	VOXEL_TEST(test_box3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_paste_dst_mask);
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
#ifdef VOXEL_ENABLE_GPU
	VOXEL_TEST(test_voxel_graph_issue461);
#endif
	VOXEL_TEST(test_voxel_graph_fuzzing);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_voxel_graph_issue427);
#ifdef TOOLS_ENABLED
	VOXEL_TEST(test_voxel_graph_hash);
#endif
#endif
#ifdef VOXEL_ENABLE_GPU
	VOXEL_TEST(test_voxel_graph_issue471);
#endif
	VOXEL_TEST(test_voxel_graph_unused_single_texture_output);
	VOXEL_TEST(test_voxel_graph_spots2d_optimized_execution_map);
	VOXEL_TEST(test_voxel_graph_unused_inner_output);
	VOXEL_TEST(test_voxel_graph_function_execute);
	VOXEL_TEST(test_voxel_graph_image);
	VOXEL_TEST(test_voxel_graph_many_weight_outputs);
	VOXEL_TEST(test_voxel_graph_many_subdivisions);
	VOXEL_TEST(test_voxel_graph_non_square_image);
	VOXEL_TEST(test_voxel_graph_empty_image);
	VOXEL_TEST(test_voxel_graph_4_default_weights);
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
	VOXEL_TEST(test_voxel_mesher_cubes);
	VOXEL_TEST(test_threaded_task_runner_misc);
	VOXEL_TEST(test_threaded_task_runner_debug_names);
	VOXEL_TEST(test_task_priority_values);
#ifdef VOXEL_ENABLE_MESH_SDF
	VOXEL_TEST(test_voxel_mesh_sdf_issue463);
#endif
#ifdef VOXEL_ENABLE_SMOOTH_MESHING
#ifdef VOXEL_ENABLE_GPU
	VOXEL_TEST(test_normalmap_render_gpu);
#endif
#endif
	VOXEL_TEST(test_slot_map);
	VOXEL_TEST(test_box_blur);
	VOXEL_TEST(test_threaded_task_postponing);
	VOXEL_TEST(test_spatial_lock_misc);
	VOXEL_TEST(test_spatial_lock_spam);
	VOXEL_TEST(test_spatial_lock_dependent_map_chunks);
	VOXEL_TEST(test_discord_soakil_copypaste);
#ifdef VOXEL_ENABLE_SQLITE
	VOXEL_TEST(test_voxel_stream_sqlite_key_string_csd_encoding);
	VOXEL_TEST(test_voxel_stream_sqlite_key_blob80_encoding);
	VOXEL_TEST(test_voxel_stream_sqlite_basic);
	VOXEL_TEST(test_voxel_stream_sqlite_coordinate_format);
#endif
	VOXEL_TEST(test_sdf_hemisphere);
	VOXEL_TEST(test_fnl_range);
	VOXEL_TEST(test_voxel_buffer_set_channel_bytes);
	VOXEL_TEST(test_voxel_buffer_issue769);
	VOXEL_TEST(test_raycast_sdf);
	VOXEL_TEST(test_raycast_blocky);
	VOXEL_TEST(test_raycast_blocky_no_cache_graph);
	VOXEL_TEST(test_voxel_graph_constant_reduction);
#ifdef VOXEL_ENABLE_SMOOTH_MESHING
	VOXEL_TEST(test_transvoxel_issue772);
#endif
#ifdef VOXEL_ENABLE_INSTANCER
	VOXEL_TEST(test_instance_generator_material_filter_issue774);
#endif
	VOXEL_TEST(test_spot_noise);

	print_line("------------ Voxel tests end -------------");
}

} // namespace zylann::voxel::tests
