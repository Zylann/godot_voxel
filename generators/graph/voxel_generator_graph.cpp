#include "voxel_generator_graph.h"
#include "../../constants/voxel_string_names.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../util/container_funcs.h"
#include "../../util/expression_parser.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "../../util/hash_funcs.h"
#include "../../util/log.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "node_type_db.h"
#include "voxel_graph_function.h"

namespace zylann::voxel {

const char *VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

VoxelGeneratorGraph::VoxelGeneratorGraph() {
	_main_function.instantiate();
	_main_function->connect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorGraph, _on_subresource_changed));
}

VoxelGeneratorGraph::~VoxelGeneratorGraph() {
	clear();
}

VoxelGeneratorGraph::Cache &VoxelGeneratorGraph::get_tls_cache() {
	thread_local Cache cache;
	return cache;
}

void VoxelGeneratorGraph::clear() {
	ERR_FAIL_COND(_main_function.is_null());
	_main_function->clear();

	{
		RWLockWrite wlock(_runtime_lock);
		_runtime.reset();
	}
}

Ref<pg::VoxelGraphFunction> VoxelGeneratorGraph::get_main_function() const {
	return _main_function;
}

bool VoxelGeneratorGraph::is_using_optimized_execution_map() const {
	return _use_optimized_execution_map;
}

void VoxelGeneratorGraph::set_use_optimized_execution_map(bool use) {
	_use_optimized_execution_map = use;
}

float VoxelGeneratorGraph::get_sdf_clip_threshold() const {
	return _sdf_clip_threshold;
}

void VoxelGeneratorGraph::set_sdf_clip_threshold(float t) {
	_sdf_clip_threshold = math::max(t, 0.f);
}

int VoxelGeneratorGraph::get_used_channels_mask() const {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	if (runtime_ptr == nullptr) {
		// The graph hasn't been compiled yet, we can't tell which channels it produces.
		return 0;
	}
	int mask = 0;
	if (runtime_ptr->sdf_output_index != -1) {
		mask |= (1 << VoxelBufferInternal::CHANNEL_SDF);
	}
	if (runtime_ptr->type_output_index != -1) {
		mask |= (1 << VoxelBufferInternal::CHANNEL_TYPE);
	}
	if (runtime_ptr->weight_outputs_count > 0 || runtime_ptr->single_texture_output_index != -1) {
		mask |= (1 << VoxelBufferInternal::CHANNEL_INDICES);
		mask |= (1 << VoxelBufferInternal::CHANNEL_WEIGHTS);
	}
	return mask;
}

void VoxelGeneratorGraph::set_use_subdivision(bool use) {
	_use_subdivision = use;
}

bool VoxelGeneratorGraph::is_using_subdivision() const {
	return _use_subdivision;
}

void VoxelGeneratorGraph::set_subdivision_size(int size) {
	_subdivision_size = size;
}

int VoxelGeneratorGraph::get_subdivision_size() const {
	return _subdivision_size;
}

void VoxelGeneratorGraph::set_debug_clipped_blocks(bool enabled) {
	_debug_clipped_blocks = enabled;
}

bool VoxelGeneratorGraph::is_debug_clipped_blocks() const {
	return _debug_clipped_blocks;
}

void VoxelGeneratorGraph::set_use_xz_caching(bool enabled) {
	_use_xz_caching = enabled;
}

bool VoxelGeneratorGraph::is_using_xz_caching() const {
	return _use_xz_caching;
}

// TODO Optimization: generating indices and weights on every voxel of a block might be avoidable
// Instead, we could only generate them near zero-crossings, because this is where materials will be seen.
// The problem is that it's harder to manage at the moment, to support edited blocks and LOD...
void VoxelGeneratorGraph::gather_indices_and_weights(Span<const WeightOutput> weight_outputs,
		const pg::Runtime::State &state, Vector3i rmin, Vector3i rmax, int ry, VoxelBufferInternal &out_voxel_buffer,
		FixedArray<uint8_t, 4> spare_indices) {
	ZN_PROFILE_SCOPE();

	// TODO Optimization: exclude up-front outputs that are known to be zero?
	// So we choose the cases below based on non-zero outputs instead of total output count

	// TODO Could maybe put this part outside?
	FixedArray<Span<const float>, 16> buffers;
	const unsigned int buffers_count = weight_outputs.size();
	for (unsigned int oi = 0; oi < buffers_count; ++oi) {
		const WeightOutput &info = weight_outputs[oi];
		const pg::Runtime::Buffer &buffer = state.get_buffer(info.output_buffer_index);
		buffers[oi] = Span<const float>(buffer.data, buffer.size);
	}

	if (buffers_count <= 4) {
		// Pick all results and fill with spare indices to keep semantic
		unsigned int value_index = 0;
		for (int rz = rmin.z; rz < rmax.z; ++rz) {
			for (int rx = rmin.x; rx < rmax.x; ++rx) {
				FixedArray<uint8_t, 4> weights;
				FixedArray<uint8_t, 4> indices = spare_indices;
				fill(weights, uint8_t(0));
				for (unsigned int oi = 0; oi < buffers_count; ++oi) {
					const float weight = buffers[oi][value_index];
					// TODO Optimization: weight output nodes could already multiply by 255 and clamp afterward
					// so we would not need to do it here
					weights[oi] = math::clamp(weight * 255.f, 0.f, 255.f);
					indices[oi] = weight_outputs[oi].layer_index;
				}
				debug_check_texture_indices(indices);
				const uint16_t encoded_indices =
						encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
				const uint16_t encoded_weights =
						encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
				// TODO Flatten this further?
				out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBufferInternal::CHANNEL_INDICES);
				out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBufferInternal::CHANNEL_WEIGHTS);
				++value_index;
			}
		}

	} else if (buffers_count == 4) {
		// Pick all results
		unsigned int value_index = 0;
		for (int rz = rmin.z; rz < rmax.z; ++rz) {
			for (int rx = rmin.x; rx < rmax.x; ++rx) {
				FixedArray<uint8_t, 4> weights;
				FixedArray<uint8_t, 4> indices;
				for (unsigned int oi = 0; oi < buffers_count; ++oi) {
					const float weight = buffers[oi][value_index];
					weights[oi] = math::clamp(weight * 255.f, 0.f, 255.f);
					indices[oi] = weight_outputs[oi].layer_index;
				}
				const uint16_t encoded_indices =
						encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
				const uint16_t encoded_weights =
						encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
				// TODO Flatten this further?
				out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBufferInternal::CHANNEL_INDICES);
				out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBufferInternal::CHANNEL_WEIGHTS);
				++value_index;
			}
		}

	} else {
		// More weights than we can have per voxel. Will need to pick most represented weights
		const float pivot = 1.f / 5.f;
		unsigned int value_index = 0;
		FixedArray<uint8_t, 16> skipped_outputs;
		for (int rz = rmin.z; rz < rmax.z; ++rz) {
			for (int rx = rmin.x; rx < rmax.x; ++rx) {
				FixedArray<uint8_t, 4> weights;
				FixedArray<uint8_t, 4> indices;
				unsigned int skipped_outputs_count = 0;
				fill(indices, uint8_t(0));
				weights[0] = 255;
				weights[1] = 0;
				weights[2] = 0;
				weights[3] = 0;
				unsigned int recorded_weights = 0;
				// Pick up weights above pivot (this is not as correct as a sort but faster)
				for (unsigned int oi = 0; oi < buffers_count && recorded_weights < indices.size(); ++oi) {
					const float weight = buffers[oi][value_index];
					if (weight > pivot) {
						weights[recorded_weights] = math::clamp(weight * 255.f, 0.f, 255.f);
						indices[recorded_weights] = weight_outputs[oi].layer_index;
						++recorded_weights;
					} else {
						skipped_outputs[skipped_outputs_count] = oi;
						++skipped_outputs_count;
					}
				}
				// If we found less outputs above pivot than expected, fill with some skipped outputs.
				// We have to do this because if an index appears twice with a different corresponding weight,
				// then the latest weight will take precedence, which would be unwanted
				for (unsigned int oi = recorded_weights; oi < indices.size(); ++oi) {
					indices[oi] = skipped_outputs[oi - recorded_weights];
				}
				const uint16_t encoded_indices =
						encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
				const uint16_t encoded_weights =
						encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
				// TODO Flatten this further?
				out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBufferInternal::CHANNEL_INDICES);
				out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBufferInternal::CHANNEL_WEIGHTS);
				++value_index;
			}
		}
	}
}

// TODO Optimization: this is a simplified output using a complex system.
// We should implement a texturing system that knows each voxel has a single texture.
void gather_indices_and_weights_from_single_texture(unsigned int output_buffer_index, const pg::Runtime::State &state,
		Vector3i rmin, Vector3i rmax, int ry, VoxelBufferInternal &out_voxel_buffer) {
	ZN_PROFILE_SCOPE();

	const pg::Runtime::Buffer &buffer = state.get_buffer(output_buffer_index);
	Span<const float> buffer_data = Span<const float>(buffer.data, buffer.size);

	// TODO Should not really be here, but may work. Left here for now so all code for this is in one place
	const uint16_t encoded_weights = encode_weights_to_packed_u16_lossy(255, 0, 0, 0);
	out_voxel_buffer.clear_channel(VoxelBufferInternal::CHANNEL_WEIGHTS, encoded_weights);

	unsigned int value_index = 0;
	for (int rz = rmin.z; rz < rmax.z; ++rz) {
		for (int rx = rmin.x; rx < rmax.x; ++rx) {
			const uint8_t index = math::clamp(int(Math::round(buffer_data[value_index])), 0, 15);
			// Make sure other indices are different so the weights associated with them don't override the first
			// index's weight
			const uint8_t other_index = (index == 0 ? 1 : 0);
			const uint16_t encoded_indices = encode_indices_to_packed_u16(index, other_index, other_index, other_index);
			out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBufferInternal::CHANNEL_INDICES);
			++value_index;
		}
	}
}

template <typename F, typename Data_T>
void fill_zx_sdf_slice(Span<Data_T> channel_data, float sdf_scale, Vector3i rmin, Vector3i rmax, int ry, int x_stride,
		const float *src_data, Vector3i buffer_size, F convert_func) {
	unsigned int src_i = 0;
	for (int rz = rmin.z; rz < rmax.z; ++rz) {
		unsigned int dst_i = Vector3iUtil::get_zxy_index(Vector3i(rmin.x, ry, rz), buffer_size);
		for (int rx = rmin.x; rx < rmax.x; ++rx) {
			channel_data[dst_i] = convert_func(sdf_scale * src_data[src_i]);
			++src_i;
			dst_i += x_stride;
		}
	}
}

static void fill_zx_sdf_slice(const pg::Runtime::Buffer &sdf_buffer, VoxelBufferInternal &out_buffer,
		unsigned int channel, VoxelBufferInternal::Depth channel_depth, float sdf_scale, Vector3i rmin, Vector3i rmax,
		int ry) {
	ZN_PROFILE_SCOPE_NAMED("Copy SDF to block");

	if (out_buffer.get_channel_compression(channel) != VoxelBufferInternal::COMPRESSION_NONE) {
		out_buffer.decompress_channel(channel);
	}
	Span<uint8_t> channel_bytes;
	ERR_FAIL_COND(!out_buffer.get_channel_raw(channel, channel_bytes));
	const Vector3i buffer_size = out_buffer.get_size();
	const unsigned int x_stride = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 0), buffer_size) -
			Vector3iUtil::get_zxy_index(Vector3i(0, 0, 0), buffer_size);

	switch (channel_depth) {
		case VoxelBufferInternal::DEPTH_8_BIT:
			fill_zx_sdf_slice(
					channel_bytes, sdf_scale, rmin, rmax, ry, x_stride, sdf_buffer.data, buffer_size, snorm_to_s8);
			break;

		case VoxelBufferInternal::DEPTH_16_BIT:
			fill_zx_sdf_slice(channel_bytes.reinterpret_cast_to<uint16_t>(), sdf_scale, rmin, rmax, ry, x_stride,
					sdf_buffer.data, buffer_size, snorm_to_s16);
			break;

		case VoxelBufferInternal::DEPTH_32_BIT:
			fill_zx_sdf_slice(channel_bytes.reinterpret_cast_to<float>(), sdf_scale, rmin, rmax, ry, x_stride,
					sdf_buffer.data, buffer_size, [](float v) { return v; });
			break;

		case VoxelBufferInternal::DEPTH_64_BIT:
			fill_zx_sdf_slice(channel_bytes.reinterpret_cast_to<double>(), sdf_scale, rmin, rmax, ry, x_stride,
					sdf_buffer.data, buffer_size, [](double v) { return v; });
			break;

		default:
			ZN_PRINT_ERROR(format("Unknown depth {}", channel_depth));
			break;
	}
}

template <typename F, typename Data_T>
void fill_zx_integer_slice(Span<Data_T> channel_data, Vector3i rmin, Vector3i rmax, int ry, int x_stride,
		const float *src_data, Vector3i buffer_size) {
	unsigned int src_i = 0;
	for (int rz = rmin.z; rz < rmax.z; ++rz) {
		unsigned int dst_i = Vector3iUtil::get_zxy_index(Vector3i(rmin.x, ry, rz), buffer_size);
		for (int rx = rmin.x; rx < rmax.x; ++rx) {
			channel_data[dst_i] = Data_T(Math::round(src_data[src_i]));
			++src_i;
			dst_i += x_stride;
		}
	}
}

static void fill_zx_integer_slice(const pg::Runtime::Buffer &src_buffer, VoxelBufferInternal &out_buffer,
		unsigned int channel, VoxelBufferInternal::Depth channel_depth, Vector3i rmin, Vector3i rmax, int ry) {
	ZN_PROFILE_SCOPE_NAMED("Copy integer data to block");

	if (out_buffer.get_channel_compression(channel) != VoxelBufferInternal::COMPRESSION_NONE) {
		out_buffer.decompress_channel(channel);
	}
	Span<uint8_t> channel_bytes;
	ERR_FAIL_COND(!out_buffer.get_channel_raw(channel, channel_bytes));
	const Vector3i buffer_size = out_buffer.get_size();
	const unsigned int x_stride = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 0), buffer_size) -
			Vector3iUtil::get_zxy_index(Vector3i(0, 0, 0), buffer_size);

	switch (channel_depth) {
		case VoxelBufferInternal::DEPTH_8_BIT:
			fill_zx_integer_slice<uint8_t>(channel_bytes, rmin, rmax, ry, x_stride, src_buffer.data, buffer_size);
			break;

		case VoxelBufferInternal::DEPTH_16_BIT:
			fill_zx_integer_slice<uint16_t>(channel_bytes.reinterpret_cast_to<uint16_t>(), rmin, rmax, ry, x_stride,
					src_buffer.data, buffer_size);
			break;

		case VoxelBufferInternal::DEPTH_32_BIT:
			fill_zx_integer_slice<uint32_t>(channel_bytes.reinterpret_cast_to<uint32_t>(), rmin, rmax, ry, x_stride,
					src_buffer.data, buffer_size);
			break;

		case VoxelBufferInternal::DEPTH_64_BIT:
			fill_zx_integer_slice<uint64_t>(channel_bytes.reinterpret_cast_to<uint64_t>(), rmin, rmax, ry, x_stride,
					src_buffer.data, buffer_size);
			break;

		default:
			ZN_PRINT_ERROR(format("Unknown depth {}", channel_depth));
			break;
	}
}

VoxelGenerator::Result VoxelGeneratorGraph::generate_block(VoxelGenerator::VoxelQueryData &input) {
	std::shared_ptr<Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	Result result;

	if (runtime_ptr == nullptr) {
		return result;
	}

	VoxelBufferInternal &out_buffer = input.voxel_buffer;

	const Vector3i bs = out_buffer.get_size();
	const VoxelBufferInternal::ChannelId sdf_channel = VoxelBufferInternal::CHANNEL_SDF;
	const Vector3i origin = input.origin_in_voxels;

	// TODO This may be shared across the module
	// Storing voxels is lossy on some depth configurations. They use normalized SDF,
	// so we must scale the values to make better use of the offered resolution
	const VoxelBufferInternal::Depth sdf_channel_depth = out_buffer.get_channel_depth(sdf_channel);
	const float sdf_scale = VoxelBufferInternal::get_sdf_quantization_scale(sdf_channel_depth);

	const VoxelBufferInternal::ChannelId type_channel = VoxelBufferInternal::CHANNEL_TYPE;
	const VoxelBufferInternal::Depth type_channel_depth = out_buffer.get_channel_depth(type_channel);

	const int stride = 1 << input.lod;

	// Clip threshold must be higher for higher lod indexes because distances for one sampled voxel are also larger
	const float clip_threshold = sdf_scale * _sdf_clip_threshold * stride;

	// Block size must be a multiple of section size, as all sections must have the same size
	const bool can_use_subdivision =
			(bs.x % _subdivision_size == 0) && (bs.y % _subdivision_size == 0) && (bs.z % _subdivision_size == 0);

	const Vector3i section_size =
			_use_subdivision && can_use_subdivision ? Vector3iUtil::create(_subdivision_size) : bs;
	// ERR_FAIL_COND_V(bs.x % section_size != 0, result);
	// ERR_FAIL_COND_V(bs.y % section_size != 0, result);
	// ERR_FAIL_COND_V(bs.z % section_size != 0, result);

	Cache &cache = get_tls_cache();

	// Slice is on the Y axis
	const unsigned int slice_buffer_size = section_size.x * section_size.z;
	pg::Runtime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, slice_buffer_size, false);

	cache.x_cache.resize(slice_buffer_size);
	cache.y_cache.resize(slice_buffer_size);
	cache.z_cache.resize(slice_buffer_size);

	Span<float> x_cache = to_span(cache.x_cache);
	Span<float> y_cache = to_span(cache.y_cache);
	Span<float> z_cache = to_span(cache.z_cache);

	const float air_sdf = _debug_clipped_blocks ? -1.f : 1.f;
	const float matter_sdf = _debug_clipped_blocks ? 1.f : -1.f;

	FixedArray<uint8_t, 4> spare_texture_indices = runtime_ptr->spare_texture_indices;
	const int sdf_output_buffer_index = runtime_ptr->sdf_output_buffer_index;
	const int type_output_buffer_index = runtime_ptr->type_output_buffer_index;

	FixedArray<unsigned int, pg::Runtime::MAX_OUTPUTS> required_outputs;
	unsigned int required_outputs_count = 0;

	bool all_sdf_is_air = (sdf_output_buffer_index != -1) && (type_output_buffer_index == -1);
	bool all_sdf_is_matter = all_sdf_is_air;

	math::Interval sdf_input_range;
	Span<float> input_sdf_full_cache;
	Span<float> input_sdf_slice_cache;
	if (runtime_ptr->sdf_input_index != -1) {
		ZN_PROFILE_SCOPE();
		cache.input_sdf_slice_cache.resize(slice_buffer_size);
		input_sdf_slice_cache = to_span(cache.input_sdf_slice_cache);

		const int64_t volume = Vector3iUtil::get_volume(bs);
		cache.input_sdf_full_cache.resize(volume);
		input_sdf_full_cache = to_span(cache.input_sdf_full_cache);

		// Note, a copy of the data is notably needed because we are going to write into that same buffer.
		get_unscaled_sdf(out_buffer, input_sdf_full_cache);

		sdf_input_range = math::Interval::from_single_value(input_sdf_full_cache[0]);
		for (const float sd : input_sdf_full_cache) {
			sdf_input_range.add_point(sd);
		}
	}

	// For each subdivision of the block
	for (int sz = 0; sz < bs.z; sz += section_size.z) {
		for (int sy = 0; sy < bs.y; sy += section_size.y) {
			for (int sx = 0; sx < bs.x; sx += section_size.x) {
				ZN_PROFILE_SCOPE_NAMED("Section");

				const Vector3i rmin(sx, sy, sz);
				const Vector3i rmax = rmin + Vector3i(section_size);
				const Vector3i gmin = origin + (rmin << input.lod);
				const Vector3i gmax = origin + (rmax << input.lod);

				// Do a quick analysis of the area. We'll only compute voxels if necessary.
				{
					QueryInputs<math::Interval> range_inputs(*runtime_ptr, math::Interval(gmin.x, gmax.x),
							math::Interval(gmin.y, gmax.y), math::Interval(gmin.z, gmax.z), sdf_input_range);
					runtime.analyze_range(cache.state, range_inputs.get());
				}

				bool sdf_is_air = true;
				bool sdf_is_uniform = true;
				if (sdf_output_buffer_index != -1) {
					const math::Interval sdf_range = cache.state.get_range(sdf_output_buffer_index) * sdf_scale;
					bool sdf_is_matter = false;

					if (sdf_range.min > clip_threshold && sdf_range.max > clip_threshold) {
						out_buffer.fill_area_f(air_sdf, rmin, rmax, sdf_channel);
						sdf_is_air = true;

					} else if (sdf_range.min < -clip_threshold && sdf_range.max < -clip_threshold) {
						out_buffer.fill_area_f(matter_sdf, rmin, rmax, sdf_channel);
						sdf_is_air = false;
						sdf_is_matter = true;

					} else if (sdf_range.is_single_value()) {
						out_buffer.fill_area_f(sdf_range.min, rmin, rmax, sdf_channel);
						sdf_is_air = sdf_range.min > 0.f;
						sdf_is_matter = !sdf_is_air;

					} else {
						// SDF is not uniform, we'll need to compute it per voxel
						required_outputs[required_outputs_count] = runtime_ptr->sdf_output_index;
						++required_outputs_count;
						sdf_is_air = false;
						sdf_is_uniform = false;
					}

					all_sdf_is_air = all_sdf_is_air && sdf_is_air;
					all_sdf_is_matter = all_sdf_is_matter && sdf_is_matter;
				}

				bool type_is_uniform = false;
				if (type_output_buffer_index != -1) {
					const math::Interval type_range = cache.state.get_range(type_output_buffer_index);
					if (type_range.is_single_value()) {
						out_buffer.fill_area(int(type_range.min), rmin, rmax, type_channel);
						type_is_uniform = true;
					} else {
						// Types are not uniform, we'll need to compute them per voxel
						required_outputs[required_outputs_count] = runtime_ptr->type_output_index;
						++required_outputs_count;
					}
				}

				if (runtime_ptr->weight_outputs_count > 0 && !sdf_is_air) {
					// We can skip this when SDF is air because there won't be any matter to give a texture to
					// TODO Range analysis on that?
					for (unsigned int i = 0; i < runtime_ptr->weight_outputs_count; ++i) {
						required_outputs[required_outputs_count] = runtime_ptr->weight_output_indices[i];
						++required_outputs_count;
					}
				}

				// TODO Instead of filling this ourselves, can we leave this to the graph runtime?
				// Because currently our logic seems redundant and more complicated, since we also have to not request
				// those outputs later if any other output isn't uniform. Instead, the graph runtime can figure out
				// that stuff is constant.
				bool single_texture_is_uniform = false;
				if (runtime_ptr->single_texture_output_index != -1 && !sdf_is_air) {
					const math::Interval index_range =
							cache.state.get_range(runtime_ptr->single_texture_output_buffer_index);
					if (index_range.is_single_value()) {
						// Make sure other indices are different so the weights associated with them don't override the
						// first index's weight
						const int index = int(index_range.min);
						const uint8_t other_index = (index == 0 ? 1 : 0);
						const uint16_t encoded_indices =
								encode_indices_to_packed_u16(index, other_index, other_index, other_index);
						out_buffer.fill_area(encoded_indices, rmin, rmax, VoxelBufferInternal::CHANNEL_INDICES);
						out_buffer.fill_area(0x000f, rmin, rmax, VoxelBufferInternal::CHANNEL_WEIGHTS);
						single_texture_is_uniform = true;
					} else {
						required_outputs[required_outputs_count] = runtime_ptr->single_texture_output_index;
						++required_outputs_count;
					}
				}

				if (required_outputs_count == 0) {
					// We found all we need with range analysis, no need to calculate per voxel.
					continue;
				}

				// At least one channel needs per-voxel computation.

				if (_use_optimized_execution_map) {
					runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map,
							to_span_const(required_outputs, required_outputs_count), false);
				}

				{
					unsigned int i = 0;
					for (int rz = rmin.z, gz = gmin.z; rz < rmax.z; ++rz, gz += stride) {
						for (int rx = rmin.x, gx = gmin.x; rx < rmax.x; ++rx, gx += stride) {
							x_cache[i] = gx;
							z_cache[i] = gz;
							++i;
						}
					}
				}

				for (int ry = rmin.y, gy = gmin.y; ry < rmax.y; ++ry, gy += stride) {
					ZN_PROFILE_SCOPE_NAMED("Full slice");

					y_cache.fill(gy);

					if (input_sdf_full_cache.size() != 0) {
						// Copy input SDF using expected coordinate convention.
						// VoxelBuffer is ZXY, but the graph runs in YXZ.
						unsigned int i = 0;
						for (int rz = rmin.z; rz < rmax.z; ++rz) {
							for (int rx = rmin.x; rx < rmax.x; ++rx) {
								const unsigned int loc = Vector3iUtil::get_zxy_index(rx, ry, rz, bs.x, bs.y);
								input_sdf_slice_cache[i] = input_sdf_full_cache[loc];
								++i;
							}
						}
					}

					// Full query (unless using execution map)
					{
						QueryInputs query_inputs(*runtime_ptr, x_cache, y_cache, z_cache, input_sdf_slice_cache);
						runtime.generate_set(cache.state, query_inputs.get(), _use_xz_caching && ry != rmin.y,
								_use_optimized_execution_map ? &cache.optimized_execution_map : nullptr);
					}

					if (sdf_output_buffer_index != -1
							// If SDF was found uniform, we already filled the results, and we did not require it in the
							// query. But if another output exists, a query might still run (so we end up at this
							// `if`), and we should not gather SDF results. Otherwise it would overwrite the slice with
							// garbage since SDF was skipped.
							// The same logic goes for other outputs: if they aren't in the query, we must not fill
							// them.
							&& !sdf_is_uniform) {
						const pg::Runtime::Buffer &sdf_buffer = cache.state.get_buffer(sdf_output_buffer_index);
						fill_zx_sdf_slice(
								sdf_buffer, out_buffer, sdf_channel, sdf_channel_depth, sdf_scale, rmin, rmax, ry);
					}

					if (type_output_buffer_index != -1 && !type_is_uniform) {
						const pg::Runtime::Buffer &type_buffer = cache.state.get_buffer(type_output_buffer_index);
						fill_zx_integer_slice(
								type_buffer, out_buffer, type_channel, type_channel_depth, rmin, rmax, ry);
					}

					if (runtime_ptr->single_texture_output_index != -1 && !single_texture_is_uniform) {
						gather_indices_and_weights_from_single_texture(runtime_ptr->single_texture_output_buffer_index,
								cache.state, rmin, rmax, ry, out_buffer);
					}

					if (runtime_ptr->weight_outputs_count > 0) {
						gather_indices_and_weights(
								to_span_const(runtime_ptr->weight_outputs, runtime_ptr->weight_outputs_count),
								cache.state, rmin, rmax, ry, out_buffer, spare_texture_indices);
					}
				}
			}
		}
	}

	out_buffer.compress_uniform_channels();

	// This is different from finding out that the buffer is uniform.
	// This really means we predicted SDF will never cross zero in this area, no matter how precise we get.
	// Relying on the block's uniform channels would bring up false positives due to LOD aliasing.
	const bool all_sdf_is_uniform = all_sdf_is_air || all_sdf_is_matter;
	if (all_sdf_is_uniform) {
		// TODO If voxel texure weights are used, octree compression might be a bit more complicated.
		// For now we only look at SDF but if texture weights are used and the player digs a bit inside terrain,
		// they will find it's all default weights.
		// Possible workarounds:
		// - Only do it for air
		// - Also take indices and weights into account, but may lead to way less compression, or none, for stuff
		// that
		//   essentially isnt showing up until dug out
		// - Invoke generator to produce LOD0 blocks somehow, but main thread could stall
		result.max_lod_hint = true;
	}

	return result;
}

static bool has_output_type(
		const pg::Runtime &runtime, const ProgramGraph &graph, pg::VoxelGraphFunction::NodeTypeID node_type_id) {
	for (unsigned int other_output_index = 0; other_output_index < runtime.get_output_count(); ++other_output_index) {
		const pg::Runtime::OutputInfo output = runtime.get_output_info(other_output_index);
		const ProgramGraph::Node &node = graph.get_node(output.node_id);
		if (node.type_id == pg::VoxelGraphFunction::NODE_OUTPUT_WEIGHT) {
			return true;
		}
	}
	return false;
}

pg::CompilationResult VoxelGeneratorGraph::compile(bool debug) {
	// This is a specialized compilation. We use VoxelGraphFunction for a more precise use case, which is to generate
	// voxels based on 3D coordinates and some other attributes. So we expect specific inputs and outputs to be used.

	ERR_FAIL_COND_V(_main_function.is_null(), pg::CompilationResult::make_error("Main function not defined"));

	const int64_t time_before = Time::get_singleton()->get_ticks_usec();

	std::shared_ptr<Runtime> r = make_shared_instance<Runtime>();

	// We usually expect X, Y, Z and SDF inputs. Custom inputs are not supported.
	_main_function->auto_pick_inputs_and_outputs();
	Span<const pg::VoxelGraphFunction::Port> input_defs = _main_function->get_input_definitions();
	for (unsigned int input_index = 0; input_index < input_defs.size(); ++input_index) {
		const pg::VoxelGraphFunction::Port &port = input_defs[input_index];
		switch (port.type) {
			case pg::VoxelGraphFunction::NODE_INPUT_X:
				r->x_input_index = input_index;
				break;
			case pg::VoxelGraphFunction::NODE_INPUT_Y:
				r->y_input_index = input_index;
				break;
			case pg::VoxelGraphFunction::NODE_INPUT_Z:
				r->z_input_index = input_index;
				break;
			case pg::VoxelGraphFunction::NODE_INPUT_SDF:
				r->sdf_input_index = input_index;
				break;
			case pg::VoxelGraphFunction::NODE_CUSTOM_INPUT:
				return pg::CompilationResult::make_error("Custom inputs are not allowed in main generator graphs.");
			default:
				return pg::CompilationResult::make_error("Unsupported input. Bug?");
		}
	}

	// Core compilation
	pg::Runtime &runtime = r->runtime;
	const pg::CompilationResult result = runtime.compile(**_main_function, debug);

	if (!result.success) {
		return result;
	}

	const ProgramGraph &source_graph = _main_function->get_graph();

	// Extra steps
	for (unsigned int output_index = 0; output_index < runtime.get_output_count(); ++output_index) {
		const pg::Runtime::OutputInfo output = runtime.get_output_info(output_index);
		const ProgramGraph::Node &node = source_graph.get_node(output.node_id);

		// TODO Allow specifying max count in pg::NodeTypeDB so we can make some of these checks more generic
		switch (node.type_id) {
			case pg::VoxelGraphFunction::NODE_OUTPUT_SDF:
				if (r->sdf_output_buffer_index != -1) {
					pg::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple SDF outputs are not supported");
					error.node_id = output.node_id;
					return error;
				} else {
					r->sdf_output_index = output_index;
					r->sdf_output_buffer_index = output.buffer_address;
				}
				break;

			case pg::VoxelGraphFunction::NODE_OUTPUT_WEIGHT: {
				if (r->weight_outputs_count >= r->weight_outputs.size()) {
					pg::CompilationResult error;
					error.success = false;
					error.message = String(ZN_TTR("Cannot use more than {0} weight outputs"))
											.format(varray(r->weight_outputs.size()));
					error.node_id = output.node_id;
					return error;
				}
				CRASH_COND(node.params.size() == 0);
				const int layer_index = node.params[0];
				if (layer_index < 0) {
					// Should not be allowed by the UI, but who knows
					pg::CompilationResult error;
					error.success = false;
					error.message = String(ZN_TTR("Cannot use negative layer index in weight output"));
					error.node_id = output.node_id;
					return error;
				}
				if (layer_index >= static_cast<int>(r->weight_outputs.size())) {
					pg::CompilationResult error;
					error.success = false;
					error.message =
							String(ZN_TTR("Weight layers cannot exceed {0}")).format(varray(r->weight_outputs.size()));
					error.node_id = output.node_id;
					return error;
				}
				for (unsigned int i = 0; i < r->weight_outputs_count; ++i) {
					const WeightOutput &wo = r->weight_outputs[i];
					if (static_cast<int>(wo.layer_index) == layer_index) {
						pg::CompilationResult error;
						error.success = false;
						error.message =
								String(ZN_TTR("Only one weight output node can use layer index {0}, found duplicate"))
										.format(varray(layer_index));
						error.node_id = output.node_id;
						return error;
					}
				}
				WeightOutput &new_weight_output = r->weight_outputs[r->weight_outputs_count];
				new_weight_output.layer_index = layer_index;
				new_weight_output.output_buffer_index = output.buffer_address;
				r->weight_output_indices[r->weight_outputs_count] = output_index;
				++r->weight_outputs_count;
			} break;

			case pg::VoxelGraphFunction::NODE_OUTPUT_TYPE:
				if (r->type_output_buffer_index != -1) {
					pg::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple TYPE outputs are not supported");
					error.node_id = output.node_id;
					return error;
				} else {
					r->type_output_index = output_index;
					r->type_output_buffer_index = output.buffer_address;
				}
				break;

			case pg::VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE:
				if (r->single_texture_output_buffer_index != -1) {
					pg::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple TYPE outputs are not supported");
					error.node_id = output.node_id;
					return error;
				}
				if (has_output_type(runtime, source_graph, pg::VoxelGraphFunction::NODE_OUTPUT_WEIGHT)) {
					pg::CompilationResult error;
					error.success = false;
					error.message =
							ZN_TTR("Using both OutputWeight nodes and an OutputSingleTexture node is not allowed");
					error.node_id = output.node_id;
					return error;
				}
				r->single_texture_output_index = output_index;
				r->single_texture_output_buffer_index = output.buffer_address;
				break;

			default:
				break;
		}
	}

	if (r->sdf_output_buffer_index == -1 && r->type_output_buffer_index == -1) {
		pg::CompilationResult error;
		error.success = false;
		error.message = String(ZN_TTR("An SDF or TYPE output is required for the graph to be valid."));
		return error;
	}

	// Sort output weights by layer index, for determinism. Could be exploited for optimization too?
	{
		struct WeightOutputComparer {
			inline bool operator()(const WeightOutput &a, const WeightOutput &b) const {
				return a.layer_index < b.layer_index;
			}
		};
		SortArray<WeightOutput, WeightOutputComparer> sorter;
		CRASH_COND(r->weight_outputs_count >= r->weight_outputs.size());
		sorter.sort(r->weight_outputs.data(), r->weight_outputs_count);
	}

	// Calculate spare indices
	{
		FixedArray<bool, 16> used_indices_map;
		FixedArray<uint8_t, 4> spare_indices;
		fill(used_indices_map, false);
		for (unsigned int i = 0; i < r->weight_outputs.size(); ++i) {
			used_indices_map[r->weight_outputs[i].layer_index] = true;
		}
		unsigned int spare_indices_count = 0;
		for (unsigned int i = 0; i < used_indices_map.size() && spare_indices_count < 4; ++i) {
			if (used_indices_map[i] == false) {
				spare_indices[spare_indices_count] = i;
				++spare_indices_count;
			}
		}
		// debug_check_texture_indices(spare_indices);
		ERR_FAIL_COND_V(spare_indices_count != 4, pg::CompilationResult());
		r->spare_texture_indices = spare_indices;
	}

	// Store valid result
	RWLockWrite wlock(_runtime_lock);
	_runtime = r;

	const int64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(format("Voxel graph compiled in {} us", time_spent));

	if (result.success) {
		invalidate_shaders();
	}

	return result;
}

// This is an external API which involves locking so better not use this internally
bool VoxelGeneratorGraph::is_good() const {
	RWLockRead rlock(_runtime_lock);
	return _runtime != nullptr;
}

// TODO Rename `generate_series`
void VoxelGeneratorGraph::generate_set(Span<float> in_x, Span<float> in_y, Span<float> in_z) {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND(_runtime == nullptr);
	Cache &cache = get_tls_cache();
	pg::Runtime &runtime = _runtime->runtime;

	Span<float> in_sdf;
	if (_runtime->sdf_input_index != -1) {
		// Support graphs having an SDF input, give it default values
		cache.input_sdf_full_cache.resize(in_x.size());
		in_sdf = to_span(cache.input_sdf_full_cache);
		in_sdf.fill(0.f);
	}

	QueryInputs inputs(*_runtime, in_x, in_y, in_z, in_sdf);

	runtime.prepare_state(cache.state, in_x.size(), false);
	runtime.generate_set(cache.state, inputs.get(), false, nullptr);
	// Note, when generating SDF, we don't scale it because the return values are uncompressed floats. Scale only
	// matters if we are storing it inside 16-bit or 8-bit VoxelBuffer.
}

void VoxelGeneratorGraph::generate_series(Span<float> in_x, Span<float> in_y, Span<float> in_z, Span<float> in_sdf) {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND(_runtime == nullptr);
	Cache &cache = get_tls_cache();
	pg::Runtime &runtime = _runtime->runtime;

	QueryInputs inputs(*_runtime, in_x, in_y, in_z, in_sdf);

	runtime.prepare_state(cache.state, in_x.size(), false);
	runtime.generate_set(cache.state, inputs.get(), false, nullptr);
	// Note, when generating SDF, we don't scale it because the return values are uncompressed floats. Scale only
	// matters if we are storing it inside 16-bit or 8-bit VoxelBuffer.
}

void VoxelGeneratorGraph::generate_series(Span<const float> positions_x, Span<const float> positions_y,
		Span<const float> positions_z, unsigned int channel, Span<float> out_values, Vector3f min_pos,
		Vector3f max_pos) {
	std::shared_ptr<Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	if (runtime_ptr == nullptr) {
		return;
	}

	int buffer_index;
	float defval = 0.f;
	switch (channel) {
		case VoxelBufferInternal::CHANNEL_SDF:
			buffer_index = runtime_ptr->sdf_output_buffer_index;
			defval = 1.f;
			break;
		case VoxelBufferInternal::CHANNEL_TYPE:
			buffer_index = runtime_ptr->type_output_buffer_index;
			break;
		default:
			ZN_PRINT_ERROR("Unexpected channel");
			return;
	}

	if (buffer_index == -1) {
		// The graph does not define such output
		out_values.fill(defval);
		return;
	}

	{
		// The implementation cannot guarantee constness at compile time, but it should not modifiy the data either way
		float *ptr_x = const_cast<float *>(positions_x.data());
		float *ptr_y = const_cast<float *>(positions_y.data());
		float *ptr_z = const_cast<float *>(positions_z.data());
		generate_set(Span<float>(ptr_x, positions_x.size()), Span<float>(ptr_y, positions_y.size()),
				Span<float>(ptr_z, positions_z.size()));
	}

	const pg::Runtime::Buffer &buffer = get_tls_cache().state.get_buffer(buffer_index);
	memcpy(out_values.data(), buffer.data, sizeof(float) * out_values.size());
}

const pg::Runtime::State &VoxelGeneratorGraph::get_last_state_from_current_thread() {
	return get_tls_cache().state;
}

Span<const uint32_t> VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread() {
	return to_span_const(get_tls_cache().optimized_execution_map.debug_nodes);
}

bool VoxelGeneratorGraph::try_get_output_port_address(ProgramGraph::PortLocation port, uint32_t &out_address) const {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND_V(_runtime == nullptr, false);
	uint16_t addr;
	const bool res = _runtime->runtime.try_get_output_port_address(port, addr);
	out_address = addr;
	return res;
}

int VoxelGeneratorGraph::get_sdf_output_port_address() const {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND_V(_runtime == nullptr, -1);
	return _runtime->sdf_output_buffer_index;
}

inline Vector3 get_3d_pos_from_panorama_uv(Vector2 uv) {
	const float xa = -Math_TAU * uv.x - Math_PI;
	const float ya = -Math_PI * (uv.y - 0.5f);
	const float y = Math::sin(ya);
	const float ca = Math::cos(ya);
	const float x = Math::cos(xa) * ca;
	const float z = Math::sin(xa) * ca;
	return Vector3(x, y, z);
}

// Subdivides a rectangle in square chunks and runs a function on each of them.
// The ref is important to allow re-using functors.
template <typename F>
inline void for_chunks_2d(int w, int h, int chunk_size, F &f) {
	const int chunks_x = w / chunk_size;
	const int chunks_y = h / chunk_size;

	const int last_chunk_width = w % chunk_size;
	const int last_chunk_height = h % chunk_size;

	for (int cy = 0; cy < chunks_y; ++cy) {
		int ry = cy * chunk_size;
		int rh = ry + chunk_size > h ? last_chunk_height : chunk_size;

		for (int cx = 0; cx < chunks_x; ++cx) {
			int rx = cx * chunk_size;
			int rw = ry + chunk_size > w ? last_chunk_width : chunk_size;

			f(rx, ry, rw, rh);
		}
	}
}

void VoxelGeneratorGraph::bake_sphere_bumpmap(Ref<Image> im, float ref_radius, float sdf_min, float sdf_max) {
	ERR_FAIL_COND(im.is_null());

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	ERR_FAIL_COND(runtime_ptr == nullptr);
	ERR_FAIL_COND_MSG(runtime_ptr->sdf_output_buffer_index == -1, "This function only works with an SDF output.");

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> in_sdf_cache;
		Image &im;
		const Runtime &runtime_wrapper;
		pg::Runtime::State &state;
		const float ref_radius;
		const float sdf_min;
		const float sdf_max;

		ProcessChunk(pg::Runtime::State &p_state, const Runtime &p_runtime, float p_ref_radius, float p_sdf_min,
				float p_sdf_max, Image &p_im) :
				im(p_im),
				runtime_wrapper(p_runtime),
				state(p_state),
				ref_radius(p_ref_radius),
				sdf_min(p_sdf_min),
				sdf_max(p_sdf_max) {}

		void operator()(int x0, int y0, int width, int height) {
			ZN_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			runtime_wrapper.runtime.prepare_state(state, area, false);

			const Vector2 suv =
					Vector2(1.f / static_cast<float>(im.get_width()), 1.f / static_cast<float>(im.get_height()));

			const float nr = 1.f / (sdf_max - sdf_min);

			const int xmax = x0 + width;
			const int ymax = y0 + height;

			unsigned int i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const Vector2 uv = suv * Vector2(ix, iy);
					const Vector3 p = get_3d_pos_from_panorama_uv(uv) * ref_radius;
					x_coords[i] = p.x;
					y_coords[i] = p.y;
					z_coords[i] = p.z;
					++i;
				}
			}

			// Support graphs having an SDF input, give it default values
			Span<float> in_sdf;
			if (runtime_wrapper.sdf_input_index != -1) {
				in_sdf_cache.resize(x_coords.size());
				in_sdf = to_span(in_sdf_cache);
				in_sdf.fill(0.f);
			}

			QueryInputs inputs(runtime_wrapper, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf);

			runtime_wrapper.runtime.generate_set(state, inputs.get(), false, nullptr);
			const pg::Runtime::Buffer &buffer = state.get_buffer(runtime_wrapper.sdf_output_buffer_index);

			// Calculate final pixels
			// TODO Optimize: could convert to buffer directly?
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float sdf = buffer.data[i];
					const float nh = (-sdf - sdf_min) * nr;
					im.set_pixel(ix, iy, Color(nh, nh, nh));
					++i;
				}
			}
		}
	};

	Cache &cache = get_tls_cache();

	ProcessChunk pc(cache.state, *runtime_ptr, ref_radius, sdf_min, sdf_max, **im);
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

// If this generator is used to produce a planet, specifically using a spherical heightmap approach,
// then this function can be used to bake a map of the surface.
// Such maps can be used by shaders to sharpen the details of the planet when seen from far away.
void VoxelGeneratorGraph::bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(im.is_null());

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	ERR_FAIL_COND(runtime_ptr == nullptr);
	ERR_FAIL_COND_MSG(runtime_ptr->sdf_output_buffer_index == -1, "This function only works with an SDF output.");

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> sdf_values_p; // TODO Could be used at the same time to get bump?
		std::vector<float> sdf_values_px;
		std::vector<float> sdf_values_py;
		std::vector<float> in_sdf_cache;
		Image &im;
		const Runtime &runtime_wrapper;
		pg::Runtime::State &state;
		const float strength;
		const float ref_radius;

		ProcessChunk(pg::Runtime::State &p_state, Image &p_im, const Runtime &p_runtime_wrapper, float p_strength,
				float p_ref_radius) :
				im(p_im),
				runtime_wrapper(p_runtime_wrapper),
				state(p_state),
				strength(p_strength),
				ref_radius(p_ref_radius) {}

		void operator()(int x0, int y0, int width, int height) {
			ZN_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			sdf_values_p.resize(area);
			sdf_values_px.resize(area);
			sdf_values_py.resize(area);
			runtime_wrapper.runtime.prepare_state(state, area, false);

			const float ns = 2.f / strength;

			const Vector2 suv =
					Vector2(1.f / static_cast<float>(im.get_width()), 1.f / static_cast<float>(im.get_height()));

			const Vector2 normal_step = 0.5f * Vector2(1.f, 1.f) / im.get_size();
			const Vector2 normal_step_x = Vector2(normal_step.x, 0.f);
			const Vector2 normal_step_y = Vector2(0.f, normal_step.y);

			const int xmax = x0 + width;
			const int ymax = y0 + height;

			const pg::Runtime::Buffer &sdf_output_buffer = state.get_buffer(runtime_wrapper.sdf_output_buffer_index);

			// Support graphs having an SDF input, give it default values
			Span<float> in_sdf;
			if (runtime_wrapper.sdf_input_index != -1) {
				in_sdf_cache.resize(x_coords.size());
				in_sdf = to_span(in_sdf_cache);
				in_sdf.fill(0.f);
			}

			// TODO instead of using 3 separate queries, interleave triplets of positions into a single array?

			// Get heights
			unsigned int i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const Vector2 uv = suv * Vector2(ix, iy);
					const Vector3 p = get_3d_pos_from_panorama_uv(uv) * ref_radius;
					x_coords[i] = p.x;
					y_coords[i] = p.y;
					z_coords[i] = p.z;
					++i;
				}
			}

			QueryInputs inputs(runtime_wrapper, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf);

			// TODO Perform range analysis on the range of coordinates, it might still yield performance benefits
			runtime_wrapper.runtime.generate_set(state, inputs.get(), false, nullptr);
			CRASH_COND(sdf_values_p.size() != sdf_output_buffer.size);
			memcpy(sdf_values_p.data(), sdf_output_buffer.data, sdf_values_p.size() * sizeof(float));

			// Get neighbors along X
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const Vector2 uv = suv * Vector2(ix, iy);
					const Vector3 p = get_3d_pos_from_panorama_uv(uv + normal_step_x) * ref_radius;
					x_coords[i] = p.x;
					y_coords[i] = p.y;
					z_coords[i] = p.z;
					++i;
				}
			}
			runtime_wrapper.runtime.generate_set(state, inputs.get(), false, nullptr);
			CRASH_COND(sdf_values_px.size() != sdf_output_buffer.size);
			memcpy(sdf_values_px.data(), sdf_output_buffer.data, sdf_values_px.size() * sizeof(float));

			// Get neighbors along Y
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const Vector2 uv = suv * Vector2(ix, iy);
					const Vector3 p = get_3d_pos_from_panorama_uv(uv + normal_step_y) * ref_radius;
					x_coords[i] = p.x;
					y_coords[i] = p.y;
					z_coords[i] = p.z;
					++i;
				}
			}
			runtime_wrapper.runtime.generate_set(state, inputs.get(), false, nullptr);
			CRASH_COND(sdf_values_py.size() != sdf_output_buffer.size);
			memcpy(sdf_values_py.data(), sdf_output_buffer.data, sdf_values_py.size() * sizeof(float));

			// TODO This is probably invalid due to the distortion, may need to use another approach.
			// Compute the 3D normal from gradient, then project it?

			// Calculate final pixels
			// TODO Optimize: convert into buffer directly?
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float h = sdf_values_p[i];
					const float h_px = sdf_values_px[i];
					const float h_py = sdf_values_py[i];
					++i;
					const Vector3 normal = Vector3(h_px - h, ns, h_py - h).normalized();
					const Color en(0.5f * normal.x + 0.5f, -0.5f * normal.z + 0.5f, 0.5f * normal.y + 0.5f);
					im.set_pixel(ix, iy, en);
				}
			}
		}
	};

	Cache &cache = get_tls_cache();

	// The default for strength is 1.f
	const float e = 0.001f;
	if (strength > -e && strength < e) {
		if (strength > 0.f) {
			strength = e;
		} else {
			strength = -e;
		}
	}

	ProcessChunk pc(cache.state, **im, *runtime_ptr, strength, ref_radius);
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

bool VoxelGeneratorGraph::get_shader_source(ShaderSourceData &out_data) const {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(_main_function.is_null(), false);
	const ProgramGraph &graph = _main_function->get_graph();

	std::string code_utf8;
	std::vector<pg::ShaderParameter> params;
	pg::CompilationResult result =
			pg::generate_shader(graph, _main_function->get_input_definitions(), code_utf8, params);

	ERR_FAIL_COND_V_MSG(!result.success, "", result.message);

	if (params.size() > 0) {
		out_data.parameters.reserve(params.size());
		for (pg::ShaderParameter &p : params) {
			out_data.parameters.push_back(ShaderParameter{ String::utf8(p.name.c_str()), std::move(p.resource) });
		}
	}

	out_data.glsl = String(code_utf8.c_str());
	return true;
}

VoxelSingleValue VoxelGeneratorGraph::generate_single(Vector3i position, unsigned int channel) {
	// TODO Support other channels
	VoxelSingleValue v;
	v.i = 0;
	if (channel != VoxelBufferInternal::CHANNEL_SDF) {
		return v;
	}
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, v);
	// TODO Allow return values from other outputs
	if (runtime_ptr->sdf_output_buffer_index == -1) {
		return v;
	}

	QueryInputs<float> inputs(*runtime_ptr, position.x, position.y, position.z, 0.f);

	Cache &cache = get_tls_cache();
	const pg::Runtime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, 1, false);
	runtime.generate_single(cache.state, inputs.get(), nullptr);
	const pg::Runtime::Buffer &buffer = cache.state.get_buffer(runtime_ptr->sdf_output_buffer_index);
	ERR_FAIL_COND_V(buffer.size == 0, v);
	ERR_FAIL_COND_V(buffer.data == nullptr, v);
	v.f = buffer.data[0];
	return v;
}

// Note, this wrapper may not be used for main generation tasks.
// It is mostly used as a debug tool.
math::Interval VoxelGeneratorGraph::debug_analyze_range(
		Vector3i min_pos, Vector3i max_pos, bool optimize_execution_map) const {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, math::Interval::from_single_value(0.f));
	Cache &cache = get_tls_cache();
	const pg::Runtime &runtime = runtime_ptr->runtime;

	QueryInputs query_inputs(*runtime_ptr, math::Interval(min_pos.x, max_pos.x), math::Interval(min_pos.y, max_pos.y),
			math::Interval(min_pos.z, max_pos.z), math::Interval());

	// Note, buffer size is irrelevant here, because range analysis doesn't use buffers
	runtime.prepare_state(cache.state, 1, false);
	runtime.analyze_range(cache.state, query_inputs.get());
	if (optimize_execution_map) {
		runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map, true);
	}
	// TODO Change return value to allow checking other outputs
	if (runtime_ptr->sdf_output_buffer_index != -1) {
		return cache.state.get_range(runtime_ptr->sdf_output_buffer_index);
	}
	return math::Interval();
}

/*Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instantiate();

	d->_graph.copy_from(_graph, p_subresources);
	d->register_subresources();
	// Program not copied, as it may contain pointers to the resources we are duplicating

	return d;
}*/

// Debug land

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel(
		bool singular, std::vector<NodeProfilingInfo> *node_profiling_info) {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V_MSG(runtime_ptr == nullptr, 0.f, "The graph hasn't been compiled yet");
	const pg::Runtime &runtime = runtime_ptr->runtime;

	const uint32_t cube_size = 16;
	const uint32_t cube_count = 250;
	// const uint32_t cube_size = 100;
	// const uint32_t cube_count = 1;
	const uint32_t voxel_count = cube_size * cube_size * cube_size * cube_count;
	ProfilingClock profiling_clock;
	uint64_t total_elapsed_us = 0;

	Cache &cache = get_tls_cache();

	if (singular) {
		runtime.prepare_state(cache.state, 1, false);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t z = 0; z < cube_size; ++z) {
				for (uint32_t y = 0; y < cube_size; ++y) {
					for (uint32_t x = 0; x < cube_size; ++x) {
						QueryInputs<float> inputs(*runtime_ptr, x, y, z, 0.f);
						runtime.generate_single(cache.state, inputs.get(), nullptr);
					}
				}
			}

			total_elapsed_us += profiling_clock.restart();
		}

	} else {
		const unsigned int cube_volume = cube_size * cube_size * cube_size;
		std::vector<float> src_x;
		std::vector<float> src_y;
		std::vector<float> src_z;
		std::vector<float> src_sdf;
		src_x.resize(cube_volume);
		src_y.resize(cube_volume);
		src_z.resize(cube_volume);
		src_sdf.resize(cube_volume);
		Span<float> sx = to_span(src_x);
		Span<float> sy = to_span(src_y);
		Span<float> sz = to_span(src_z);
		Span<float> ssdf = to_span(src_sdf);

		QueryInputs inputs(*runtime_ptr, sx, sy, sz, ssdf);

		const bool per_node_profiling = node_profiling_info != nullptr;
		runtime.prepare_state(cache.state, sx.size(), per_node_profiling);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t y = 0; y < cube_size; ++y) {
				runtime.generate_set(cache.state, inputs.get(), false, nullptr);
			}

			total_elapsed_us += profiling_clock.restart();
		}

		if (per_node_profiling) {
			const pg::Runtime::ExecutionMap &execution_map = runtime.get_default_execution_map();
			node_profiling_info->resize(execution_map.debug_nodes.size());
			for (unsigned int i = 0; i < node_profiling_info->size(); ++i) {
				NodeProfilingInfo &info = (*node_profiling_info)[i];
				info.node_id = execution_map.debug_nodes[i];
				info.microseconds = cache.state.get_execution_time(i);
			}
		}
	}

	const float us = static_cast<double>(total_elapsed_us) / voxel_count;
	return us;
}

// This may be used as template when creating new graphs
void VoxelGeneratorGraph::load_plane_preset() {
	using namespace pg;
	clear();

	/*
	 *     X
	 *
	 *     Y --- SdfPlane --- OutputSDF
	 *
	 *     Z
	 */

	ERR_FAIL_COND(_main_function.is_null());
	pg::VoxelGraphFunction &g = **_main_function;

	const Vector2 k(40, 50);

	/*const uint32_t n_x = */ g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(11, 1) * k); // 1
	const uint32_t n_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(11, 3) * k); // 2
	/*const uint32_t n_z = */ g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(11, 5) * k); // 3
	const uint32_t n_o = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(18, 3) * k); // 4
	const uint32_t n_plane = g.create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2(14, 3) * k); // 5

	g.add_connection(n_y, 0, n_plane, 0);
	g.add_connection(n_plane, 0, n_o, 0);
}

void VoxelGeneratorGraph::debug_load_waves_preset() {
	using namespace pg;
	clear();
	// This is mostly for testing

	ERR_FAIL_COND(_main_function.is_null());
	VoxelGraphFunction &g = **_main_function;

	const Vector2 k(35, 50);

	const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(11, 1) * k); // 1
	const uint32_t n_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(37, 1) * k); // 2
	const uint32_t n_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(11, 5) * k); // 3
	const uint32_t n_o = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(45, 3) * k); // 4
	const uint32_t n_sin0 = g.create_node(VoxelGraphFunction::NODE_SIN, Vector2(23, 1) * k); // 5
	const uint32_t n_sin1 = g.create_node(VoxelGraphFunction::NODE_SIN, Vector2(23, 5) * k); // 6
	const uint32_t n_add = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2(27, 3) * k); // 7
	const uint32_t n_mul0 = g.create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2(17, 1) * k); // 8
	const uint32_t n_mul1 = g.create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2(17, 5) * k); // 9
	const uint32_t n_mul2 = g.create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2(33, 3) * k); // 10
	const uint32_t n_c0 = g.create_node(VoxelGraphFunction::NODE_CONSTANT, Vector2(14, 3) * k); // 11
	const uint32_t n_c1 = g.create_node(VoxelGraphFunction::NODE_CONSTANT, Vector2(30, 5) * k); // 12
	const uint32_t n_sub = g.create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2(39, 3) * k); // 13

	g.set_node_param(n_c0, 0, 1.f / 20.f);
	g.set_node_param(n_c1, 0, 10.f);

	/*
	 *    X --- * --- sin           Y
	 *         /         \           \
	 *       1/20         + --- * --- - --- O
	 *         \         /     /
	 *    Z --- * --- sin    10.0
	 */

	g.add_connection(n_x, 0, n_mul0, 0);
	g.add_connection(n_z, 0, n_mul1, 0);
	g.add_connection(n_c0, 0, n_mul0, 1);
	g.add_connection(n_c0, 0, n_mul1, 1);
	g.add_connection(n_mul0, 0, n_sin0, 0);
	g.add_connection(n_mul1, 0, n_sin1, 0);
	g.add_connection(n_sin0, 0, n_add, 0);
	g.add_connection(n_sin1, 0, n_add, 1);
	g.add_connection(n_add, 0, n_mul2, 0);
	g.add_connection(n_c1, 0, n_mul2, 1);
	g.add_connection(n_y, 0, n_sub, 0);
	g.add_connection(n_mul2, 0, n_sub, 1);
	g.add_connection(n_sub, 0, n_o, 0);
}

#ifdef TOOLS_ENABLED

void VoxelGeneratorGraph::get_configuration_warnings(PackedStringArray &out_warnings) const {
	if (_main_function.is_valid()) {
		_main_function->get_configuration_warnings(out_warnings);

		if (_main_function->get_nodes_count() == 0) {
			// Making a `String` explicitely because in GDExtension `get_class_static` is a `const char*`
			out_warnings.append(String(VoxelGeneratorGraph::get_class_static()) + " is empty.");
			return;
		}
	}

	if (!is_good()) {
		// Making a `String` explicitely because in GDExtension `get_class_static` is a `const char*`
		out_warnings.append(String(VoxelGeneratorGraph::get_class_static()) + " contains errors.");
		return;
	}
}

#endif // TOOLS_ENABLED

float VoxelGeneratorGraph::_b_generate_single(Vector3 pos) {
	return generate_single(math::floor_to_int(pos), VoxelBufferInternal::CHANNEL_SDF).f;
}

Vector2 VoxelGeneratorGraph::_b_debug_analyze_range(Vector3 min_pos, Vector3 max_pos) const {
	ERR_FAIL_COND_V(min_pos.x > max_pos.x, Vector2());
	ERR_FAIL_COND_V(min_pos.y > max_pos.y, Vector2());
	ERR_FAIL_COND_V(min_pos.z > max_pos.z, Vector2());
	const math::Interval r = debug_analyze_range(math::floor_to_int(min_pos), math::floor_to_int(max_pos), false);
	return Vector2(r.min, r.max);
}

Dictionary VoxelGeneratorGraph::_b_compile() {
	pg::CompilationResult res = compile(false);
	Dictionary d;
	d["success"] = res.success;
	if (!res.success) {
		d["message"] = res.message;
		d["node_id"] = res.node_id;
	}
	return d;
}

float VoxelGeneratorGraph::_b_debug_measure_microseconds_per_voxel(bool singular) {
	return debug_measure_microseconds_per_voxel(singular, nullptr);
}

void VoxelGeneratorGraph::_on_subresource_changed() {
	emit_changed();
}

Dictionary VoxelGeneratorGraph::get_graph_as_variant_data() const {
	ERR_FAIL_COND_V(_main_function.is_null(), Dictionary());
	return _main_function->get_graph_as_variant_data();
}

void VoxelGeneratorGraph::load_graph_from_variant_data(Dictionary data) {
	ERR_FAIL_COND(_main_function.is_null());
	if (_main_function->load_graph_from_variant_data(data)) {
		// It's possible to auto-compile on load because `graph_data` is the only property set by the loader,
		// which is enough to have all information we need
		compile(Engine::get_singleton()->is_editor_hint());
	}
}

void VoxelGeneratorGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGeneratorGraph::clear);
	ClassDB::bind_method(D_METHOD("get_main_function"), &VoxelGeneratorGraph::get_main_function);

	ClassDB::bind_method(D_METHOD("set_sdf_clip_threshold", "threshold"), &VoxelGeneratorGraph::set_sdf_clip_threshold);
	ClassDB::bind_method(D_METHOD("get_sdf_clip_threshold"), &VoxelGeneratorGraph::get_sdf_clip_threshold);

	ClassDB::bind_method(
			D_METHOD("is_using_optimized_execution_map"), &VoxelGeneratorGraph::is_using_optimized_execution_map);
	ClassDB::bind_method(
			D_METHOD("set_use_optimized_execution_map", "use"), &VoxelGeneratorGraph::set_use_optimized_execution_map);

	ClassDB::bind_method(D_METHOD("set_use_subdivision", "use"), &VoxelGeneratorGraph::set_use_subdivision);
	ClassDB::bind_method(D_METHOD("is_using_subdivision"), &VoxelGeneratorGraph::is_using_subdivision);

	ClassDB::bind_method(D_METHOD("set_subdivision_size", "size"), &VoxelGeneratorGraph::set_subdivision_size);
	ClassDB::bind_method(D_METHOD("get_subdivision_size"), &VoxelGeneratorGraph::get_subdivision_size);

	ClassDB::bind_method(
			D_METHOD("set_debug_clipped_blocks", "enabled"), &VoxelGeneratorGraph::set_debug_clipped_blocks);
	ClassDB::bind_method(D_METHOD("is_debug_clipped_blocks"), &VoxelGeneratorGraph::is_debug_clipped_blocks);

	ClassDB::bind_method(D_METHOD("set_use_xz_caching", "enabled"), &VoxelGeneratorGraph::set_use_xz_caching);
	ClassDB::bind_method(D_METHOD("is_using_xz_caching"), &VoxelGeneratorGraph::is_using_xz_caching);

	ClassDB::bind_method(D_METHOD("compile"), &VoxelGeneratorGraph::_b_compile);

	// ClassDB::bind_method(D_METHOD("generate_single"), &VoxelGeneratorGraph::_b_generate_single);
	ClassDB::bind_method(
			D_METHOD("debug_analyze_range", "min_pos", "max_pos"), &VoxelGeneratorGraph::_b_debug_analyze_range);

	ClassDB::bind_method(D_METHOD("bake_sphere_bumpmap", "im", "ref_radius", "sdf_min", "sdf_max"),
			&VoxelGeneratorGraph::bake_sphere_bumpmap);
	ClassDB::bind_method(D_METHOD("bake_sphere_normalmap", "im", "ref_radius", "strength"),
			&VoxelGeneratorGraph::bake_sphere_normalmap);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &VoxelGeneratorGraph::debug_load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel", "use_singular_queries"),
			&VoxelGeneratorGraph::_b_debug_measure_microseconds_per_voxel);

	// Still present here for compatibility
	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &VoxelGeneratorGraph::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &VoxelGeneratorGraph::get_graph_as_variant_data);

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGeneratorGraph::_on_subresource_changed);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

	ADD_GROUP("Performance Tuning", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "sdf_clip_threshold"), "set_sdf_clip_threshold", "get_sdf_clip_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_optimized_execution_map"), "set_use_optimized_execution_map",
			"is_using_optimized_execution_map");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_subdivision"), "set_use_subdivision", "is_using_subdivision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivision_size"), "set_subdivision_size", "get_subdivision_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_xz_caching"), "set_use_xz_caching", "is_using_xz_caching");
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "debug_block_clipping"), "set_debug_clipped_blocks", "is_debug_clipped_blocks");

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_NAME_CHANGED, PropertyInfo(Variant::INT, "node_id")));
}

} // namespace zylann::voxel
