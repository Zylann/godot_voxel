#include "voxel_generator_graph.h"
#include "../../constants/voxel_string_names.h"
#include "../../storage/mixel4.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/godot/core/string.h"
#include "../../util/hash_funcs.h"
#include "../../util/io/log.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string/expression_parser.h"
#include "../../util/string/format.h"
#include "node_type_db.h"
#include "voxel_graph_function.h"

namespace zylann::voxel {

const char *VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

VoxelGeneratorGraph::VoxelGeneratorGraph() {
	_main_function.instantiate();
	_main_function->connect(
			VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelGeneratorGraph::_on_subresource_changed)
	);
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
		mask |= (1 << VoxelBuffer::CHANNEL_SDF);
	}
	if (runtime_ptr->type_output_index != -1) {
		mask |= (1 << VoxelBuffer::CHANNEL_TYPE);
	}
	if (runtime_ptr->weight_outputs_count > 0 || runtime_ptr->single_texture_output_index != -1) {
		switch (_texture_mode) {
			case TEXTURE_MODE_MIXEL4:
				mask |= (1 << VoxelBuffer::CHANNEL_INDICES);
				mask |= (1 << VoxelBuffer::CHANNEL_WEIGHTS);
				break;

			case TEXTURE_MODE_SINGLE:
				mask |= (1 << VoxelBuffer::CHANNEL_INDICES);
				break;

			default:
				ZN_PRINT_ERROR_ONCE("Unknown texture mode");
				break;
		}
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

void VoxelGeneratorGraph::set_texture_mode(const TextureMode mode) {
	ZN_ASSERT_RETURN(mode >= 0 && mode < TEXTURE_MODE_COUNT);
	_texture_mode = mode;
}

VoxelGeneratorGraph::TextureMode VoxelGeneratorGraph::get_texture_mode() const {
	return _texture_mode;
}

// TODO Optimization: generating indices and weights on every voxel of a block might be avoidable
// Instead, we could only generate them near zero-crossings, because this is where materials will be seen.
// The problem is that it's harder to manage at the moment, to support edited blocks and LOD...
void VoxelGeneratorGraph::gather_texturing_data_from_weight_outputs(
		Span<const WeightOutput> weight_outputs,
		const pg::Runtime::State &state,
		const Vector3i rmin,
		const Vector3i rmax,
		const int ry,
		VoxelBuffer &out_voxel_buffer,
		const FixedArray<uint8_t, 4> spare_indices,
		const VoxelGeneratorGraph::TextureMode mode
) {
	ZN_PROFILE_SCOPE();

	// TODO Optimization: exclude up-front outputs that are known to be zero?
	// So we choose the cases below based on non-zero outputs instead of total output count

	// It is unfortunately not practical to handle all combinations of single-value/array-of-values for every
	// layer, so for now we try to workaround that
	struct WeightBufferArray {
		FixedArray<Span<const float>, 16> array;

		inline float get_weight(unsigned int output_index, size_t value_index) const {
			Span<const float> s = array[output_index];
#ifdef DEV_ENABLED
			// That span can either have size 1 (means all values would be the same) or have regular buffer
			// size. We use min() to quickly access that without branching, but that could hide actual OOB bugs, so
			// make sure they are detected
			ZN_ASSERT(s.size() > 0 && (value_index < s.size() || s.size() == 1));
#endif
			const float weight = s[math::min(value_index, s.size() - 1)];
			return weight;
		}
	};

	// TODO This should not be necessary!
	// We could use a pointer to the `min` member of intervals inside State, however Interval uses `real_t`, which
	// breaks our code in 64-bit float builds of Godot. We should refactor Interval so everything can line up.
	FixedArray<float, 16> constants;

	// TODO Could maybe put this part outside?
	WeightBufferArray buffers;
	const unsigned int buffers_count = weight_outputs.size();
	for (unsigned int oi = 0; oi < buffers_count; ++oi) {
		const WeightOutput &info = weight_outputs[oi];
		const math::Interval &range = state.get_range_const_ref(info.output_buffer_index);

		if (range.is_single_value()) {
			// The weight is locally constant (or compile-time constant).
			// We can't use the usual buffer because if we use optimized execution mapping, they won't be filled by any
			// operation and would contain garbage
			constants[oi] = range.min;
			buffers.array[oi] = Span<const float>(&constants[oi], 1);
			// buffers.array[oi] = Span<const float>(&range.min, 1);
		} else {
			const pg::Runtime::Buffer &buffer = state.get_buffer(info.output_buffer_index);
			buffers.array[oi] = Span<const float>(buffer.data, buffer.size);
		}
	}

	switch (mode) {
		case TEXTURE_MODE_MIXEL4: {
			if (buffers_count <= 4) {
				// Pick all results and fill with spare indices to keep semantic
				size_t value_index = 0;
				for (int rz = rmin.z; rz < rmax.z; ++rz) {
					for (int rx = rmin.x; rx < rmax.x; ++rx) {
						FixedArray<uint8_t, 4> weights;
						FixedArray<uint8_t, 4> indices = spare_indices;
						fill(weights, uint8_t(0));
						for (unsigned int oi = 0; oi < buffers_count; ++oi) {
							const float weight = buffers.get_weight(oi, value_index);
							// TODO Optimization: weight output nodes could already multiply by 255 and clamp afterward
							// so we would not need to do it here
							weights[oi] = math::clamp(weight * 255.f, 0.f, 255.f);
							indices[oi] = weight_outputs[oi].layer_index;
						}
						mixel4::debug_check_texture_indices(indices);
						const uint16_t encoded_indices =
								mixel4::encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
						const uint16_t encoded_weights = mixel4::encode_weights_to_packed_u16_lossy(
								weights[0], weights[1], weights[2], weights[3]
						);
						// TODO Flatten this further?
						out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
						out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBuffer::CHANNEL_WEIGHTS);
						++value_index;
					}
				}

			} else if (buffers_count == 4) {
				// Pick all results
				size_t value_index = 0;
				for (int rz = rmin.z; rz < rmax.z; ++rz) {
					for (int rx = rmin.x; rx < rmax.x; ++rx) {
						FixedArray<uint8_t, 4> weights;
						FixedArray<uint8_t, 4> indices;
						for (unsigned int oi = 0; oi < buffers_count; ++oi) {
							const float weight = buffers.get_weight(oi, value_index);
							weights[oi] = math::clamp(weight * 255.f, 0.f, 255.f);
							indices[oi] = weight_outputs[oi].layer_index;
						}
						const uint16_t encoded_indices =
								mixel4::encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
						const uint16_t encoded_weights = mixel4::encode_weights_to_packed_u16_lossy(
								weights[0], weights[1], weights[2], weights[3]
						);
						// TODO Flatten this further?
						out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
						out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBuffer::CHANNEL_WEIGHTS);
						++value_index;
					}
				}

			} else {
				// More weights than we can have per voxel. Will need to pick most represented weights
				const float pivot = 1.f / 5.f;
				size_t value_index = 0;
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
							const float weight = buffers.get_weight(oi, value_index);

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
								mixel4::encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
						const uint16_t encoded_weights = mixel4::encode_weights_to_packed_u16_lossy(
								weights[0], weights[1], weights[2], weights[3]
						);
						// TODO Flatten this further?
						out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
						out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBuffer::CHANNEL_WEIGHTS);
						++value_index;
					}
				}
			}
		} break;

		case TEXTURE_MODE_SINGLE: {
			size_t value_index = 0;
			for (int rz = rmin.z; rz < rmax.z; ++rz) {
				for (int rx = rmin.x; rx < rmax.x; ++rx) {
					// Take highest weight as the index
					float highest_weight = 0.f;
					uint8_t selected_index = 0;
					for (unsigned int oi = 0; oi < buffers_count; ++oi) {
						const float weight = buffers.get_weight(oi, value_index);
						if (weight > highest_weight) {
							highest_weight = weight;
							selected_index = weight_outputs[oi].layer_index;
						}
					}
					out_voxel_buffer.set_voxel(selected_index, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
					++value_index;
				}
			}
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown texture mode");
			break;
	}
}

namespace {

void fill_texturing_data_from_single_texture_index(
		VoxelBuffer &out_buffer,
		const int index,
		const Vector3i rmin,
		const Vector3i rmax,
		const VoxelGeneratorGraph::TextureMode mode
) {
	switch (mode) {
		case VoxelGeneratorGraph::TEXTURE_MODE_MIXEL4: {
			const uint8_t cindex = math::clamp(index, 0, 15);
			// Make sure other indices are different so the weights associated with them don't
			// override the first index's weight
			const uint16_t encoded_indices = mixel4::make_encoded_indices_for_single_texture(cindex);
			const uint16_t encoded_weights = mixel4::make_encoded_weights_for_single_texture();
			out_buffer.fill_area(encoded_indices, rmin, rmax, VoxelBuffer::CHANNEL_INDICES);
			out_buffer.fill_area(encoded_weights, rmin, rmax, VoxelBuffer::CHANNEL_WEIGHTS);
		} break;

		case VoxelGeneratorGraph::TEXTURE_MODE_SINGLE: {
			const uint8_t cindex = math::clamp(index, 0, 255);
			out_buffer.fill_area(cindex, rmin, rmax, VoxelBuffer::CHANNEL_INDICES);
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown texture mode");
			break;
	}
}

void fill_texturing_data_from_single_texture_index(
		VoxelBuffer &out_buffer,
		const int index,
		const VoxelGeneratorGraph::TextureMode mode
) {
	switch (mode) {
		case VoxelGeneratorGraph::TEXTURE_MODE_MIXEL4: {
			const uint8_t cindex = math::clamp(index, 0, 15);
			// Make sure other indices are different so the weights associated with them don't
			// override the first index's weight
			const uint16_t encoded_indices = mixel4::make_encoded_indices_for_single_texture(cindex);
			const uint16_t encoded_weights = mixel4::make_encoded_weights_for_single_texture();
			out_buffer.fill(encoded_indices, VoxelBuffer::CHANNEL_INDICES);
			out_buffer.fill(encoded_weights, VoxelBuffer::CHANNEL_WEIGHTS);
		} break;

		case VoxelGeneratorGraph::TEXTURE_MODE_SINGLE: {
			const uint8_t cindex = math::clamp(index, 0, 255);
			out_buffer.fill(cindex, VoxelBuffer::CHANNEL_INDICES);
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown texture mode");
			break;
	}
}

void gather_texturing_data_from_single_texture_output(
		const unsigned int output_buffer_index,
		const pg::Runtime::State &state,
		const Vector3i rmin,
		const Vector3i rmax,
		const int ry,
		VoxelBuffer &out_voxel_buffer,
		const VoxelGeneratorGraph::TextureMode mode
) {
	ZN_PROFILE_SCOPE();

	const pg::Runtime::Buffer &buffer = state.get_buffer(output_buffer_index);
	Span<const float> buffer_data = Span<const float>(buffer.data, buffer.size);

	switch (mode) {
		case VoxelGeneratorGraph::TEXTURE_MODE_MIXEL4: {
			// TODO Should not really be here, but may work. Left here for now so all code for this is in one place
			const uint16_t encoded_weights = mixel4::make_encoded_weights_for_single_texture();
			out_voxel_buffer.clear_channel(VoxelBuffer::CHANNEL_WEIGHTS, encoded_weights);

			unsigned int value_index = 0;
			for (int rz = rmin.z; rz < rmax.z; ++rz) {
				for (int rx = rmin.x; rx < rmax.x; ++rx) {
					const uint8_t index = math::clamp(int(Math::round(buffer_data[value_index])), 0, 15);
					const uint16_t encoded_indices = mixel4::make_encoded_indices_for_single_texture(index);
					out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
					++value_index;
				}
			}
		} break;

		case VoxelGeneratorGraph::TEXTURE_MODE_SINGLE: {
			unsigned int value_index = 0;
			for (int rz = rmin.z; rz < rmax.z; ++rz) {
				for (int rx = rmin.x; rx < rmax.x; ++rx) {
					const uint8_t index = math::clamp(int(Math::round(buffer_data[value_index])), 0, 255);
					out_voxel_buffer.set_voxel(index, rx, ry, rz, VoxelBuffer::CHANNEL_INDICES);
					++value_index;
				}
			}
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown texture mode");
			break;
	}
}

template <typename F, typename Data_T>
void fill_zx_sdf_slice(
		Span<Data_T> channel_data,
		float sdf_scale,
		Vector3i rmin,
		Vector3i rmax,
		int ry,
		int x_stride,
		const float *src_data,
		Vector3i buffer_size,
		F convert_func
) {
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

void fill_zx_sdf_slice(
		const pg::Runtime::Buffer &sdf_buffer,
		VoxelBuffer &out_buffer,
		unsigned int channel,
		VoxelBuffer::Depth channel_depth,
		float sdf_scale,
		Vector3i rmin,
		Vector3i rmax,
		int ry
) {
	ZN_PROFILE_SCOPE_NAMED("Copy SDF to block");

	if (out_buffer.get_channel_compression(channel) != VoxelBuffer::COMPRESSION_NONE) {
		out_buffer.decompress_channel(channel);
	}
	Span<uint8_t> channel_bytes;
	ERR_FAIL_COND(!out_buffer.get_channel_as_bytes(channel, channel_bytes));
	const Vector3i buffer_size = out_buffer.get_size();
	const unsigned int x_stride = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 0), buffer_size) -
			Vector3iUtil::get_zxy_index(Vector3i(0, 0, 0), buffer_size);

	switch (channel_depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			fill_zx_sdf_slice(
					channel_bytes, sdf_scale, rmin, rmax, ry, x_stride, sdf_buffer.data, buffer_size, snorm_to_s8
			);
			break;

		case VoxelBuffer::DEPTH_16_BIT:
			fill_zx_sdf_slice(
					channel_bytes.reinterpret_cast_to<uint16_t>(),
					sdf_scale,
					rmin,
					rmax,
					ry,
					x_stride,
					sdf_buffer.data,
					buffer_size,
					snorm_to_s16
			);
			break;

		case VoxelBuffer::DEPTH_32_BIT:
			fill_zx_sdf_slice(
					channel_bytes.reinterpret_cast_to<float>(),
					sdf_scale,
					rmin,
					rmax,
					ry,
					x_stride,
					sdf_buffer.data,
					buffer_size,
					[](float v) { return v; }
			);
			break;

		case VoxelBuffer::DEPTH_64_BIT:
			fill_zx_sdf_slice(
					channel_bytes.reinterpret_cast_to<double>(),
					sdf_scale,
					rmin,
					rmax,
					ry,
					x_stride,
					sdf_buffer.data,
					buffer_size,
					[](double v) { return v; }
			);
			break;

		default:
			ZN_PRINT_ERROR(format("Unknown depth {}", channel_depth));
			break;
	}
}

template <typename F, typename Data_T>
void fill_zx_integer_slice(
		Span<Data_T> channel_data,
		Vector3i rmin,
		Vector3i rmax,
		int ry,
		int x_stride,
		const float *src_data,
		Vector3i buffer_size
) {
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

void fill_zx_integer_slice(
		const pg::Runtime::Buffer &src_buffer,
		VoxelBuffer &out_buffer,
		unsigned int channel,
		VoxelBuffer::Depth channel_depth,
		Vector3i rmin,
		Vector3i rmax,
		int ry
) {
	ZN_PROFILE_SCOPE_NAMED("Copy integer data to block");

	if (out_buffer.get_channel_compression(channel) != VoxelBuffer::COMPRESSION_NONE) {
		out_buffer.decompress_channel(channel);
	}
	Span<uint8_t> channel_bytes;
	ERR_FAIL_COND(!out_buffer.get_channel_as_bytes(channel, channel_bytes));
	const Vector3i buffer_size = out_buffer.get_size();
	const unsigned int x_stride = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 0), buffer_size) -
			Vector3iUtil::get_zxy_index(Vector3i(0, 0, 0), buffer_size);

	switch (channel_depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			fill_zx_integer_slice<uint8_t>(channel_bytes, rmin, rmax, ry, x_stride, src_buffer.data, buffer_size);
			break;

		case VoxelBuffer::DEPTH_16_BIT:
			fill_zx_integer_slice<uint16_t>(
					channel_bytes.reinterpret_cast_to<uint16_t>(),
					rmin,
					rmax,
					ry,
					x_stride,
					src_buffer.data,
					buffer_size
			);
			break;

		case VoxelBuffer::DEPTH_32_BIT:
			fill_zx_integer_slice<uint32_t>(
					channel_bytes.reinterpret_cast_to<uint32_t>(),
					rmin,
					rmax,
					ry,
					x_stride,
					src_buffer.data,
					buffer_size
			);
			break;

		case VoxelBuffer::DEPTH_64_BIT:
			fill_zx_integer_slice<uint64_t>(
					channel_bytes.reinterpret_cast_to<uint64_t>(),
					rmin,
					rmax,
					ry,
					x_stride,
					src_buffer.data,
					buffer_size
			);
			break;

		default:
			ZN_PRINT_ERROR(format("Unknown depth {}", channel_depth));
			break;
	}
}

} // namespace

VoxelGenerator::Result VoxelGeneratorGraph::generate_block(VoxelGenerator::VoxelQueryData input) {
	std::shared_ptr<Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	Result result;

	if (runtime_ptr == nullptr) {
		return result;
	}

	VoxelBuffer &out_buffer = input.voxel_buffer;

#ifdef TOOLS_ENABLED
	switch (_texture_mode) {
		case TEXTURE_MODE_MIXEL4: {
			const VoxelBuffer::Depth indices_depth = out_buffer.get_channel_depth(VoxelBuffer::CHANNEL_INDICES);
			if (indices_depth != VoxelBuffer::DEPTH_16_BIT) {
				ZN_PRINT_ERROR_ONCE(format(
						"The Indices channel is set to {} bits, but 16 bits are necessary to use the Mixel4 texturing "
						"mode.",
						VoxelBuffer::get_depth_byte_count(indices_depth)
				));
			}
			const VoxelBuffer::Depth weights_depth = out_buffer.get_channel_depth(VoxelBuffer::CHANNEL_WEIGHTS);
			if (weights_depth != VoxelBuffer::DEPTH_16_BIT) {
				ZN_PRINT_ERROR_ONCE(format(
						"The Weights channel is set to {} bits, but 16 bits are necessary to use the Mixel4 texturing "
						"mode.",
						VoxelBuffer::get_depth_byte_count(weights_depth)
				));
			}
		} break;

		case TEXTURE_MODE_SINGLE: {
			const VoxelBuffer::Depth indices_depth = out_buffer.get_channel_depth(VoxelBuffer::CHANNEL_INDICES);
			if (indices_depth != VoxelBuffer::DEPTH_8_BIT) {
				ZN_PRINT_WARNING_ONCE(
						format("The Indices channel is set to {} bits, but only 8 bits are required to use the Single "
							   "texturing mode.",
							   VoxelBuffer::get_depth_byte_count(indices_depth))
				);
			}
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unknown texture mode");
			break;
	}
#endif

	const Vector3i bs = out_buffer.get_size();
	const VoxelBuffer::ChannelId sdf_channel = VoxelBuffer::CHANNEL_SDF;
	const Vector3i origin = input.origin_in_voxels;

	// TODO This may be shared across the module
	// Storing voxels is lossy on some depth configurations. They use normalized SDF,
	// so we must scale the values to make better use of the offered resolution
	const VoxelBuffer::Depth sdf_channel_depth = out_buffer.get_channel_depth(sdf_channel);
	const float sdf_scale = VoxelBuffer::get_sdf_quantization_scale(sdf_channel_depth);

	const VoxelBuffer::ChannelId type_channel = VoxelBuffer::CHANNEL_TYPE;
	const VoxelBuffer::Depth type_channel_depth = out_buffer.get_channel_depth(type_channel);

	const int stride = 1 << input.lod;

	// Clip threshold must be higher for higher lod indexes because distances for one sampled voxel are also larger
	const float clip_threshold = _sdf_clip_threshold * stride;

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

	const float air_sdf = _debug_clipped_blocks ? constants::SDF_FAR_INSIDE : constants::SDF_FAR_OUTSIDE;
	const float matter_sdf = _debug_clipped_blocks ? constants::SDF_FAR_OUTSIDE : constants::SDF_FAR_INSIDE;

	FixedArray<uint8_t, 4> spare_texture_indices = runtime_ptr->spare_texture_indices;
	const int sdf_output_buffer_index = runtime_ptr->sdf_output_buffer_index;
	const int type_output_buffer_index = runtime_ptr->type_output_buffer_index;

	bool all_sdf_is_air = (sdf_output_buffer_index != -1) && (type_output_buffer_index == -1);
	bool all_sdf_is_matter = all_sdf_is_air;

	math::Interval sdf_input_range;
	Span<float> input_sdf_full_cache;
	Span<float> input_sdf_slice_cache;
	if (runtime_ptr->sdf_input_index != -1) {
		ZN_PROFILE_SCOPE();
		cache.input_sdf_slice_cache.resize(slice_buffer_size);
		input_sdf_slice_cache = to_span(cache.input_sdf_slice_cache);

		const size_t volume = Vector3iUtil::get_volume_u64(bs);
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
					QueryInputs<math::Interval> range_inputs(
							*runtime_ptr,
							math::Interval(gmin.x, gmax.x),
							math::Interval(gmin.y, gmax.y),
							math::Interval(gmin.z, gmax.z),
							sdf_input_range
					);
					runtime.analyze_range(cache.state, range_inputs.get());
				}

				SmallVector<unsigned int, pg::Runtime::MAX_OUTPUTS> required_outputs;

				bool sdf_is_air = true;
				bool sdf_is_uniform = true;
				if (sdf_output_buffer_index != -1) {
					const math::Interval sdf_range = cache.state.get_range(sdf_output_buffer_index);
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
						required_outputs.push_back(runtime_ptr->sdf_output_index);
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
						required_outputs.push_back(runtime_ptr->type_output_index);
					}
				}

				if (runtime_ptr->weight_outputs_count > 0 && !sdf_is_air) {
					// We can skip this when SDF is air because there won't be any matter to give a texture to
					// TODO Range analysis on that?
					// Not easy to do that from here, they would have to ALL be locally constant in order to use a
					// short-circuit...
					for (unsigned int i = 0; i < runtime_ptr->weight_outputs_count; ++i) {
						required_outputs.push_back(runtime_ptr->weight_output_indices[i]);
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
						single_texture_is_uniform = true;
						fill_texturing_data_from_single_texture_index(
								out_buffer, static_cast<int>(index_range.min), rmin, rmax, _texture_mode
						);
					} else {
						required_outputs.push_back(runtime_ptr->single_texture_output_index);
					}
				}

				if (required_outputs.size() == 0) {
					// We found all we need with range analysis, no need to calculate per voxel.
					continue;
				}

				// At least one channel needs per-voxel computation.

				if (_use_optimized_execution_map) {
					runtime.generate_optimized_execution_map(
							cache.state, cache.optimized_execution_map, to_span(required_outputs), false
					);
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
						QueryInputs<Span<const float>> query_inputs(
								*runtime_ptr, x_cache, y_cache, z_cache, input_sdf_slice_cache
						);
						runtime.generate_set(
								cache.state,
								query_inputs.get(),
								_use_xz_caching && ry != rmin.y,
								_use_optimized_execution_map ? &cache.optimized_execution_map : nullptr
						);
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
								sdf_buffer, out_buffer, sdf_channel, sdf_channel_depth, sdf_scale, rmin, rmax, ry
						);
					}

					if (type_output_buffer_index != -1 && !type_is_uniform) {
						const pg::Runtime::Buffer &type_buffer = cache.state.get_buffer(type_output_buffer_index);
						fill_zx_integer_slice(
								type_buffer, out_buffer, type_channel, type_channel_depth, rmin, rmax, ry
						);
					}

					if (runtime_ptr->single_texture_output_index != -1 && !single_texture_is_uniform) {
						gather_texturing_data_from_single_texture_output(
								runtime_ptr->single_texture_output_buffer_index,
								cache.state,
								rmin,
								rmax,
								ry,
								out_buffer,
								_texture_mode
						);
					}

					if (runtime_ptr->weight_outputs_count > 0) {
						gather_texturing_data_from_weight_outputs(
								to_span_const(runtime_ptr->weight_outputs, runtime_ptr->weight_outputs_count),
								cache.state,
								rmin,
								rmax,
								ry,
								out_buffer,
								spare_texture_indices,
								_texture_mode
						);
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

bool VoxelGeneratorGraph::generate_broad_block(VoxelGenerator::VoxelQueryData input) {
	// This is a reduced version of what `generate_block` does already, so it can be used before scheduling GPU work.
	// If range analysis and SDF clipping finds that we don't need to generate the full block, we can get away with the
	// broad result. If any channel cannot be determined this way, we have to perform full generation.

	std::shared_ptr<Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	if (runtime_ptr == nullptr) {
		return true;
	}

	VoxelBuffer &out_buffer = input.voxel_buffer;

	const Vector3i bs = out_buffer.get_size();
	const VoxelBuffer::ChannelId sdf_channel = VoxelBuffer::CHANNEL_SDF;
	const Vector3i origin = input.origin_in_voxels;

	const VoxelBuffer::ChannelId type_channel = VoxelBuffer::CHANNEL_TYPE;

	const int stride = 1 << input.lod;

	// Clip threshold must be higher for higher lod indexes because distances for one sampled voxel are also larger
	const float clip_threshold = _sdf_clip_threshold * stride;

	Cache &cache = get_tls_cache();

	// Slice is on the Y axis
	pg::Runtime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, 1, false);

	const float air_sdf = _debug_clipped_blocks ? constants::SDF_FAR_INSIDE : constants::SDF_FAR_OUTSIDE;
	const float matter_sdf = _debug_clipped_blocks ? constants::SDF_FAR_OUTSIDE : constants::SDF_FAR_INSIDE;

	const int sdf_output_buffer_index = runtime_ptr->sdf_output_buffer_index;
	const int type_output_buffer_index = runtime_ptr->type_output_buffer_index;

	const Vector3i gmin = origin;
	const Vector3i gmax = origin + (bs << input.lod);

	{
		QueryInputs<math::Interval> range_inputs(
				*runtime_ptr,
				math::Interval(gmin.x, gmax.x),
				math::Interval(gmin.y, gmax.y),
				math::Interval(gmin.z, gmax.z),
				math::Interval()
		);
		runtime.analyze_range(cache.state, range_inputs.get());
	}

	bool sdf_is_air = true;
	if (sdf_output_buffer_index != -1) {
		const math::Interval sdf_range = cache.state.get_range(sdf_output_buffer_index);

		if (sdf_range.min > clip_threshold && sdf_range.max > clip_threshold) {
			out_buffer.fill_f(air_sdf, sdf_channel);
			sdf_is_air = true;

		} else if (sdf_range.min < -clip_threshold && sdf_range.max < -clip_threshold) {
			out_buffer.fill_f(matter_sdf, sdf_channel);
			sdf_is_air = false;

		} else if (sdf_range.is_single_value()) {
			out_buffer.fill_f(sdf_range.min, sdf_channel);
			sdf_is_air = sdf_range.min > 0.f;

		} else {
			// SDF is not uniform, we'll need to compute it per voxel
			return false;
		}
	}

	if (type_output_buffer_index != -1) {
		const math::Interval type_range = cache.state.get_range(type_output_buffer_index);
		if (type_range.is_single_value()) {
			out_buffer.fill(int(type_range.min), type_channel);
		} else {
			// Types are not uniform, we'll need to compute them per voxel
			return false;
		}
	}

	// We can skip this when SDF is air because there won't be any matter to give a texture to
	if (runtime_ptr->weight_outputs_count > 0 && !sdf_is_air) {
		return false;
		// TODO Range analysis on that?
	}

	// TODO Instead of filling this ourselves, can we leave this to the graph runtime?
	// Because currently our logic seems redundant and more complicated, since we also have to not request
	// those outputs later if any other output isn't uniform. Instead, the graph runtime can figure out
	// that stuff is constant.
	if (runtime_ptr->single_texture_output_index != -1 && !sdf_is_air) {
		const math::Interval index_range = cache.state.get_range(runtime_ptr->single_texture_output_buffer_index);
		if (index_range.is_single_value()) {
			fill_texturing_data_from_single_texture_index(out_buffer, int(index_range.min), _texture_mode);
		} else {
			return false;
		}
	}

	// We found all we need with range analysis, no need to calculate per voxel.
	return true;
}

namespace {

bool has_output_type(
		const pg::Runtime &runtime,
		const ProgramGraph &graph,
		pg::VoxelGraphFunction::NodeTypeID node_type_id
) {
	for (unsigned int other_output_index = 0; other_output_index < runtime.get_output_count(); ++other_output_index) {
		const pg::Runtime::OutputInfo output = runtime.get_output_info(other_output_index);
		const ProgramGraph::Node &node = graph.get_node(output.node_id);
		if (node.type_id == pg::VoxelGraphFunction::NODE_OUTPUT_WEIGHT) {
			return true;
		}
	}
	return false;
}

} // namespace

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

	// TODO This bypasses VoxelGraphFunction's compiling method, we should probably use it now
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
		ZN_ASSERT(r->weight_outputs_count <= r->weight_outputs.size());
		sorter.sort(r->weight_outputs.data(), r->weight_outputs_count);
	}

	// Calculate spare indices
	{
		FixedArray<bool, 16> used_indices_map;
		FixedArray<uint8_t, 4> spare_indices;
		fill(used_indices_map, false);
		unsigned int used_indices_count = 0;
		for (unsigned int i = 0; i < r->weight_outputs.size(); ++i) {
			const unsigned int layer_index = r->weight_outputs[i].layer_index;
			used_indices_count += static_cast<unsigned int>(!used_indices_map[layer_index]);
			used_indices_map[layer_index] = true;
		}
		unsigned int spare_indices_count = 0;
		for (unsigned int i = 0; i < used_indices_map.size() && spare_indices_count < 4; ++i) {
			if (used_indices_map[i] == false) {
				spare_indices[spare_indices_count] = i;
				++spare_indices_count;
			}
		}
		// debug_check_texture_indices(spare_indices);
		const unsigned int expected_spare_indices_count =
				math::min(spare_indices.size(), used_indices_map.size() - used_indices_count);
		ZN_ASSERT_RETURN_V(spare_indices_count == expected_spare_indices_count, pg::CompilationResult());
		r->spare_texture_indices = spare_indices;
	}

	// Store valid result
	RWLockWrite wlock(_runtime_lock);
	_runtime = r;

	const int64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(format("Voxel graph compiled in {} us", time_spent));

#ifdef VOXEL_ENABLE_GPU
	if (result.success) {
		invalidate_shaders();
	}
#endif

	return result;
}

// This is an external API which involves locking so better not use this internally
bool VoxelGeneratorGraph::is_good() const {
	RWLockRead rlock(_runtime_lock);
	return _runtime != nullptr;
}

// TODO Rename `generate_series`
void VoxelGeneratorGraph::generate_set(Span<const float> in_x, Span<const float> in_y, Span<const float> in_z) {
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

	QueryInputs<Span<const float>> inputs(*_runtime, in_x, in_y, in_z, in_sdf);

	runtime.prepare_state(cache.state, in_x.size(), false);
	runtime.generate_set(cache.state, inputs.get(), false, nullptr);
	// Note, when generating SDF, we don't scale it because the return values are uncompressed floats. Scale only
	// matters if we are storing it inside 16-bit or 8-bit VoxelBuffer.
}

void VoxelGeneratorGraph::generate_series(
		Span<const float> in_x,
		Span<const float> in_y,
		Span<const float> in_z,
		Span<const float> in_sdf
) {
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

void VoxelGeneratorGraph::generate_series(
		Span<const float> positions_x,
		Span<const float> positions_y,
		Span<const float> positions_z,
		unsigned int channel,
		Span<float> out_values,
		Vector3f min_pos,
		Vector3f max_pos
) {
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
		case VoxelBuffer::CHANNEL_SDF:
			buffer_index = runtime_ptr->sdf_output_buffer_index;
			defval = constants::SDF_FAR_OUTSIDE;
			break;
		case VoxelBuffer::CHANNEL_TYPE:
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
		generate_set(
				Span<float>(ptr_x, positions_x.size()),
				Span<float>(ptr_y, positions_y.size()),
				Span<float>(ptr_z, positions_z.size())
		);
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

bool VoxelGeneratorGraph::has_texture_output() const {
	RWLockRead rlock(_runtime_lock);
	if (_runtime == nullptr) {
		return false;
	}
	return _runtime->single_texture_output_index != -1 || _runtime->weight_outputs_count > 0;
}

inline Vector3 get_3d_pos_from_panorama_uv(Vector2 uv) {
	const float xa = -math::TAU<real_t> * uv.x - math::PI<real_t>;
	const float ya = math::PI<real_t> * (uv.y - 0.5);
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
		StdVector<float> x_coords;
		StdVector<float> y_coords;
		StdVector<float> z_coords;
		StdVector<float> in_sdf_cache;
		Image &im;
		const Runtime &runtime_wrapper;
		pg::Runtime::State &state;
		const float ref_radius;
		const float sdf_min;
		const float sdf_max;

		ProcessChunk(
				pg::Runtime::State &p_state,
				const Runtime &p_runtime,
				float p_ref_radius,
				float p_sdf_min,
				float p_sdf_max,
				Image &p_im
		) :
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

			QueryInputs<Span<const float>> inputs(
					runtime_wrapper, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf
			);

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
		StdVector<float> x_coords;
		StdVector<float> y_coords;
		StdVector<float> z_coords;
		StdVector<float> sdf_values_p; // TODO Could be used at the same time to get bump?
		StdVector<float> sdf_values_px;
		StdVector<float> sdf_values_py;
		StdVector<float> in_sdf_cache;
		Image &im;
		const Runtime &runtime_wrapper;
		pg::Runtime::State &state;
		const float strength;
		const float ref_radius;

		ProcessChunk(
				pg::Runtime::State &p_state,
				Image &p_im,
				const Runtime &p_runtime_wrapper,
				float p_strength,
				float p_ref_radius
		) :
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

			QueryInputs<Span<const float>> inputs(
					runtime_wrapper, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf
			);

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

struct MaybeRayHit {
	float fraction = 0.f;
	bool valid = false;
};

MaybeRayHit raycast_sdf_approx_batch(
		const Vector3f from,
		const Vector3f to,
		Span<float> x_array,
		Span<float> y_array,
		Span<float> z_array,
		const pg::Runtime &runtime,
		const unsigned int sdf_output_buffer_index,
		pg::Runtime::State &state
) {
	const int step_count = x_array.size();

	for (int i = 0; i < step_count; ++i) {
		const float a = static_cast<float>(i) / static_cast<float>(step_count);
		const Vector3f pos = math::lerp(from, to, a);
		x_array[i] = pos.x;
		y_array[i] = pos.y;
		z_array[i] = pos.z;
	}

	FixedArray<Span<const float>, 3> inputs;
	inputs[0] = x_array;
	inputs[1] = y_array;
	inputs[2] = z_array;

	Span<const Span<const float>> inputs_spans = to_span_const(inputs);

	// Generate a group of values
	runtime.generate_set(state, inputs_spans, false, nullptr);
	const pg::Runtime::Buffer &sd_output_buffer = state.get_buffer(sdf_output_buffer_index);
	ZN_ASSERT(x_array.size() == sd_output_buffer.size);
	Span<const float> out_sd_values(sd_output_buffer.data, sd_output_buffer.size);

	// Analyse values
	unsigned int hit_index = 0;
	for (const float sd : out_sd_values) {
		if (sd < 0.f) {
			break;
		}
		++hit_index;
	}

	if (hit_index < out_sd_values.size()) {
		// Found a hit
		MaybeRayHit hit;
		hit.fraction = static_cast<float>(hit_index) / static_cast<float>(step_count);
		hit.valid = true;
		return hit;
	} else {
		return MaybeRayHit{ 0.f, false };
	}
}

float VoxelGeneratorGraph::raycast_sdf_approx(
		const Vector3 ray_origin,
		const Vector3 ray_end,
		const float stride
) const {
	// Generates values along a ray to find the first one where SDF < 0.0, and return distance along the ray. If no hit
	// is found, returns -1.0.
	// This is an approximation: the error margin of the returned value will be up to the given stride.
	// The longer the distance, the more expensive it is.
	// The lower the stride, the more expensive and accurate it is.

	// Possible optimizations if the ray is vertical:
	// TODO Detect if the graph is purely 2D, and if it is, fast-track to a heightmap single evaluation?
	// TODO Optimize 2D parts of the graph if possible
	// TODO Use range analysis? Could be effective on such a restricted XZ range
	// TODO Allow rational stride, for LOD use cases?

	ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN_V_MSG(stride >= 0.001f, -1.f, "Stride is too low");
	ZN_ASSERT_RETURN_V_MSG(Math::is_finite(stride), -1.f, "Stride is invalid");

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	ZN_ASSERT_RETURN_V(runtime_ptr != nullptr, -1.f);
	ZN_ASSERT_RETURN_V_MSG(
			runtime_ptr->sdf_output_buffer_index != -1, -1.f, "This function only works with an SDF output."
	);
	ZN_ASSERT_RETURN_V_MSG(
			runtime_ptr->sdf_input_index == -1, -1.f, "This function doesn't support graphs that have an SDF input."
	);

	const Vector3 diff = ray_end - ray_origin;
	const real_t distance_sq = diff.length_squared();
	if (distance_sq == 0.0f) {
		return -1.f;
	}

	const real_t distance = Math::sqrt(distance_sq);
	const Vector3 direction = diff / distance;

	const unsigned int step_count = Math::ceil(distance / stride);

	// We'll compute batches of values instead of one by one, as the graph runtime prefers it.
	// It's chosen such that the runtime could use SIMD.
	static const unsigned int BATCH_LENGTH = 16;

	const unsigned int batch_count = math::ceildiv(step_count, BATCH_LENGTH);
	const real_t batch_distance = distance / batch_count;

	FixedArray<float, BATCH_LENGTH> x_array;
	FixedArray<float, BATCH_LENGTH> y_array;
	FixedArray<float, BATCH_LENGTH> z_array;

	Cache &cache = get_tls_cache();

	runtime_ptr->runtime.prepare_state(cache.state, x_array.size(), false);

	FixedArray<Span<float>, 3> inputs;
	inputs[0] = to_span(x_array);
	inputs[1] = to_span(y_array);
	inputs[2] = to_span(z_array);

	for (unsigned int batch_index = 0; batch_index < batch_count; ++batch_index) {
		const real_t from_distance = batch_index * batch_distance;
		const real_t to_distance = math::min(from_distance + batch_distance, distance);

		const Vector3 from = ray_origin + from_distance * direction;
		const Vector3 to = ray_origin + to_distance * direction;

		const MaybeRayHit hit = raycast_sdf_approx_batch(
				to_vec3f(from),
				to_vec3f(to),
				to_span(x_array),
				to_span(y_array),
				to_span(z_array),
				runtime_ptr->runtime,
				runtime_ptr->sdf_output_buffer_index,
				cache.state
		);

		if (hit.valid) {
			return Math::lerp(from_distance, to_distance, static_cast<real_t>(hit.fraction));
			// TODO Affine search by doing hierarchical search? Or leave it to the caller for now?
			// Evaluate voxels between hit.y and hit.y+stride
		}
	}

	return -1.f;
}

void VoxelGeneratorGraph::generate_image_from_sdf(Ref<Image> image, const Transform3D transform, const Vector2 size) {
	ZN_ASSERT_RETURN(image.is_valid());
	ZN_ASSERT_RETURN(!image->is_compressed());

	const Vector2i resolution = image->get_size();
	ZN_ASSERT_RETURN(resolution.x > 0 && resolution.y > 0);

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ZN_ASSERT_RETURN(runtime_ptr != nullptr);
	ZN_ASSERT_RETURN_MSG(runtime_ptr->sdf_output_buffer_index != -1, "This function only works with an SDF output.");
	ZN_ASSERT_RETURN_MSG(
			runtime_ptr->sdf_input_index == -1, "This function doesn't support graphs that have an SDF input."
	);

	StdVector<float> x_buffer;
	StdVector<float> y_buffer;
	StdVector<float> z_buffer;

	// TODO Candidate for temp allocator
	x_buffer.resize(resolution.x);
	y_buffer.resize(resolution.x);
	z_buffer.resize(resolution.x);

	const Vector2 half_size = size * 0.5;

	const pg::Runtime &runtime = runtime_ptr->runtime;

	Cache &cache = get_tls_cache();

	runtime_ptr->runtime.prepare_state(cache.state, x_buffer.size(), false);

	FixedArray<Span<const float>, 3> inputs;
	inputs[0] = to_span(x_buffer);
	inputs[1] = to_span(y_buffer);
	inputs[2] = to_span(z_buffer);

	Span<const Span<const float>> inputs_spans = to_span_const(inputs);

	for (int yi = 0; yi < resolution.y; ++yi) {
		// We add 0.5 to sample at the center of pixels
		const real_t yf = (static_cast<real_t>(resolution.y - yi - 1) + 0.5) / static_cast<real_t>(resolution.y);
		const real_t ly = Math::lerp(-half_size.y, half_size.y, yf);

		for (int xi = 0; xi < resolution.x; ++xi) {
			const real_t xf = (static_cast<real_t>(xi) + 0.5) / static_cast<real_t>(resolution.x);
			const real_t lx = Math::lerp(-half_size.x, half_size.x, xf);

			const Vector3 pos = transform.xform(Vector3(lx, ly, 0.0));

			x_buffer[xi] = pos.x;
			y_buffer[xi] = pos.y;
			z_buffer[xi] = pos.z;
		}

		runtime.generate_set(cache.state, inputs_spans, false, nullptr);
		const pg::Runtime::Buffer &sd_output_buffer = cache.state.get_buffer(runtime_ptr->sdf_output_buffer_index);
		ZN_ASSERT_RETURN(x_buffer.size() == sd_output_buffer.size);
		Span<const float> out_sd_values(sd_output_buffer.data, sd_output_buffer.size);

		for (int xi = 0; xi < resolution.x; ++xi) {
			const float sd = out_sd_values[xi];
			const Color color(sd, sd, sd);
			image->set_pixel(xi, yi, color);
		}
	}
}

#ifdef VOXEL_ENABLE_GPU

bool VoxelGeneratorGraph::get_shader_source(ShaderSourceData &out_data) const {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(_main_function.is_null(), false);
	pg::VoxelGraphFunction::ShaderResult shader_res = _main_function->get_shader_source();

	ERR_FAIL_COND_V_MSG(!shader_res.compilation.success, false, shader_res.compilation.message);

	if (shader_res.params.size() > 0) {
		out_data.parameters.reserve(shader_res.params.size());
		for (pg::ShaderParameter &p : shader_res.params) {
			out_data.parameters.push_back(ShaderParameter{ String::utf8(p.name.c_str()), p.resource });
		}
	}

	out_data.glsl = String(shader_res.code_utf8.c_str());

	for (const pg::ShaderOutput &output : shader_res.outputs) {
		ShaderOutput out;
		switch (output.type) {
			case pg::ShaderOutput::TYPE_SDF:
				out.type = ShaderOutput::TYPE_SDF;
				break;
			case pg::ShaderOutput::TYPE_SINGLE_TEXTURE:
				out.type = ShaderOutput::TYPE_SINGLE_TEXTURE;
				break;
			case pg::ShaderOutput::TYPE_TYPE:
				out.type = ShaderOutput::TYPE_TYPE;
				break;
			default:
				ZN_PRINT_ERROR(format("Unsupported output for voxel generator shader generation ({})", output.type));
				return false;
		}
		out_data.outputs.push_back(out);
	}

	return true;
}

#endif

VoxelSingleValue VoxelGeneratorGraph::generate_single(Vector3i position, unsigned int channel) {
	// This is very slow when used multiple times, so if possible prefer using bulk queries

	struct L {
		static inline float query(const Runtime &runtime_info, const Vector3i pos, const int buffer_index) {
			if (buffer_index == -1) {
				return 0.f;
			}

			QueryInputs<float> inputs(runtime_info, pos.x, pos.y, pos.z, 0.f);

			Cache &cache = get_tls_cache();
			const pg::Runtime &runtime = runtime_info.runtime;
			runtime.prepare_state(cache.state, 1, false);
			runtime.generate_single(cache.state, inputs.get(), nullptr);
			const pg::Runtime::Buffer &buffer = cache.state.get_buffer(buffer_index);
			ERR_FAIL_COND_V(buffer.size == 0, 0.f);
			ERR_FAIL_COND_V(buffer.data == nullptr, 0.f);
			return buffer.data[0];
		}
	};

	VoxelSingleValue v;
	v.i = 0;

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	if (runtime_ptr == nullptr) {
		ZN_PRINT_ERROR_ONCE("No compiled graph available");
		return v;
	}

	switch (channel) {
		case VoxelBuffer::CHANNEL_SDF: {
			v.f = L::query(*runtime_ptr, position, runtime_ptr->sdf_output_buffer_index);
		} break;

		case VoxelBuffer::CHANNEL_TYPE: {
			v.i = L::query(*runtime_ptr, position, runtime_ptr->type_output_buffer_index);
		} break;

		case VoxelBuffer::CHANNEL_INDICES: {
			const float tex_index = L::query(*runtime_ptr, position, runtime_ptr->single_texture_output_buffer_index);
			switch (_texture_mode) {
				case TEXTURE_MODE_MIXEL4:
					v.i = mixel4::make_encoded_indices_for_single_texture(math::clamp(int(tex_index), 0, 15));
					break;
				case TEXTURE_MODE_SINGLE:
					v.i = math::clamp(int(tex_index), 0, 255);
					break;
				default:
					ZN_PRINT_ERROR("Unknown texture mode");
					break;
			}
		} break;

		case VoxelBuffer::CHANNEL_WEIGHTS: {
			v.i = mixel4::make_encoded_weights_for_single_texture();
		} break;

		default:
			// Fallback to slow default
			v = VoxelGenerator::generate_single(position, channel);
			break;
	}

	return v;
}

math::Interval get_range(const Span<const float> values) {
	float minv = values[0];
	float maxv = minv;
	for (const float v : values) {
		minv = math::min(v, minv);
		maxv = math::max(v, maxv);
	}
	return math::Interval(minv, maxv);
}

// Note, this wrapper may not be used for main generation tasks.
// It is mostly used as a debug tool.
math::Interval VoxelGeneratorGraph::debug_analyze_range(
		Vector3i min_pos,
		Vector3i max_pos,
		bool optimize_execution_map
) const {
	ZN_ASSERT_RETURN_V(max_pos.x >= min_pos.x, math::Interval());
	ZN_ASSERT_RETURN_V(max_pos.y >= min_pos.y, math::Interval());
	ZN_ASSERT_RETURN_V(max_pos.z >= min_pos.z, math::Interval());

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, math::Interval::from_single_value(0.f));
	Cache &cache = get_tls_cache();
	const pg::Runtime &runtime = runtime_ptr->runtime;

	QueryInputs query_inputs(
			*runtime_ptr,
			math::Interval(min_pos.x, max_pos.x),
			math::Interval(min_pos.y, max_pos.y),
			math::Interval(min_pos.z, max_pos.z),
			math::Interval()
	);

	// Note, buffer size is irrelevant here, because range analysis doesn't use buffers
	runtime.prepare_state(cache.state, 1, false);
	runtime.analyze_range(cache.state, query_inputs.get());
	if (optimize_execution_map) {
		runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map, true);
	}

#if 0
	const bool range_validation = true;
	if (range_validation) {
		// Actually calculate every value and check if range analysis bounds them.
		// If it does not, then it's a bug.
		// This is not 100% accurate. We would have to calculate every value at every point in space in the area which
		// is not possible, we can only sample a finite number within a grid.

		// TODO Make sure the graph is compiled in debug mode, otherwise buffer lookups won't match
		// TODO Even with debug on, it appears buffers don't really match???

		const Vector3i cube_size = max_pos - min_pos;
		if (cube_size.x > 64 || cube_size.y > 64 || cube_size.z > 64) {
			ZN_PRINT_ERROR("Area too big for range validation");
		} else {
			const int64_t cube_volume = Vector3iUtil::get_volume(cube_size);
			ZN_ASSERT_RETURN_V(cube_volume >= 0, math::Interval());

			StdVector<float> src_x;
			StdVector<float> src_y;
			StdVector<float> src_z;
			StdVector<float> src_sdf;
			src_x.resize(cube_volume);
			src_y.resize(cube_volume);
			src_z.resize(cube_volume);
			src_sdf.resize(cube_volume);
			Span<float> sx = to_span(src_x);
			Span<float> sy = to_span(src_y);
			Span<float> sz = to_span(src_z);
			Span<float> ssdf = to_span(src_sdf);

			{
				unsigned int i = 0;
				for (int z = min_pos.z; z < max_pos.z; ++z) {
					for (int y = min_pos.y; y < max_pos.y; ++y) {
						for (int x = min_pos.x; x < max_pos.x; ++x) {
							src_x[i] = x;
							src_y[i] = y;
							src_z[i] = z;
							++i;
						}
					}
				}
			}

			QueryInputs inputs(*runtime_ptr, sx, sy, sz, ssdf);

			runtime.prepare_state(cache.state, sx.size(), false);
			runtime.generate_set(cache.state, inputs.get(), false, nullptr);

			const pg::Runtime::State &state = get_last_state_from_current_thread();

			_main_function->get_graph().for_each_node_const([this, &state](const ProgramGraph::Node &node) {
				for (uint32_t output_index = 0; output_index < node.outputs.size(); ++output_index) {
					// const ProgramGraph::Port &output_port = node.outputs[output_index];
					const ProgramGraph::PortLocation loc{ node.id, output_index };

					uint32_t address;
					if (!try_get_output_port_address(loc, address)) {
						continue;
					}

					const math::Interval analytic_range = state.get_range(address);

					const pg::Runtime::Buffer &buffer = state.get_buffer(address);

					if (buffer.data == nullptr) {
						if (buffer.is_binding) {
							// Not supported
							continue;
						}
						ZN_PRINT_ERROR("Didn't expect nullptr in buffer data");
						continue;
					}

					const Span<const float> values(buffer.data, buffer.size);
					const math::Interval empiric_range = get_range(values);

					if (!analytic_range.contains(empiric_range)) {
						const String node_name = node.name;
						const pg::NodeType &node_type = pg::NodeTypeDB::get_singleton().get_type(node.type_id);
						ZN_PRINT_WARNING(
								format("Empiric range not included in analytic range. A: {}, E: {}; {} "
									   "output {} instance {} \"{}\"",
									   analytic_range,
									   empiric_range,
									   node_type.name,
									   output_index,
									   node.id,
									   node_name)
						);
					}
				}
			});
		}
	}
#endif

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
		bool singular,
		StdVector<NodeProfilingInfo> *node_profiling_info
) {
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
		StdVector<float> src_x;
		StdVector<float> src_y;
		StdVector<float> src_z;
		StdVector<float> src_sdf;
		src_x.resize(cube_volume);
		src_y.resize(cube_volume);
		src_z.resize(cube_volume);
		src_sdf.resize(cube_volume);
		Span<const float> sx = to_span(src_x);
		Span<const float> sy = to_span(src_y);
		Span<const float> sz = to_span(src_z);
		Span<const float> ssdf = to_span(src_sdf);

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
	return generate_single(math::floor_to_int(pos), VoxelBuffer::CHANNEL_SDF).f;
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
	using Self = VoxelGeneratorGraph;

	ClassDB::bind_method(D_METHOD("clear"), &Self::clear);
	ClassDB::bind_method(D_METHOD("get_main_function"), &Self::get_main_function);

	ClassDB::bind_method(D_METHOD("set_sdf_clip_threshold", "threshold"), &Self::set_sdf_clip_threshold);
	ClassDB::bind_method(D_METHOD("get_sdf_clip_threshold"), &Self::get_sdf_clip_threshold);

	ClassDB::bind_method(D_METHOD("is_using_optimized_execution_map"), &Self::is_using_optimized_execution_map);
	ClassDB::bind_method(D_METHOD("set_use_optimized_execution_map", "use"), &Self::set_use_optimized_execution_map);

	ClassDB::bind_method(D_METHOD("set_use_subdivision", "use"), &Self::set_use_subdivision);
	ClassDB::bind_method(D_METHOD("is_using_subdivision"), &Self::is_using_subdivision);

	ClassDB::bind_method(D_METHOD("set_subdivision_size", "size"), &Self::set_subdivision_size);
	ClassDB::bind_method(D_METHOD("get_subdivision_size"), &Self::get_subdivision_size);

	ClassDB::bind_method(D_METHOD("set_debug_clipped_blocks", "enabled"), &Self::set_debug_clipped_blocks);
	ClassDB::bind_method(D_METHOD("is_debug_clipped_blocks"), &Self::is_debug_clipped_blocks);

	ClassDB::bind_method(D_METHOD("set_use_xz_caching", "enabled"), &Self::set_use_xz_caching);
	ClassDB::bind_method(D_METHOD("is_using_xz_caching"), &Self::is_using_xz_caching);

	ClassDB::bind_method(D_METHOD("set_texture_mode", "mode"), &Self::set_texture_mode);
	ClassDB::bind_method(D_METHOD("get_texture_mode"), &Self::get_texture_mode);

	ClassDB::bind_method(D_METHOD("compile"), &Self::_b_compile);

	// ClassDB::bind_method(D_METHOD("generate_single"), &Self::_b_generate_single);
	ClassDB::bind_method(D_METHOD("debug_analyze_range", "min_pos", "max_pos"), &Self::_b_debug_analyze_range);

	ClassDB::bind_method(
			D_METHOD("bake_sphere_bumpmap", "im", "ref_radius", "sdf_min", "sdf_max"), &Self::bake_sphere_bumpmap
	);
	ClassDB::bind_method(
			D_METHOD("bake_sphere_normalmap", "im", "ref_radius", "strength"), &Self::bake_sphere_normalmap
	);
	ClassDB::bind_method(
			D_METHOD("generate_image_from_sdf", "im", "transform", "size"), &Self::generate_image_from_sdf
	);
	ClassDB::bind_method(D_METHOD("raycast_sdf_approx", "ray_origin", "ray_end", "stride"), &Self::raycast_sdf_approx);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &Self::debug_load_waves_preset);
	ClassDB::bind_method(
			D_METHOD("debug_measure_microseconds_per_voxel", "use_singular_queries"),
			&Self::_b_debug_measure_microseconds_per_voxel
	);

	// Still present here for compatibility
	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &Self::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &Self::get_graph_as_variant_data);

#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_dummy_function"), &Self::_b_dummy_function);
#endif

	ADD_PROPERTY(
			PropertyInfo(
					Variant::DICTIONARY,
					"graph_data",
					PROPERTY_HINT_NONE,
					"",
					PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL
			),
			"_set_graph_data",
			"_get_graph_data"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "texture_mode", PROPERTY_HINT_ENUM, "Mixel4,Single"),
			"set_texture_mode",
			"get_texture_mode"
	);

	ADD_GROUP("Performance Tuning", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "sdf_clip_threshold"), "set_sdf_clip_threshold", "get_sdf_clip_threshold"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "use_optimized_execution_map"),
			"set_use_optimized_execution_map",
			"is_using_optimized_execution_map"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_subdivision"), "set_use_subdivision", "is_using_subdivision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivision_size"), "set_subdivision_size", "get_subdivision_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_xz_caching"), "set_use_xz_caching", "is_using_xz_caching");
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "debug_block_clipping"), "set_debug_clipped_blocks", "is_debug_clipped_blocks"
	);

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_NAME_CHANGED, PropertyInfo(Variant::INT, "node_id")));

	BIND_ENUM_CONSTANT(TEXTURE_MODE_MIXEL4);
	BIND_ENUM_CONSTANT(TEXTURE_MODE_SINGLE);
}

} // namespace zylann::voxel
