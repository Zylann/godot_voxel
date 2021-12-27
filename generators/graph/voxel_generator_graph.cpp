#include "voxel_generator_graph.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "voxel_graph_node_db.h"

#include <core/core_string_names.h>

const char *VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

thread_local VoxelGeneratorGraph::Cache VoxelGeneratorGraph::_cache;

VoxelGeneratorGraph::VoxelGeneratorGraph() {
}

VoxelGeneratorGraph::~VoxelGeneratorGraph() {
	clear();
}

void VoxelGeneratorGraph::clear() {
	unregister_subresources();
	_graph.clear();
	{
		RWLockWrite wlock(_runtime_lock);
		_runtime.reset();
	}
}

static ProgramGraph::Node *create_node_internal(ProgramGraph &graph,
		VoxelGeneratorGraph::NodeTypeID type_id, Vector2 position, uint32_t id) {
	const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(type_id);

	ProgramGraph::Node *node = graph.create_node(type_id, id);
	ERR_FAIL_COND_V(node == nullptr, nullptr);
	node->inputs.resize(type.inputs.size());
	node->outputs.resize(type.outputs.size());
	node->default_inputs.resize(type.inputs.size());
	node->gui_position = position;

	node->params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		node->params[i] = type.params[i].default_value;
	}
	for (size_t i = 0; i < type.inputs.size(); ++i) {
		node->default_inputs[i] = type.inputs[i].default_value;
	}

	return node;
}

uint32_t VoxelGeneratorGraph::create_node(NodeTypeID type_id, Vector2 position, uint32_t id) {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton()->is_valid_type_id(type_id), ProgramGraph::NULL_ID);
	const ProgramGraph::Node *node = create_node_internal(_graph, type_id, position, id);
	ERR_FAIL_COND_V(node == nullptr, ProgramGraph::NULL_ID);
	return node->id;
}

void VoxelGeneratorGraph::remove_node(uint32_t node_id) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	for (size_t i = 0; i < node->params.size(); ++i) {
		Ref<Resource> resource = node->params[i];
		if (resource.is_valid()) {
			unregister_subresource(**resource);
		}
	}
	_graph.remove_node(node_id);
	emit_changed();
}

bool VoxelGeneratorGraph::can_connect(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND_V(!_graph.is_output_port_valid(src_port), false);
	ERR_FAIL_COND_V(!_graph.is_input_port_valid(dst_port), false);
	return _graph.can_connect(src_port, dst_port);
}

void VoxelGeneratorGraph::add_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND(!_graph.is_output_port_valid(src_port));
	ERR_FAIL_COND(!_graph.is_input_port_valid(dst_port));
	_graph.connect(src_port, dst_port);
	emit_changed();
}

void VoxelGeneratorGraph::remove_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND(!_graph.is_output_port_valid(src_port));
	ERR_FAIL_COND(!_graph.is_input_port_valid(dst_port));
	_graph.disconnect(src_port, dst_port);
	emit_changed();
}

void VoxelGeneratorGraph::get_connections(std::vector<ProgramGraph::Connection> &connections) const {
	_graph.get_connections(connections);
}

bool VoxelGeneratorGraph::try_get_connection_to(
		ProgramGraph::PortLocation dst, ProgramGraph::PortLocation &out_src) const {
	const ProgramGraph::Node *node = _graph.get_node(dst.node_id);
	CRASH_COND(node == nullptr);
	CRASH_COND(dst.port_index >= node->inputs.size());
	const ProgramGraph::Port &port = node->inputs[dst.port_index];
	if (port.connections.size() == 0) {
		return false;
	}
	out_src = port.connections[0];
	return true;
}

bool VoxelGeneratorGraph::has_node(uint32_t node_id) const {
	return _graph.try_get_node(node_id) != nullptr;
}

void VoxelGeneratorGraph::set_node_name(uint32_t node_id, StringName name) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_MSG(node == nullptr, "No node was found with the specified ID");
	if (node->name == name) {
		return;
	}
	if (name != StringName()) {
		const uint32_t existing_node_id = _graph.find_node_by_name(name);
		if (existing_node_id != ProgramGraph::NULL_ID && node_id == existing_node_id) {
			ERR_PRINT(String("More than one graph node has the name \"{0}\"").format(varray(name)));
		}
	}
	node->name = name;
	emit_signal(SIGNAL_NODE_NAME_CHANGED, node_id);
}

StringName VoxelGeneratorGraph::get_node_name(uint32_t node_id) const {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, StringName());
	return node->name;
}

uint32_t VoxelGeneratorGraph::find_node_by_name(StringName name) const {
	return _graph.find_node_by_name(name);
}

void VoxelGeneratorGraph::set_node_param(uint32_t node_id, uint32_t param_index, Variant value) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_INDEX(param_index, node->params.size());

	if (node->params[param_index] != value) {
		Ref<Resource> prev_resource = node->params[param_index];
		if (prev_resource.is_valid()) {
			unregister_subresource(**prev_resource);
		}

		node->params[param_index] = value;

		Ref<Resource> resource = value;
		if (resource.is_valid()) {
			register_subresource(**resource);
		}

		emit_changed();
	}
}

Variant VoxelGeneratorGraph::get_node_param(uint32_t node_id, uint32_t param_index) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Variant());
	ERR_FAIL_INDEX_V(param_index, node->params.size(), Variant());
	return node->params[param_index];
}

Variant VoxelGeneratorGraph::get_node_default_input(uint32_t node_id, uint32_t input_index) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Variant());
	ERR_FAIL_INDEX_V(input_index, node->default_inputs.size(), Variant());
	return node->default_inputs[input_index];
}

void VoxelGeneratorGraph::set_node_default_input(uint32_t node_id, uint32_t input_index, Variant value) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_INDEX(input_index, node->default_inputs.size());
	if (node->default_inputs[input_index] != value) {
		node->default_inputs[input_index] = value;
		emit_changed();
	}
}

Vector2 VoxelGeneratorGraph::get_node_gui_position(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, Vector2());
	return node->gui_position;
}

void VoxelGeneratorGraph::set_node_gui_position(uint32_t node_id, Vector2 pos) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	if (node->gui_position != pos) {
		node->gui_position = pos;
	}
}

VoxelGeneratorGraph::NodeTypeID VoxelGeneratorGraph::get_node_type_id(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, NODE_TYPE_COUNT);
	CRASH_COND(node->type_id >= NODE_TYPE_COUNT);
	return (NodeTypeID)node->type_id;
}

PoolIntArray VoxelGeneratorGraph::get_node_ids() const {
	return _graph.get_node_ids();
}

int VoxelGeneratorGraph::get_nodes_count() const {
	return _graph.get_nodes_count();
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
	_sdf_clip_threshold = max(t, 0.f);
}

int VoxelGeneratorGraph::get_used_channels_mask() const {
	return 1 << VoxelBufferInternal::CHANNEL_SDF;
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
		const VoxelGraphRuntime::State &state, Vector3i rmin, Vector3i rmax, int ry,
		VoxelBufferInternal &out_voxel_buffer, FixedArray<uint8_t, 4> spare_indices) {
	VOXEL_PROFILE_SCOPE();

	// TODO Optimization: exclude up-front outputs that are known to be zero?
	// So we choose the cases below based on non-zero outputs instead of total output count

	// TODO Could maybe put this part outside?
	FixedArray<Span<const float>, 16> buffers;
	const unsigned int buffers_count = weight_outputs.size();
	for (unsigned int oi = 0; oi < buffers_count; ++oi) {
		const WeightOutput &info = weight_outputs[oi];
		const VoxelGraphRuntime::Buffer &buffer = state.get_buffer(info.output_buffer_index);
		buffers[oi] = Span<const float>(buffer.data, buffer.size);
	}

	if (buffers_count <= 4) {
		// Pick all results and fill with spare indices to keep semantic
		unsigned int value_index = 0;
		for (int rz = rmin.z; rz < rmax.z; ++rz) {
			for (int rx = rmin.x; rx < rmax.x; ++rx) {
				FixedArray<uint8_t, 4> weights;
				FixedArray<uint8_t, 4> indices = spare_indices;
				weights.fill(0);
				for (unsigned int oi = 0; oi < buffers_count; ++oi) {
					const float weight = buffers[oi][value_index];
					// TODO Optimization: weight output nodes could already multiply by 255 and clamp afterward
					// so we would not need to do it here
					weights[oi] = clamp(weight * 255.f, 0.f, 255.f);
					indices[oi] = weight_outputs[oi].layer_index;
				}
				debug_check_texture_indices(indices);
				const uint16_t encoded_indices =
						encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
				const uint16_t encoded_weights =
						encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
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
					weights[oi] = clamp(weight * 255.f, 0.f, 255.f);
					indices[oi] = weight_outputs[oi].layer_index;
				}
				const uint16_t encoded_indices =
						encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
				const uint16_t encoded_weights =
						encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
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
				indices.fill(0);
				weights[0] = 1.f;
				weights[1] = 0.f;
				weights[2] = 0.f;
				weights[3] = 0.f;
				unsigned int recorded_weights = 0;
				// Pick up weights above pivot (this is not as correct as a sort but faster)
				for (unsigned int oi = 0; oi < buffers_count && recorded_weights < indices.size(); ++oi) {
					const float weight = buffers[oi][value_index];
					if (weight > pivot) {
						weights[recorded_weights] = clamp(weight * 255.f, 0.f, 255.f);
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
						encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
				// TODO Flatten this further?
				out_voxel_buffer.set_voxel(encoded_indices, rx, ry, rz, VoxelBufferInternal::CHANNEL_INDICES);
				out_voxel_buffer.set_voxel(encoded_weights, rx, ry, rz, VoxelBufferInternal::CHANNEL_WEIGHTS);
				++value_index;
			}
		}
	}
}

VoxelGenerator::Result VoxelGeneratorGraph::generate_block(VoxelBlockRequest &input) {
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
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	const Vector3i origin = input.origin_in_voxels;

	// TODO This may be shared across the module
	// Storing voxels is lossy on some depth configurations. They use normalized SDF,
	// so we must scale the values to make better use of the offered resolution
	const float sdf_scale = VoxelBufferInternal::get_sdf_quantization_scale(
			out_buffer.get_channel_depth(out_buffer.get_channel_depth(channel)));

	const int stride = 1 << input.lod;

	// Clip threshold must be higher for higher lod indexes because distances for one sampled voxel are also larger
	const float clip_threshold = sdf_scale * _sdf_clip_threshold * stride;

	// TODO Allow non-cubic block size when not using subdivision
	const int section_size = _use_subdivision ? _subdivision_size : min(min(bs.x, bs.y), bs.z);
	// Block size must be a multiple of section size
	ERR_FAIL_COND_V(bs.x % section_size != 0, result);
	ERR_FAIL_COND_V(bs.y % section_size != 0, result);
	ERR_FAIL_COND_V(bs.z % section_size != 0, result);

	Cache &cache = _cache;

	const unsigned int slice_buffer_size = section_size * section_size;
	VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, slice_buffer_size);

	cache.x_cache.resize(slice_buffer_size);
	cache.y_cache.resize(slice_buffer_size);
	cache.z_cache.resize(slice_buffer_size);

	Span<float> x_cache(cache.x_cache, 0, cache.x_cache.size());
	Span<float> y_cache(cache.y_cache, 0, cache.y_cache.size());
	Span<float> z_cache(cache.z_cache, 0, cache.z_cache.size());

	const float air_sdf = _debug_clipped_blocks ? -1.f : 1.f;
	const float matter_sdf = _debug_clipped_blocks ? 1.f : -1.f;

	FixedArray<uint8_t, 4> spare_texture_indices = runtime_ptr->spare_texture_indices;
	const unsigned int sdf_output_buffer_index = runtime_ptr->sdf_output_buffer_index;

	bool all_sdf_is_uniform = true;

	// For each subdivision of the block
	for (int sz = 0; sz < bs.z; sz += section_size) {
		for (int sy = 0; sy < bs.y; sy += section_size) {
			for (int sx = 0; sx < bs.x; sx += section_size) {
				VOXEL_PROFILE_SCOPE_NAMED("Section");

				const Vector3i rmin(sx, sy, sz);
				const Vector3i rmax = rmin + Vector3i(section_size);
				const Vector3i gmin = origin + (rmin << input.lod);
				const Vector3i gmax = origin + (rmax << input.lod);

				runtime.analyze_range(cache.state, gmin, gmax);
				const Interval sdf_range = cache.state.get_range(sdf_output_buffer_index) * sdf_scale;
				bool sdf_is_uniform = false;
				if (sdf_range.min > clip_threshold && sdf_range.max > clip_threshold) {
					out_buffer.fill_area_f(air_sdf, rmin, rmax, channel);
					// In case of air, we skip weights because there is nothing to texture anyways
					continue;

				} else if (sdf_range.min < -clip_threshold && sdf_range.max < -clip_threshold) {
					out_buffer.fill_area_f(matter_sdf, rmin, rmax, channel);
					sdf_is_uniform = true;

				} else if (sdf_range.is_single_value()) {
					out_buffer.fill_area_f(sdf_range.min, rmin, rmax, channel);
					if (sdf_range.min > 0.f) {
						continue;
					}
					sdf_is_uniform = true;
				}

				// The section may have the surface in it, we have to calculate it

				if (!sdf_is_uniform) {
					// SDF is not uniform, we may do a full query
					all_sdf_is_uniform = false;

					if (_use_optimized_execution_map) {
						// Optimize out branches of the graph that won't contribute to the result
						runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map, false);
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
						VOXEL_PROFILE_SCOPE_NAMED("Full slice");

						y_cache.fill(gy);

						// Full query
						runtime.generate_set(cache.state, x_cache, y_cache, z_cache,
								_use_xz_caching && ry != rmin.y,
								_use_optimized_execution_map ? &cache.optimized_execution_map : nullptr);

						{
							VOXEL_PROFILE_SCOPE_NAMED("Copy SDF to block");
							unsigned int i = 0;
							const VoxelGraphRuntime::Buffer &sdf_buffer =
									cache.state.get_buffer(sdf_output_buffer_index);
							for (int rz = rmin.z; rz < rmax.z; ++rz) {
								for (int rx = rmin.x; rx < rmax.x; ++rx) {
									// TODO Flatten this further, this may run checks we don't need
									out_buffer.set_voxel_f(sdf_scale * sdf_buffer.data[i], rx, ry, rz, channel);
									++i;
								}
							}
						}

						if (runtime_ptr->weight_outputs_count > 0) {
							gather_indices_and_weights(
									to_span_const(runtime_ptr->weight_outputs, runtime_ptr->weight_outputs_count),
									cache.state, rmin, rmax, ry, out_buffer, spare_texture_indices);
						}
					}

				} else if (runtime_ptr->weight_outputs_count > 0) {
					// SDF is uniform and full of matter, but we may want to query weights

					if (_use_optimized_execution_map) {
						// Optimize out branches of the graph that won't contribute to the result
						runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map,
								to_span_const(runtime_ptr->weight_output_indices, runtime_ptr->weight_outputs_count),
								false);
					}

					unsigned int i = 0;
					for (int rz = rmin.z, gz = gmin.z; rz < rmax.z; ++rz, gz += stride) {
						for (int rx = rmin.x, gx = gmin.x; rx < rmax.x; ++rx, gx += stride) {
							x_cache[i] = gx;
							z_cache[i] = gz;
							++i;
						}
					}

					for (int ry = rmin.y, gy = gmin.y; ry < rmax.y; ++ry, gy += stride) {
						VOXEL_PROFILE_SCOPE_NAMED("Weights slice");

						y_cache.fill(gy);

						runtime.generate_set(cache.state, x_cache, y_cache, z_cache,
								_use_xz_caching && ry != rmin.y,
								_use_optimized_execution_map ? &cache.optimized_execution_map : nullptr);

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
	if (all_sdf_is_uniform) {
		// TODO If voxel texure weights are used, octree compression might be a bit more complicated.
		// For now we only look at SDF but if texture weights are used and the player digs a bit inside terrain,
		// they will find it's all default weights.
		// Possible workarounds:
		// - Only do it for air
		// - Also take indices and weights into account, but may lead to way less compression, or none, for stuff that
		//   essentially isnt showing up until dug out
		// - Invoke generator to produce LOD0 blocks somehow, but main thread could stall
		result.max_lod_hint = true;
	}

	return result;
}

VoxelGraphRuntime::CompilationResult VoxelGeneratorGraph::compile() {
	const int64_t time_before = OS::get_singleton()->get_ticks_usec();

	std::shared_ptr<Runtime> r = std::make_shared<Runtime>();
	VoxelGraphRuntime &runtime = r->runtime;

	// Core compilation
	const VoxelGraphRuntime::CompilationResult result =
			runtime.compile(_graph, Engine::get_singleton()->is_editor_hint());

	if (!result.success) {
		return result;
	}

	// Extra steps
	for (unsigned int output_index = 0; output_index < runtime.get_output_count(); ++output_index) {
		const VoxelGraphRuntime::OutputInfo output = runtime.get_output_info(output_index);
		const ProgramGraph::Node *node = _graph.get_node(output.node_id);

		ERR_FAIL_COND_V(node == nullptr, VoxelGraphRuntime::CompilationResult());

		switch (node->type_id) {
			case NODE_OUTPUT_SDF:
				if (r->sdf_output_buffer_index != -1) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = TTR("Multiple SDF outputs are not supported");
					error.node_id = output.node_id;
					return error;
				} else {
					r->sdf_output_buffer_index = output.buffer_address;
				}
				break;

			case NODE_OUTPUT_WEIGHT: {
				if (r->weight_outputs_count >= r->weight_outputs.size()) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = String(TTR("Cannot use more than {0} weight outputs"))
											.format(varray(r->weight_outputs.size()));
					error.node_id = output.node_id;
					return error;
				}
				CRASH_COND(node->params.size() == 0);
				const int layer_index = node->params[0];
				if (layer_index < 0) {
					// Should not be allowed by the UI, but who knows
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = String(TTR("Cannot use negative layer index in weight output"));
					error.node_id = output.node_id;
					return error;
				}
				if (layer_index >= static_cast<int>(r->weight_outputs.size())) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = String(TTR("Weight layers cannot exceed {}"))
											.format(varray(r->weight_outputs.size()));
					error.node_id = output.node_id;
					return error;
				}
				for (unsigned int i = 0; i < r->weight_outputs_count; ++i) {
					const WeightOutput &wo = r->weight_outputs[i];
					if (static_cast<int>(wo.layer_index) == layer_index) {
						VoxelGraphRuntime::CompilationResult error;
						error.success = false;
						error.message =
								String(TTR("Only one weight output node can use layer index {0}, found duplicate"))
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

			default:
				break;
		}
	}

	if (r->sdf_output_buffer_index == -1) {
		VoxelGraphRuntime::CompilationResult error;
		error.success = false;
		error.message = String(TTR("An SDF output is required for the graph to be valid."));
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
		used_indices_map.fill(false);
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
		//debug_check_texture_indices(spare_indices);
		ERR_FAIL_COND_V(spare_indices_count != 4, VoxelGraphRuntime::CompilationResult());
		r->spare_texture_indices = spare_indices;
	}

	// Store valid result
	RWLockWrite wlock(_runtime_lock);
	_runtime = r;

	const int64_t time_spent = OS::get_singleton()->get_ticks_usec() - time_before;
	PRINT_VERBOSE(String("Voxel graph compiled in {0} us").format(varray(time_spent)));

	return result;
}

// This is an external API which involves locking so better not use this internally
bool VoxelGeneratorGraph::is_good() const {
	RWLockRead rlock(_runtime_lock);
	return _runtime != nullptr;
}

void VoxelGeneratorGraph::generate_set(Span<float> in_x, Span<float> in_y, Span<float> in_z) {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND(_runtime == nullptr);
	Cache &cache = _cache;
	VoxelGraphRuntime &runtime = _runtime->runtime;
	runtime.prepare_state(cache.state, in_x.size());
	runtime.generate_set(cache.state, in_x, in_y, in_z, false, nullptr);
}

const VoxelGraphRuntime::State &VoxelGeneratorGraph::get_last_state_from_current_thread() {
	return _cache.state;
}

Span<const int> VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread() {
	return to_span_const(_cache.optimized_execution_map.debug_nodes);
}

bool VoxelGeneratorGraph::try_get_output_port_address(ProgramGraph::PortLocation port, uint32_t &out_address) const {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND_V(_runtime == nullptr, false);
	uint16_t addr;
	const bool res = _runtime->runtime.try_get_output_port_address(port, addr);
	out_address = addr;
	return res;
}

void VoxelGeneratorGraph::find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_dependencies) const {
	std::vector<uint32_t> dst;
	dst.push_back(node_id);
	_graph.find_dependencies(dst, out_dependencies);
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

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		Ref<Image> im;
		const VoxelGraphRuntime &runtime;
		VoxelGraphRuntime::State &state;
		const unsigned int sdf_buffer_index;
		const float ref_radius;
		const float sdf_min;
		const float sdf_max;

		ProcessChunk(VoxelGraphRuntime::State &p_state, unsigned int p_sdf_buffer_index,
				const VoxelGraphRuntime &p_runtime,
				float p_ref_radius, float p_sdf_min, float p_sdf_max) :
				runtime(p_runtime),
				state(p_state),
				sdf_buffer_index(p_sdf_buffer_index),
				ref_radius(p_ref_radius),
				sdf_min(p_sdf_min),
				sdf_max(p_sdf_max) {}

		void operator()(int x0, int y0, int width, int height) {
			VOXEL_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			runtime.prepare_state(state, area);

			const Vector2 suv = Vector2(
					1.f / static_cast<float>(im->get_width()),
					1.f / static_cast<float>(im->get_height()));

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

			runtime.generate_set(state, to_span(x_coords), to_span(y_coords), to_span(z_coords), false, nullptr);
			const VoxelGraphRuntime::Buffer &buffer = state.get_buffer(sdf_buffer_index);

			// Calculate final pixels
			im->lock();
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float sdf = buffer.data[i];
					const float nh = (-sdf - sdf_min) * nr;
					im->set_pixel(ix, iy, Color(nh, nh, nh));
					++i;
				}
			}
			im->unlock();
		}
	};

	Cache &cache = _cache;

	ProcessChunk pc(cache.state, runtime_ptr->sdf_output_buffer_index, runtime_ptr->runtime,
			ref_radius, sdf_min, sdf_max);
	pc.im = im;
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

// If this generator is used to produce a planet, specifically using a spherical heightmap approach,
// then this function can be used to bake a map of the surface.
// Such maps can be used by shaders to sharpen the details of the planet when seen from far away.
void VoxelGeneratorGraph::bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(im.is_null());

	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}

	ERR_FAIL_COND(runtime_ptr == nullptr);

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> sdf_values_p; // TODO Could be used at the same time to get bump?
		std::vector<float> sdf_values_px;
		std::vector<float> sdf_values_py;
		unsigned int sdf_buffer_index;
		Ref<Image> im;
		const VoxelGraphRuntime &runtime;
		VoxelGraphRuntime::State &state;
		const float strength;
		const float ref_radius;

		ProcessChunk(VoxelGraphRuntime::State &p_state, unsigned int p_sdf_buffer_index,
				Ref<Image> p_im,
				const VoxelGraphRuntime &p_runtime,
				float p_strength, float p_ref_radius) :
				sdf_buffer_index(p_sdf_buffer_index),
				im(p_im),
				runtime(p_runtime),
				state(p_state),
				strength(p_strength),
				ref_radius(p_ref_radius) {}

		void operator()(int x0, int y0, int width, int height) {
			VOXEL_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			sdf_values_p.resize(area);
			sdf_values_px.resize(area);
			sdf_values_py.resize(area);
			runtime.prepare_state(state, area);

			const float ns = 2.f / strength;

			const Vector2 suv = Vector2(
					1.f / static_cast<float>(im->get_width()),
					1.f / static_cast<float>(im->get_height()));

			const Vector2 normal_step = 0.5f * Vector2(1.f, 1.f) / im->get_size();
			const Vector2 normal_step_x = Vector2(normal_step.x, 0.f);
			const Vector2 normal_step_y = Vector2(0.f, normal_step.y);

			const int xmax = x0 + width;
			const int ymax = y0 + height;

			const VoxelGraphRuntime::Buffer &sdf_buffer = state.get_buffer(sdf_buffer_index);

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
			// TODO Perform range analysis on the range of coordinates, it might still yield performance benefits
			runtime.generate_set(state, to_span(x_coords), to_span(y_coords), to_span(z_coords), false, nullptr);
			CRASH_COND(sdf_values_p.size() != sdf_buffer.size);
			memcpy(sdf_values_p.data(), sdf_buffer.data, sdf_values_p.size() * sizeof(float));

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
			runtime.generate_set(state, to_span(x_coords), to_span(y_coords), to_span(z_coords), false, nullptr);
			CRASH_COND(sdf_values_px.size() != sdf_buffer.size);
			memcpy(sdf_values_px.data(), sdf_buffer.data, sdf_values_px.size() * sizeof(float));

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
			runtime.generate_set(state, to_span(x_coords), to_span(y_coords), to_span(z_coords), false, nullptr);
			CRASH_COND(sdf_values_py.size() != sdf_buffer.size);
			memcpy(sdf_values_py.data(), sdf_buffer.data, sdf_values_py.size() * sizeof(float));

			// TODO This is probably invalid due to the distortion, may need to use another approach.
			// Compute the 3D normal from gradient, then project it?

			// Calculate final pixels
			im->lock();
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float h = sdf_values_p[i];
					const float h_px = sdf_values_px[i];
					const float h_py = sdf_values_py[i];
					++i;
					const Vector3 normal = Vector3(h_px - h, ns, h_py - h).normalized();
					const Color en(
							0.5f * normal.x + 0.5f,
							-0.5f * normal.z + 0.5f,
							0.5f * normal.y + 0.5f);
					im->set_pixel(ix, iy, en);
				}
			}
			im->unlock();
		}
	};

	Cache &cache = _cache;

	// The default for strength is 1.f
	const float e = 0.001f;
	if (strength > -e && strength < e) {
		if (strength > 0.f) {
			strength = e;
		} else {
			strength = -e;
		}
	}

	ProcessChunk pc(cache.state, runtime_ptr->sdf_output_buffer_index, im, runtime_ptr->runtime, strength, ref_radius);
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

// TODO This function isn't used yet, but whatever uses it should probably put locking and cache outside
float VoxelGeneratorGraph::generate_single(const Vector3i &position) {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, 0.f);
	Cache &cache = _cache;
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, 1);
	runtime.generate_single(cache.state, position.to_vec3(), nullptr);
	const VoxelGraphRuntime::Buffer &buffer = cache.state.get_buffer(runtime_ptr->sdf_output_buffer_index);
	ERR_FAIL_COND_V(buffer.size == 0, 0.f);
	ERR_FAIL_COND_V(buffer.data == nullptr, 0.f);
	return buffer.data[0];
}

// Note, this wrapper may not be used for main generation tasks.
// It is mostly used as a debug tool.
Interval VoxelGeneratorGraph::debug_analyze_range(Vector3i min_pos, Vector3i max_pos,
		bool optimize_execution_map) const {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, Interval::from_single_value(0.f));
	Cache &cache = _cache;
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	// Note, buffer size is irrelevant here, because range analysis doesn't use buffers
	runtime.prepare_state(cache.state, 1);
	runtime.analyze_range(cache.state, min_pos, max_pos);
	if (optimize_execution_map) {
		runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map, true);
	}
	return cache.state.get_range(runtime_ptr->sdf_output_buffer_index);
}

Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instance();

	d->_graph.copy_from(_graph, p_subresources);
	d->register_subresources();
	// Program not copied, as it may contain pointers to the resources we are duplicating

	return d;
}

static Dictionary get_graph_as_variant_data(const ProgramGraph &graph) {
	Dictionary nodes_data;
	PoolVector<int> node_ids = graph.get_node_ids();
	{
		PoolVector<int>::Read r = node_ids.read();
		for (int i = 0; i < node_ids.size(); ++i) {
			uint32_t node_id = r[i];
			const ProgramGraph::Node *node = graph.get_node(node_id);
			ERR_FAIL_COND_V(node == nullptr, Dictionary());

			Dictionary node_data;

			const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node->type_id);
			node_data["type"] = type.name;
			node_data["gui_position"] = node->gui_position;

			if (node->name != StringName()) {
				node_data["name"] = node->name;
			}

			for (size_t j = 0; j < type.params.size(); ++j) {
				const VoxelGraphNodeDB::Param &param = type.params[j];
				node_data[param.name] = node->params[j];
			}

			for (size_t j = 0; j < type.inputs.size(); ++j) {
				if (node->inputs[j].connections.size() == 0) {
					const VoxelGraphNodeDB::Port &port = type.inputs[j];
					node_data[port.name] = node->default_inputs[j];
				}
			}

			String key = String::num_uint64(node_id);
			nodes_data[key] = node_data;
		}
	}

	Array connections_data;
	std::vector<ProgramGraph::Connection> connections;
	graph.get_connections(connections);
	connections_data.resize(connections.size());
	for (size_t i = 0; i < connections.size(); ++i) {
		const ProgramGraph::Connection &con = connections[i];
		Array con_data;
		con_data.resize(4);
		con_data[0] = con.src.node_id;
		con_data[1] = con.src.port_index;
		con_data[2] = con.dst.node_id;
		con_data[3] = con.dst.port_index;
		connections_data[i] = con_data;
	}

	Dictionary data;
	data["nodes"] = nodes_data;
	data["connections"] = connections_data;
	return data;
}

Dictionary VoxelGeneratorGraph::get_graph_as_variant_data() const {
	return ::get_graph_as_variant_data(_graph);
}

static bool var_to_id(Variant v, uint32_t &out_id, uint32_t min = 0) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT, false);
	int i = v;
	ERR_FAIL_COND_V(i < 0 || (unsigned int)i < min, false);
	out_id = i;
	return true;
}

static bool load_graph_from_variant_data(ProgramGraph &graph, Dictionary data) {
	Dictionary nodes_data = data["nodes"];
	Array connections_data = data["connections"];
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();

	const Variant *id_key = nullptr;
	while ((id_key = nodes_data.next(id_key))) {
		const String id_str = *id_key;
		ERR_FAIL_COND_V(!id_str.is_valid_integer(), false);
		const int sid = id_str.to_int();
		ERR_FAIL_COND_V(sid < static_cast<int>(ProgramGraph::NULL_ID), false);
		const uint32_t id = sid;

		Dictionary node_data = nodes_data[*id_key];

		const String type_name = node_data["type"];
		const Vector2 gui_position = node_data["gui_position"];
		VoxelGeneratorGraph::NodeTypeID type_id;
		ERR_FAIL_COND_V(!type_db.try_get_type_id_from_name(type_name, type_id), false);
		ProgramGraph::Node *node = create_node_internal(graph, type_id, gui_position, id);
		ERR_FAIL_COND_V(node == nullptr, false);

		const Variant *param_key = nullptr;
		while ((param_key = node_data.next(param_key))) {
			const String param_name = *param_key;
			if (param_name == "type") {
				continue;
			}
			if (param_name == "gui_position") {
				continue;
			}
			uint32_t param_index;
			if (type_db.try_get_param_index_from_name(type_id, param_name, param_index)) {
				node->params[param_index] = node_data[*param_key];
			}
			if (type_db.try_get_input_index_from_name(type_id, param_name, param_index)) {
				node->default_inputs[param_index] = node_data[*param_key];
			}
			const Variant *vname = node_data.getptr("name");
			if (vname != nullptr) {
				node->name = *vname;
			}
		}
	}

	for (int i = 0; i < connections_data.size(); ++i) {
		Array con_data = connections_data[i];
		ERR_FAIL_COND_V(con_data.size() != 4, false);
		ProgramGraph::PortLocation src;
		ProgramGraph::PortLocation dst;
		ERR_FAIL_COND_V(!var_to_id(con_data[0], src.node_id, ProgramGraph::NULL_ID), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[1], src.port_index), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[2], dst.node_id, ProgramGraph::NULL_ID), false);
		ERR_FAIL_COND_V(!var_to_id(con_data[3], dst.port_index), false);
		graph.connect(src, dst);
	}

	return true;
}

void VoxelGeneratorGraph::load_graph_from_variant_data(Dictionary data) {
	clear();

	if (::load_graph_from_variant_data(_graph, data)) {
		register_subresources();
		// It's possible to auto-compile on load because `graph_data` is the only property set by the loader,
		// which is enough to have all information we need
		compile();

	} else {
		_graph.clear();
	}
}

void VoxelGeneratorGraph::register_subresource(Resource &resource) {
	//print_line(String("{0}: Registering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.connect(CoreStringNames::get_singleton()->changed, this, "_on_subresource_changed");
}

void VoxelGeneratorGraph::unregister_subresource(Resource &resource) {
	//print_line(String("{0}: Unregistering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.disconnect(CoreStringNames::get_singleton()->changed, this, "_on_subresource_changed");
}

void VoxelGeneratorGraph::register_subresources() {
	_graph.for_each_node([this](ProgramGraph::Node &node) {
		for (size_t i = 0; i < node.params.size(); ++i) {
			Ref<Resource> resource = node.params[i];
			if (resource.is_valid()) {
				register_subresource(**resource);
			}
		}
	});
}

void VoxelGeneratorGraph::unregister_subresources() {
	_graph.for_each_node([this](ProgramGraph::Node &node) {
		for (size_t i = 0; i < node.params.size(); ++i) {
			Ref<Resource> resource = node.params[i];
			if (resource.is_valid()) {
				unregister_subresource(**resource);
			}
		}
	});
}

// Debug land

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel(bool singular) {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, 0.f);
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;

	const uint32_t cube_size = 16;
	const uint32_t cube_count = 250;
	// const uint32_t cube_size = 100;
	// const uint32_t cube_count = 1;
	const uint32_t voxel_count = cube_size * cube_size * cube_size * cube_count;
	ProfilingClock profiling_clock;
	uint64_t elapsed_us = 0;

	Cache &cache = _cache;

	if (singular) {
		runtime.prepare_state(cache.state, 1);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t z = 0; z < cube_size; ++z) {
				for (uint32_t y = 0; y < cube_size; ++y) {
					for (uint32_t x = 0; x < cube_size; ++x) {
						runtime.generate_single(cache.state, Vector3i(x, y, z).to_vec3(), nullptr);
					}
				}
			}

			elapsed_us += profiling_clock.restart();
		}

	} else {
		const unsigned int cube_volume = cube_size * cube_size * cube_size;
		std::vector<float> src_x;
		std::vector<float> src_y;
		std::vector<float> src_z;
		src_x.resize(cube_volume);
		src_y.resize(cube_volume);
		src_z.resize(cube_volume);
		Span<float> sx(src_x, 0, src_x.size());
		Span<float> sy(src_y, 0, src_y.size());
		Span<float> sz(src_z, 0, src_z.size());

		runtime.prepare_state(cache.state, sx.size());

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t y = 0; y < cube_size; ++y) {
				runtime.generate_set(cache.state, sx, sy, sz, false, nullptr);
			}

			elapsed_us += profiling_clock.restart();
		}
	}

	float us = static_cast<double>(elapsed_us) / voxel_count;
	return us;
}

// This may be used as template when creating new graphs
void VoxelGeneratorGraph::load_plane_preset() {
	clear();

	/*
	 *     X
	 *
	 *     Y --- SdfPlane --- OutputSDF
	 *
	 *     Z
	 */

	const Vector2 k(40, 50);

	/*const uint32_t n_x = */ create_node(NODE_INPUT_X, Vector2(11, 1) * k); // 1
	const uint32_t n_y = create_node(NODE_INPUT_Y, Vector2(11, 3) * k); // 2
	/*const uint32_t n_z = */ create_node(NODE_INPUT_Z, Vector2(11, 5) * k); // 3
	const uint32_t n_o = create_node(NODE_OUTPUT_SDF, Vector2(18, 3) * k); // 4
	const uint32_t n_plane = create_node(NODE_SDF_PLANE, Vector2(14, 3) * k); // 5

	add_connection(n_y, 0, n_plane, 0);
	add_connection(n_plane, 0, n_o, 0);
}

void VoxelGeneratorGraph::debug_load_waves_preset() {
	clear();
	// This is mostly for testing

	const Vector2 k(35, 50);

	const uint32_t n_x = create_node(NODE_INPUT_X, Vector2(11, 1) * k); // 1
	const uint32_t n_y = create_node(NODE_INPUT_Y, Vector2(37, 1) * k); // 2
	const uint32_t n_z = create_node(NODE_INPUT_Z, Vector2(11, 5) * k); // 3
	const uint32_t n_o = create_node(NODE_OUTPUT_SDF, Vector2(45, 3) * k); // 4
	const uint32_t n_sin0 = create_node(NODE_SIN, Vector2(23, 1) * k); // 5
	const uint32_t n_sin1 = create_node(NODE_SIN, Vector2(23, 5) * k); // 6
	const uint32_t n_add = create_node(NODE_ADD, Vector2(27, 3) * k); // 7
	const uint32_t n_mul0 = create_node(NODE_MULTIPLY, Vector2(17, 1) * k); // 8
	const uint32_t n_mul1 = create_node(NODE_MULTIPLY, Vector2(17, 5) * k); // 9
	const uint32_t n_mul2 = create_node(NODE_MULTIPLY, Vector2(33, 3) * k); // 10
	const uint32_t n_c0 = create_node(NODE_CONSTANT, Vector2(14, 3) * k); // 11
	const uint32_t n_c1 = create_node(NODE_CONSTANT, Vector2(30, 5) * k); // 12
	const uint32_t n_sub = create_node(NODE_SUBTRACT, Vector2(39, 3) * k); // 13

	set_node_param(n_c0, 0, 1.f / 20.f);
	set_node_param(n_c1, 0, 10.f);

	/*
	 *    X --- * --- sin           Y
	 *         /         \           \
	 *       1/20         + --- * --- - --- O
	 *         \         /     /
	 *    Z --- * --- sin    10.0
	 */

	add_connection(n_x, 0, n_mul0, 0);
	add_connection(n_z, 0, n_mul1, 0);
	add_connection(n_c0, 0, n_mul0, 1);
	add_connection(n_c0, 0, n_mul1, 1);
	add_connection(n_mul0, 0, n_sin0, 0);
	add_connection(n_mul1, 0, n_sin1, 0);
	add_connection(n_sin0, 0, n_add, 0);
	add_connection(n_sin1, 0, n_add, 1);
	add_connection(n_add, 0, n_mul2, 0);
	add_connection(n_c1, 0, n_mul2, 1);
	add_connection(n_y, 0, n_sub, 0);
	add_connection(n_mul2, 0, n_sub, 1);
	add_connection(n_sub, 0, n_o, 0);
}

// Binding land

int VoxelGeneratorGraph::_b_get_node_type_count() const {
	return VoxelGraphNodeDB::get_singleton()->get_type_count();
}

Dictionary VoxelGeneratorGraph::_b_get_node_type_info(int type_id) const {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton()->is_valid_type_id(type_id), Dictionary());
	return VoxelGraphNodeDB::get_singleton()->get_type_info_dict(type_id);
}

Array VoxelGeneratorGraph::_b_get_connections() const {
	Array con_array;
	std::vector<ProgramGraph::Connection> cons;
	_graph.get_connections(cons);
	con_array.resize(cons.size());

	for (size_t i = 0; i < cons.size(); ++i) {
		const ProgramGraph::Connection &con = cons[i];
		Dictionary d;
		d["src_node_id"] = con.src.node_id;
		d["src_port_index"] = con.src.port_index;
		d["dst_node_id"] = con.dst.node_id;
		d["dst_port_index"] = con.dst.port_index;
		con_array[i] = d;
	}

	return con_array;
}

void VoxelGeneratorGraph::_b_set_node_param_null(int node_id, int param_index) {
	set_node_param(node_id, param_index, Variant());
}

void VoxelGeneratorGraph::_b_set_node_name(int node_id, String name) {
	set_node_name(node_id, name);
}

float VoxelGeneratorGraph::_b_generate_single(Vector3 pos) {
	return generate_single(Vector3i::from_floored(pos));
}

Vector2 VoxelGeneratorGraph::_b_debug_analyze_range(Vector3 min_pos, Vector3 max_pos) const {
	ERR_FAIL_COND_V(min_pos.x > max_pos.x, Vector2());
	ERR_FAIL_COND_V(min_pos.y > max_pos.y, Vector2());
	ERR_FAIL_COND_V(min_pos.z > max_pos.z, Vector2());
	const Interval r = debug_analyze_range(
			Vector3i::from_floored(min_pos), Vector3i::from_floored(max_pos), false);
	return Vector2(r.min, r.max);
}

Dictionary VoxelGeneratorGraph::_b_compile() {
	VoxelGraphRuntime::CompilationResult res = compile();
	Dictionary d;
	d["success"] = res.success;
	if (!res.success) {
		d["message"] = res.message;
		d["node_id"] = res.node_id;
	}
	return d;
}

void VoxelGeneratorGraph::_on_subresource_changed() {
	emit_changed();
}

void VoxelGeneratorGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGeneratorGraph::clear);
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"),
			&VoxelGeneratorGraph::create_node, DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("remove_node", "node_id"), &VoxelGeneratorGraph::remove_node);
	ClassDB::bind_method(D_METHOD("can_connect", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGeneratorGraph::can_connect);
	ClassDB::bind_method(D_METHOD("add_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGeneratorGraph::add_connection);
	ClassDB::bind_method(
			D_METHOD("remove_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGeneratorGraph::remove_connection);
	ClassDB::bind_method(D_METHOD("get_connections"), &VoxelGeneratorGraph::_b_get_connections);
	ClassDB::bind_method(D_METHOD("get_node_ids"), &VoxelGeneratorGraph::get_node_ids);
	ClassDB::bind_method(D_METHOD("find_node_by_name", "name"), &VoxelGeneratorGraph::find_node_by_name);

	ClassDB::bind_method(D_METHOD("get_node_type_id", "node_id"), &VoxelGeneratorGraph::get_node_type_id);
	ClassDB::bind_method(D_METHOD("get_node_param", "node_id", "param_index"), &VoxelGeneratorGraph::get_node_param);
	ClassDB::bind_method(D_METHOD("set_node_param", "node_id", "param_index", "value"),
			&VoxelGeneratorGraph::set_node_param);
	ClassDB::bind_method(D_METHOD("get_node_default_input", "node_id", "input_index"),
			&VoxelGeneratorGraph::get_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_default_input", "node_id", "input_index", "value"),
			&VoxelGeneratorGraph::set_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_param_null", "node_id", "param_index"),
			&VoxelGeneratorGraph::_b_set_node_param_null);
	ClassDB::bind_method(D_METHOD("get_node_gui_position", "node_id"), &VoxelGeneratorGraph::get_node_gui_position);
	ClassDB::bind_method(D_METHOD("set_node_gui_position", "node_id", "position"),
			&VoxelGeneratorGraph::set_node_gui_position);
	ClassDB::bind_method(D_METHOD("set_node_name", "node_id", "name"), &VoxelGeneratorGraph::_b_set_node_name);

	ClassDB::bind_method(D_METHOD("set_sdf_clip_threshold", "threshold"), &VoxelGeneratorGraph::set_sdf_clip_threshold);
	ClassDB::bind_method(D_METHOD("get_sdf_clip_threshold"), &VoxelGeneratorGraph::get_sdf_clip_threshold);

	ClassDB::bind_method(D_METHOD("is_using_optimized_execution_map"),
			&VoxelGeneratorGraph::is_using_optimized_execution_map);
	ClassDB::bind_method(D_METHOD("set_use_optimized_execution_map", "use"),
			&VoxelGeneratorGraph::set_use_optimized_execution_map);

	ClassDB::bind_method(D_METHOD("set_use_subdivision", "use"), &VoxelGeneratorGraph::set_use_subdivision);
	ClassDB::bind_method(D_METHOD("is_using_subdivision"), &VoxelGeneratorGraph::is_using_subdivision);

	ClassDB::bind_method(D_METHOD("set_subdivision_size", "size"), &VoxelGeneratorGraph::set_subdivision_size);
	ClassDB::bind_method(D_METHOD("get_subdivision_size"), &VoxelGeneratorGraph::get_subdivision_size);

	ClassDB::bind_method(D_METHOD("set_debug_clipped_blocks", "enabled"),
			&VoxelGeneratorGraph::set_debug_clipped_blocks);
	ClassDB::bind_method(D_METHOD("is_debug_clipped_blocks"), &VoxelGeneratorGraph::is_debug_clipped_blocks);

	ClassDB::bind_method(D_METHOD("set_use_xz_caching", "enabled"), &VoxelGeneratorGraph::set_use_xz_caching);
	ClassDB::bind_method(D_METHOD("is_using_xz_caching"), &VoxelGeneratorGraph::is_using_xz_caching);

	ClassDB::bind_method(D_METHOD("compile"), &VoxelGeneratorGraph::_b_compile);

	ClassDB::bind_method(D_METHOD("get_node_type_count"), &VoxelGeneratorGraph::_b_get_node_type_count);
	ClassDB::bind_method(D_METHOD("get_node_type_info", "type_id"), &VoxelGeneratorGraph::_b_get_node_type_info);

	ClassDB::bind_method(D_METHOD("generate_single"), &VoxelGeneratorGraph::_b_generate_single);
	ClassDB::bind_method(D_METHOD("debug_analyze_range", "min_pos", "max_pos"),
			&VoxelGeneratorGraph::_b_debug_analyze_range);

	ClassDB::bind_method(D_METHOD("bake_sphere_bumpmap", "im", "ref_radius", "sdf_min", "sdf_max"),
			&VoxelGeneratorGraph::bake_sphere_bumpmap);
	ClassDB::bind_method(D_METHOD("bake_sphere_normalmap", "im", "ref_radius", "strength"),
			&VoxelGeneratorGraph::bake_sphere_normalmap);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &VoxelGeneratorGraph::debug_load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel", "use_singular_queries"),
			&VoxelGeneratorGraph::debug_measure_microseconds_per_voxel);

	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &VoxelGeneratorGraph::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &VoxelGeneratorGraph::get_graph_as_variant_data);

	ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGeneratorGraph::_on_subresource_changed);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

	ADD_GROUP("Performance Tuning", "");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "sdf_clip_threshold"), "set_sdf_clip_threshold", "get_sdf_clip_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_optimized_execution_map"),
			"set_use_optimized_execution_map", "is_using_optimized_execution_map");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_subdivision"), "set_use_subdivision", "is_using_subdivision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivision_size"), "set_subdivision_size", "get_subdivision_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_xz_caching"), "set_use_xz_caching", "is_using_xz_caching");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_block_clipping"),
			"set_debug_clipped_blocks", "is_debug_clipped_blocks");

	ADD_SIGNAL(MethodInfo(SIGNAL_NODE_NAME_CHANGED, PropertyInfo(Variant::INT, "node_id")));

	BIND_ENUM_CONSTANT(NODE_CONSTANT);
	BIND_ENUM_CONSTANT(NODE_INPUT_X);
	BIND_ENUM_CONSTANT(NODE_INPUT_Y);
	BIND_ENUM_CONSTANT(NODE_INPUT_Z);
	BIND_ENUM_CONSTANT(NODE_OUTPUT_SDF);
	BIND_ENUM_CONSTANT(NODE_ADD);
	BIND_ENUM_CONSTANT(NODE_SUBTRACT);
	BIND_ENUM_CONSTANT(NODE_MULTIPLY);
	BIND_ENUM_CONSTANT(NODE_DIVIDE);
	BIND_ENUM_CONSTANT(NODE_SIN);
	BIND_ENUM_CONSTANT(NODE_FLOOR);
	BIND_ENUM_CONSTANT(NODE_ABS);
	BIND_ENUM_CONSTANT(NODE_SQRT);
	BIND_ENUM_CONSTANT(NODE_FRACT);
	BIND_ENUM_CONSTANT(NODE_STEPIFY);
	BIND_ENUM_CONSTANT(NODE_WRAP);
	BIND_ENUM_CONSTANT(NODE_MIN);
	BIND_ENUM_CONSTANT(NODE_MAX);
	BIND_ENUM_CONSTANT(NODE_DISTANCE_2D);
	BIND_ENUM_CONSTANT(NODE_DISTANCE_3D);
	BIND_ENUM_CONSTANT(NODE_CLAMP);
	BIND_ENUM_CONSTANT(NODE_MIX);
	BIND_ENUM_CONSTANT(NODE_REMAP);
	BIND_ENUM_CONSTANT(NODE_SMOOTHSTEP);
	BIND_ENUM_CONSTANT(NODE_CURVE);
	BIND_ENUM_CONSTANT(NODE_SELECT);
	BIND_ENUM_CONSTANT(NODE_NOISE_2D);
	BIND_ENUM_CONSTANT(NODE_NOISE_3D);
	BIND_ENUM_CONSTANT(NODE_IMAGE_2D);
	BIND_ENUM_CONSTANT(NODE_SDF_PLANE);
	BIND_ENUM_CONSTANT(NODE_SDF_BOX);
	BIND_ENUM_CONSTANT(NODE_SDF_SPHERE);
	BIND_ENUM_CONSTANT(NODE_SDF_TORUS);
	BIND_ENUM_CONSTANT(NODE_SDF_PREVIEW);
	BIND_ENUM_CONSTANT(NODE_SDF_SPHERE_HEIGHTMAP);
	BIND_ENUM_CONSTANT(NODE_SDF_SMOOTH_UNION);
	BIND_ENUM_CONSTANT(NODE_SDF_SMOOTH_SUBTRACT);
	BIND_ENUM_CONSTANT(NODE_NORMALIZE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_3D);
	BIND_ENUM_CONSTANT(NODE_OUTPUT_WEIGHT);
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}
