#include "voxel_generator_graph.h"
#include "../../constants/voxel_string_names.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../util/container_funcs.h"
#include "../../util/expression_parser.h"
#include "../../util/godot/array.h"
#include "../../util/godot/callable.h"
#include "../../util/godot/engine.h"
#include "../../util/godot/image.h"
#include "../../util/godot/object.h"
#include "../../util/godot/string.h"
#include "../../util/hash_funcs.h"
#include "../../util/log.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "voxel_graph_node_db.h"

namespace zylann::voxel {

const char *VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

thread_local VoxelGeneratorGraph::Cache VoxelGeneratorGraph::_cache;

VoxelGeneratorGraph::VoxelGeneratorGraph() {}

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

ProgramGraph::Node *create_node_internal(ProgramGraph &graph, VoxelGeneratorGraph::NodeTypeID type_id, Vector2 position,
		uint32_t id, bool create_default_instances) {
	const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton().get_type(type_id);

	ProgramGraph::Node *node = graph.create_node(type_id, id);
	ERR_FAIL_COND_V(node == nullptr, nullptr);
	node->inputs.resize(type.inputs.size());
	node->outputs.resize(type.outputs.size());
	node->default_inputs.resize(type.inputs.size());
	node->gui_position = position;

	node->params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const VoxelGraphNodeDB::Param &param = type.params[i];

		if (param.class_name.is_empty()) {
			node->params[i] = param.default_value;

		} else if (param.default_value_func != nullptr && create_default_instances) {
			node->params[i] = param.default_value_func();
		}
	}

	node->autoconnect_default_inputs = type.has_autoconnect_inputs();

	for (size_t i = 0; i < type.inputs.size(); ++i) {
		node->default_inputs[i] = type.inputs[i].default_value;
	}

	return node;
}

uint32_t VoxelGeneratorGraph::create_node(NodeTypeID type_id, Vector2 position, uint32_t id) {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton().is_valid_type_id(type_id), ProgramGraph::NULL_ID);
	const ProgramGraph::Node *node = create_node_internal(_graph, type_id, position, id, true);
	ERR_FAIL_COND_V(node == nullptr, ProgramGraph::NULL_ID);
	// Register resources if any were created by default
	for (const Variant &v : node->params) {
		if (v.get_type() == Variant::OBJECT) {
			Ref<Resource> res = v;
			if (res.is_valid()) {
				register_subresource(**res);
			} else {
				ZN_PRINT_WARNING("Non-resource object found in node parameter");
			}
		}
	}
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

bool VoxelGeneratorGraph::is_valid_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	const ProgramGraph::PortLocation src_port{ src_node_id, src_port_index };
	const ProgramGraph::PortLocation dst_port{ dst_node_id, dst_port_index };
	ERR_FAIL_COND_V(!_graph.is_output_port_valid(src_port), false);
	ERR_FAIL_COND_V(!_graph.is_input_port_valid(dst_port), false);
	return _graph.is_valid_connection(src_port, dst_port);
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
	const ProgramGraph::Node &node = _graph.get_node(dst.node_id);
	CRASH_COND(dst.port_index >= node.inputs.size());
	const ProgramGraph::Port &port = node.inputs[dst.port_index];
	if (port.connections.size() == 0) {
		return false;
	}
	// There can be at most one inbound connection
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
			ZN_PRINT_ERROR(format("More than one graph node has the name \"{}\"", GodotStringWrapper(String(name))));
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

bool VoxelGeneratorGraph::get_expression_variables(std::string_view code, std::vector<std::string_view> &vars) {
	Span<const ExpressionParser::Function> functions =
			VoxelGraphNodeDB::get_singleton().get_expression_parser_functions();
	ExpressionParser::Result result = ExpressionParser::parse(code, functions);
	if (result.error.id == ExpressionParser::ERROR_NONE) {
		if (result.root != nullptr) {
			ExpressionParser::find_variables(*result.root, vars);
		}
		return true;
	} else {
		return false;
	}
}

void VoxelGeneratorGraph::get_expression_node_inputs(uint32_t node_id, std::vector<std::string> &out_names) const {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND(node->type_id != NODE_EXPRESSION);
	for (unsigned int i = 0; i < node->inputs.size(); ++i) {
		const ProgramGraph::Port &port = node->inputs[i];
		ERR_FAIL_COND(!port.is_dynamic());
		out_names.push_back(port.dynamic_name);
	}
}

inline bool has_duplicate(const PackedStringArray &sa) {
	return find_duplicate(Span<const String>(sa.ptr(), sa.size())) != size_t(sa.size());
}

void VoxelGeneratorGraph::set_expression_node_inputs(uint32_t node_id, PackedStringArray names) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);

	// Validate
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND(node->type_id != NODE_EXPRESSION);
	for (int i = 0; i < names.size(); ++i) {
		const String name = names[i];
		ERR_FAIL_COND(!name.is_valid_identifier());
	}
	ERR_FAIL_COND(has_duplicate(names));
	for (unsigned int i = 0; i < node->inputs.size(); ++i) {
		const ProgramGraph::Port &port = node->inputs[i];
		// Sounds annoying if you call this from a script, but this is supposed to be editor functionality for now
		ERR_FAIL_COND_MSG(port.connections.size() > 0,
				ZN_TTR("Cannot change input ports if connections exist, disconnect them first."));
	}

	node->inputs.resize(names.size());
	node->default_inputs.resize(names.size());
	for (int i = 0; i < names.size(); ++i) {
		const String name = names[i];
		const CharString name_utf8 = name.utf8();
		node->inputs[i].dynamic_name = name_utf8.get_data();
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
	Variant &defval = node->default_inputs[input_index];
	if (defval != value) {
		//node->autoconnect_default_inputs = false;
		defval = value;
		emit_changed();
	}
}

bool VoxelGeneratorGraph::get_node_default_inputs_autoconnect(uint32_t node_id) const {
	const ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND_V(node == nullptr, false);
	return node->autoconnect_default_inputs;
}

void VoxelGeneratorGraph::set_node_default_inputs_autoconnect(uint32_t node_id, bool enabled) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	if (node->autoconnect_default_inputs != enabled) {
		node->autoconnect_default_inputs = enabled;
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

PackedInt32Array VoxelGeneratorGraph::get_node_ids() const {
	PackedInt32Array ids;
	{
		_graph.for_each_node_id([&ids](int id) {
			// Not resizing up-front, because the code to assign elements is different between Godot modules and
			// GDExtension
			ids.append(id);
		});
	}
	return ids;
}

unsigned int VoxelGeneratorGraph::get_nodes_count() const {
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
	_sdf_clip_threshold = math::max(t, 0.f);
}

int VoxelGeneratorGraph::get_used_channels_mask() const {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V(runtime_ptr == nullptr, 0);
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
		const VoxelGraphRuntime::State &state, Vector3i rmin, Vector3i rmax, int ry,
		VoxelBufferInternal &out_voxel_buffer, FixedArray<uint8_t, 4> spare_indices) {
	ZN_PROFILE_SCOPE();

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
					weights[oi] = math::clamp(weight * 255.f, 0.f, 255.f);
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
						encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
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
void gather_indices_and_weights_from_single_texture(unsigned int output_buffer_index,
		const VoxelGraphRuntime::State &state, Vector3i rmin, Vector3i rmax, int ry,
		VoxelBufferInternal &out_voxel_buffer) {
	ZN_PROFILE_SCOPE();

	const VoxelGraphRuntime::Buffer &buffer = state.get_buffer(output_buffer_index);
	Span<const float> buffer_data = Span<const float>(buffer.data, buffer.size);

	// TODO Should not really be here, but may work. Left here for now so all code for this is in one place
	const uint16_t encoded_weights = encode_weights_to_packed_u16(255, 0, 0, 0);
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

static void fill_zx_sdf_slice(const VoxelGraphRuntime::Buffer &sdf_buffer, VoxelBufferInternal &out_buffer,
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

static void fill_zx_integer_slice(const VoxelGraphRuntime::Buffer &src_buffer, VoxelBufferInternal &out_buffer,
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

	Cache &cache = _cache;

	// Slice is on the Y axis
	const unsigned int slice_buffer_size = section_size.x * section_size.z;
	VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, slice_buffer_size, false);

	cache.x_cache.resize(slice_buffer_size);
	cache.y_cache.resize(slice_buffer_size);
	cache.z_cache.resize(slice_buffer_size);
	cache.input_sdf_cache.resize(slice_buffer_size);

	Span<float> x_cache = to_span(cache.x_cache);
	Span<float> y_cache = to_span(cache.y_cache);
	Span<float> z_cache = to_span(cache.z_cache);
	Span<float> input_sdf_cache = to_span(cache.input_sdf_cache);

	const float air_sdf = _debug_clipped_blocks ? -1.f : 1.f;
	const float matter_sdf = _debug_clipped_blocks ? 1.f : -1.f;

	FixedArray<uint8_t, 4> spare_texture_indices = runtime_ptr->spare_texture_indices;
	const int sdf_output_buffer_index = runtime_ptr->sdf_output_buffer_index;
	const int type_output_buffer_index = runtime_ptr->type_output_buffer_index;

	FixedArray<unsigned int, VoxelGraphRuntime::MAX_OUTPUTS> required_outputs;
	unsigned int required_outputs_count = 0;

	bool all_sdf_is_air = (sdf_output_buffer_index != -1) && (type_output_buffer_index == -1);
	bool all_sdf_is_matter = all_sdf_is_air;

	math::Interval sdf_input_range;
	if (runtime.has_input(VoxelGeneratorGraph::NODE_INPUT_SDF)) {
		ZN_PROFILE_SCOPE();
		// const Vector3i bs = out_buffer.get_size();
		// const float midv = out_buffer.get_voxel_f(bs / 2, VoxelBufferInternal::CHANNEL_SDF) / sdf_scale;
		// const float maxd = 0.501f * math::length(to_vec3f(bs << input.lod));
		// sdf_input_range = math::Interval(midv - maxd, midv + maxd);

		get_unscaled_sdf(out_buffer, input_sdf_cache);

		sdf_input_range = math::Interval::from_single_value(input_sdf_cache[0]);
		for (unsigned int i = 0; i < input_sdf_cache.size(); ++i) {
			sdf_input_range.add_point(input_sdf_cache[i]);
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
				runtime.analyze_range(cache.state, gmin, gmax, sdf_input_range);

				bool sdf_is_air = true;
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
					}

					all_sdf_is_air = all_sdf_is_air && sdf_is_air;
					all_sdf_is_matter = all_sdf_is_matter && sdf_is_matter;
				}

				if (type_output_buffer_index != -1) {
					const math::Interval type_range = cache.state.get_range(type_output_buffer_index);
					if (type_range.is_single_value()) {
						out_buffer.fill_area(int(type_range.min), rmin, rmax, type_channel);
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

				if (runtime_ptr->single_texture_output_index != -1 && !sdf_is_air) {
					const math::Interval index_range = cache.state.get_range(runtime_ptr->single_texture_output_index);
					if (index_range.is_single_value()) {
						out_buffer.fill_area(int(index_range.min), rmin, rmax, type_channel);
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

					// Full query (unless using execution map)
					runtime.generate_set(cache.state, x_cache, y_cache, z_cache, input_sdf_cache,
							_use_xz_caching && ry != rmin.y,
							_use_optimized_execution_map ? &cache.optimized_execution_map : nullptr);

					if (sdf_output_buffer_index != -1) {
						const VoxelGraphRuntime::Buffer &sdf_buffer = cache.state.get_buffer(sdf_output_buffer_index);
						fill_zx_sdf_slice(
								sdf_buffer, out_buffer, sdf_channel, sdf_channel_depth, sdf_scale, rmin, rmax, ry);
					}

					if (type_output_buffer_index != -1) {
						const VoxelGraphRuntime::Buffer &type_buffer = cache.state.get_buffer(type_output_buffer_index);
						fill_zx_integer_slice(
								type_buffer, out_buffer, type_channel, type_channel_depth, rmin, rmax, ry);
					}

					if (runtime_ptr->single_texture_output_index != -1) {
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
		const VoxelGraphRuntime &runtime, const ProgramGraph &graph, VoxelGeneratorGraph::NodeTypeID node_type_id) {
	for (unsigned int other_output_index = 0; other_output_index < runtime.get_output_count(); ++other_output_index) {
		const VoxelGraphRuntime::OutputInfo output = runtime.get_output_info(other_output_index);
		const ProgramGraph::Node &node = graph.get_node(output.node_id);
		if (node.type_id == VoxelGeneratorGraph::NODE_OUTPUT_WEIGHT) {
			return true;
		}
	}
	return false;
}

VoxelGraphRuntime::CompilationResult VoxelGeneratorGraph::compile(bool debug) {
	const int64_t time_before = Time::get_singleton()->get_ticks_usec();

	std::shared_ptr<Runtime> r = make_shared_instance<Runtime>();
	VoxelGraphRuntime &runtime = r->runtime;

	// Core compilation
	const VoxelGraphRuntime::CompilationResult result = runtime.compile(_graph, debug);

	if (!result.success) {
		return result;
	}

	// Extra steps
	for (unsigned int output_index = 0; output_index < runtime.get_output_count(); ++output_index) {
		const VoxelGraphRuntime::OutputInfo output = runtime.get_output_info(output_index);
		const ProgramGraph::Node &node = _graph.get_node(output.node_id);

		// TODO Allow specifying max count in VoxelGraphNodeDB so we can make some of these checks more generic
		switch (node.type_id) {
			case NODE_OUTPUT_SDF:
				if (r->sdf_output_buffer_index != -1) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple SDF outputs are not supported");
					error.node_id = output.node_id;
					return error;
				} else {
					r->sdf_output_index = output_index;
					r->sdf_output_buffer_index = output.buffer_address;
				}
				break;

			case NODE_OUTPUT_WEIGHT: {
				if (r->weight_outputs_count >= r->weight_outputs.size()) {
					VoxelGraphRuntime::CompilationResult error;
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
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = String(ZN_TTR("Cannot use negative layer index in weight output"));
					error.node_id = output.node_id;
					return error;
				}
				if (layer_index >= static_cast<int>(r->weight_outputs.size())) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message =
							String(ZN_TTR("Weight layers cannot exceed {0}")).format(varray(r->weight_outputs.size()));
					error.node_id = output.node_id;
					return error;
				}
				for (unsigned int i = 0; i < r->weight_outputs_count; ++i) {
					const WeightOutput &wo = r->weight_outputs[i];
					if (static_cast<int>(wo.layer_index) == layer_index) {
						VoxelGraphRuntime::CompilationResult error;
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

			case NODE_OUTPUT_TYPE:
				if (r->type_output_buffer_index != -1) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple TYPE outputs are not supported");
					error.node_id = output.node_id;
					return error;
				} else {
					r->type_output_index = output_index;
					r->type_output_buffer_index = output.buffer_address;
				}
				break;

			case NODE_OUTPUT_SINGLE_TEXTURE:
				if (r->single_texture_output_buffer_index != -1) {
					VoxelGraphRuntime::CompilationResult error;
					error.success = false;
					error.message = ZN_TTR("Multiple TYPE outputs are not supported");
					error.node_id = output.node_id;
					return error;
				}
				if (has_output_type(runtime, _graph, VoxelGeneratorGraph::NODE_OUTPUT_WEIGHT)) {
					VoxelGraphRuntime::CompilationResult error;
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
		VoxelGraphRuntime::CompilationResult error;
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
		//debug_check_texture_indices(spare_indices);
		ERR_FAIL_COND_V(spare_indices_count != 4, VoxelGraphRuntime::CompilationResult());
		r->spare_texture_indices = spare_indices;
	}

	// Store valid result
	RWLockWrite wlock(_runtime_lock);
	_runtime = r;

	const int64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(format("Voxel graph compiled in {} us", time_spent));

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
	Cache &cache = _cache;
	VoxelGraphRuntime &runtime = _runtime->runtime;

	// Support graphs having an SDF input, give it default values
	Span<float> in_sdf;
	if (runtime.has_input(NODE_INPUT_SDF)) {
		cache.input_sdf_cache.resize(in_x.size());
		in_sdf = to_span(cache.input_sdf_cache);
		in_sdf.fill(0.f);
	}

	runtime.prepare_state(cache.state, in_x.size(), false);
	runtime.generate_set(cache.state, in_x, in_y, in_z, in_sdf, false, nullptr);
	// Note, when generating SDF, we don't scale it because the return values are uncompressed floats. Scale only
	// matters if we are storing it inside 16-bit or 8-bit VoxelBuffer.
}

void VoxelGeneratorGraph::generate_series(Span<float> in_x, Span<float> in_y, Span<float> in_z, Span<float> in_sdf) {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND(_runtime == nullptr);
	Cache &cache = _cache;
	VoxelGraphRuntime &runtime = _runtime->runtime;

	runtime.prepare_state(cache.state, in_x.size(), false);
	runtime.generate_set(cache.state, in_x, in_y, in_z, in_sdf, false, nullptr);
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

	const VoxelGraphRuntime::Buffer &buffer = _cache.state.get_buffer(buffer_index);
	memcpy(out_values.data(), buffer.data, sizeof(float) * out_values.size());
}

const VoxelGraphRuntime::State &VoxelGeneratorGraph::get_last_state_from_current_thread() {
	return _cache.state;
}

Span<const uint32_t> VoxelGeneratorGraph::get_last_execution_map_debug_from_current_thread() {
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

int VoxelGeneratorGraph::get_sdf_output_port_address() const {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND_V(_runtime == nullptr, -1);
	return _runtime->sdf_output_buffer_index;
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
	ERR_FAIL_COND_MSG(runtime_ptr->sdf_output_buffer_index == -1, "This function only works with an SDF output.");

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> in_sdf_cache;
		Ref<Image> im;
		const VoxelGraphRuntime &runtime;
		VoxelGraphRuntime::State &state;
		const unsigned int sdf_buffer_index;
		const float ref_radius;
		const float sdf_min;
		const float sdf_max;

		ProcessChunk(VoxelGraphRuntime::State &p_state, unsigned int p_sdf_buffer_index,
				const VoxelGraphRuntime &p_runtime, float p_ref_radius, float p_sdf_min, float p_sdf_max) :
				runtime(p_runtime),
				state(p_state),
				sdf_buffer_index(p_sdf_buffer_index),
				ref_radius(p_ref_radius),
				sdf_min(p_sdf_min),
				sdf_max(p_sdf_max) {}

		void operator()(int x0, int y0, int width, int height) {
			ZN_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			runtime.prepare_state(state, area, false);

			const Vector2 suv =
					Vector2(1.f / static_cast<float>(im->get_width()), 1.f / static_cast<float>(im->get_height()));

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
			if (runtime.has_input(NODE_INPUT_SDF)) {
				in_sdf_cache.resize(x_coords.size());
				in_sdf = to_span(in_sdf_cache);
				in_sdf.fill(0.f);
			}

			runtime.generate_set(
					state, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf, false, nullptr);
			const VoxelGraphRuntime::Buffer &buffer = state.get_buffer(sdf_buffer_index);

			// Calculate final pixels
			// TODO Optimize: could convert to buffer directly?
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float sdf = buffer.data[i];
					const float nh = (-sdf - sdf_min) * nr;
					im->set_pixel(ix, iy, Color(nh, nh, nh));
					++i;
				}
			}
		}
	};

	Cache &cache = _cache;

	ProcessChunk pc(
			cache.state, runtime_ptr->sdf_output_buffer_index, runtime_ptr->runtime, ref_radius, sdf_min, sdf_max);
	pc.im = im;
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
		unsigned int sdf_buffer_index;
		Ref<Image> im;
		const VoxelGraphRuntime &runtime;
		VoxelGraphRuntime::State &state;
		const float strength;
		const float ref_radius;

		ProcessChunk(VoxelGraphRuntime::State &p_state, unsigned int p_sdf_buffer_index, Ref<Image> p_im,
				const VoxelGraphRuntime &p_runtime, float p_strength, float p_ref_radius) :
				sdf_buffer_index(p_sdf_buffer_index),
				im(p_im),
				runtime(p_runtime),
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
			runtime.prepare_state(state, area, false);

			const float ns = 2.f / strength;

			const Vector2 suv =
					Vector2(1.f / static_cast<float>(im->get_width()), 1.f / static_cast<float>(im->get_height()));

			const Vector2 normal_step = 0.5f * Vector2(1.f, 1.f) / im->get_size();
			const Vector2 normal_step_x = Vector2(normal_step.x, 0.f);
			const Vector2 normal_step_y = Vector2(0.f, normal_step.y);

			const int xmax = x0 + width;
			const int ymax = y0 + height;

			const VoxelGraphRuntime::Buffer &sdf_buffer = state.get_buffer(sdf_buffer_index);

			// Support graphs having an SDF input, give it default values
			Span<float> in_sdf;
			if (runtime.has_input(NODE_INPUT_SDF)) {
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
			// TODO Perform range analysis on the range of coordinates, it might still yield performance benefits
			runtime.generate_set(
					state, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf, false, nullptr);
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
			runtime.generate_set(
					state, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf, false, nullptr);
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
			runtime.generate_set(
					state, to_span(x_coords), to_span(y_coords), to_span(z_coords), in_sdf, false, nullptr);
			CRASH_COND(sdf_values_py.size() != sdf_buffer.size);
			memcpy(sdf_values_py.data(), sdf_buffer.data, sdf_values_py.size() * sizeof(float));

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
					im->set_pixel(ix, iy, en);
				}
			}
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

String VoxelGeneratorGraph::generate_shader() {
	ZN_PROFILE_SCOPE();

	std::string code_utf8;
	VoxelGraphRuntime::CompilationResult result = zylann::voxel::generate_shader(_graph, code_utf8);

	ERR_FAIL_COND_V_MSG(!result.success, "", result.message);

	return String(code_utf8.c_str());
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
	Cache &cache = _cache;
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	runtime.prepare_state(cache.state, 1, false);
	runtime.generate_single(cache.state, to_vec3f(position), nullptr);
	const VoxelGraphRuntime::Buffer &buffer = cache.state.get_buffer(runtime_ptr->sdf_output_buffer_index);
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
	Cache &cache = _cache;
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;
	// Note, buffer size is irrelevant here, because range analysis doesn't use buffers
	runtime.prepare_state(cache.state, 1, false);
	runtime.analyze_range(cache.state, min_pos, max_pos, math::Interval());
	if (optimize_execution_map) {
		runtime.generate_optimized_execution_map(cache.state, cache.optimized_execution_map, true);
	}
	// TODO Change return value to allow checking other outputs
	if (runtime_ptr->sdf_output_buffer_index != -1) {
		return cache.state.get_range(runtime_ptr->sdf_output_buffer_index);
	}
	return math::Interval();
}

Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instantiate();

	d->_graph.copy_from(_graph, p_subresources);
	d->register_subresources();
	// Program not copied, as it may contain pointers to the resources we are duplicating

	return d;
}

static Dictionary get_graph_as_variant_data(const ProgramGraph &graph) {
	Dictionary nodes_data;
	graph.for_each_node_id([&graph, &nodes_data](uint32_t node_id) {
		const ProgramGraph::Node *node = graph.try_get_node(node_id);
		ERR_FAIL_COND(node == nullptr);

		Dictionary node_data;

		const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton().get_type(node->type_id);
		node_data["type"] = type.name;
		node_data["gui_position"] = node->gui_position;

		if (node->name != StringName()) {
			node_data["name"] = node->name;
		}

		for (size_t j = 0; j < type.params.size(); ++j) {
			const VoxelGraphNodeDB::Param &param = type.params[j];
			node_data[param.name] = node->params[j];
		}

		// Static inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			if (node->inputs[j].connections.size() == 0) {
				const VoxelGraphNodeDB::Port &port = type.inputs[j];
				node_data[port.name] = node->default_inputs[j];
			}
		}

		if (node->autoconnect_default_inputs) {
			node_data["auto_connect"] = true;
		}

		// Dynamic inputs. Order matters.
		Array dynamic_inputs_data;
		for (size_t j = 0; j < node->inputs.size(); ++j) {
			const ProgramGraph::Port &port = node->inputs[j];
			if (port.is_dynamic()) {
				Array d;
				d.resize(2);
				d[0] = String(port.dynamic_name.c_str());
				d[1] = node->default_inputs[j];
				dynamic_inputs_data.append(d);
			}
		}

		if (dynamic_inputs_data.size() > 0) {
			node_data["dynamic_inputs"] = dynamic_inputs_data;
		}

		String key = String::num_uint64(node_id);
		nodes_data[key] = node_data;
	});

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
	data["version"] = 2;
	return data;
}

Dictionary VoxelGeneratorGraph::get_graph_as_variant_data() const {
	return zylann::voxel::get_graph_as_variant_data(_graph);
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
	const VoxelGraphNodeDB &type_db = VoxelGraphNodeDB::get_singleton();

	// Can't iterate using `next()`, because in GDExtension, there doesn't seem to be a way to do so.
	const Array nodes_data_keys = nodes_data.keys();

	for (int nodes_data_key_index = 0; nodes_data_key_index < nodes_data_keys.size(); ++nodes_data_key_index) {
		const Variant id_key = nodes_data_keys[nodes_data_key_index];
		const String id_str = id_key;
		ERR_FAIL_COND_V(!id_str.is_valid_int(), false);
		const int sid = id_str.to_int();
		ERR_FAIL_COND_V(sid < static_cast<int>(ProgramGraph::NULL_ID), false);
		const uint32_t id = sid;

		Dictionary node_data = nodes_data[id_key];
		ERR_FAIL_COND_V(node_data.is_empty(), false);

		const String type_name = node_data["type"];
		const Vector2 gui_position = node_data["gui_position"];
		VoxelGeneratorGraph::NodeTypeID type_id;
		ERR_FAIL_COND_V(!type_db.try_get_type_id_from_name(type_name, type_id), false);
		// Don't create default param values, they will be assigned from serialized data
		ProgramGraph::Node *node = create_node_internal(graph, type_id, gui_position, id, false);
		ERR_FAIL_COND_V(node == nullptr, false);
		// TODO Graphs made in older versions must have autoconnect always off

		Variant auto_connect_v = node_data.get("auto_connect", Variant());
		if (auto_connect_v != Variant()) {
			node->autoconnect_default_inputs = auto_connect_v;
		}

		// Can't iterate using `next()`, because in GDExtension, there doesn't seem to be a way to do so.
		const Array node_data_keys = node_data.keys();

		for (int node_data_key_index = 0; node_data_key_index < node_data_keys.size(); ++node_data_key_index) {
			const Variant param_key = node_data_keys[node_data_key_index];
			const String param_name = param_key;
			if (param_name == "type") {
				continue;
			}
			if (param_name == "gui_position") {
				continue;
			}
			if (param_name == "auto_connect") {
				continue;
			}
			if (param_name == "dynamic_inputs") {
				const Array dynamic_inputs_data = node_data[param_key];

				for (int dpi = 0; dpi < dynamic_inputs_data.size(); ++dpi) {
					const Array d = dynamic_inputs_data[dpi];
					ERR_FAIL_COND_V(d.size() != 2, false);

					const String dynamic_param_name = d[0];
					ProgramGraph::Port dport;
					CharString dynamic_param_name_utf8 = dynamic_param_name.utf8();
					dport.dynamic_name = dynamic_param_name_utf8.get_data();

					const Variant defval = d[1];
					node->default_inputs.push_back(defval);

					node->inputs.push_back(dport);
				}

				continue;
			}
			uint32_t param_index;
			if (type_db.try_get_param_index_from_name(type_id, param_name, param_index)) {
				ERR_CONTINUE(param_index < 0 || param_index >= node->params.size());
				node->params[param_index] = node_data[param_key];
			}
			if (type_db.try_get_input_index_from_name(type_id, param_name, param_index)) {
				ERR_CONTINUE(param_index < 0 || param_index >= node->default_inputs.size());
				node->default_inputs[param_index] = node_data[param_key];
			}
			const Variant vname = node_data.get("name", Variant());
			if (vname != Variant()) {
				node->name = vname;
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

	if (zylann::voxel::load_graph_from_variant_data(_graph, data)) {
		register_subresources();
		// It's possible to auto-compile on load because `graph_data` is the only property set by the loader,
		// which is enough to have all information we need
		compile(Engine::get_singleton()->is_editor_hint());

	} else {
		_graph.clear();
	}
}

void VoxelGeneratorGraph::register_subresource(Resource &resource) {
	//print_line(String("{0}: Registering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.connect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorGraph, _on_subresource_changed));
}

void VoxelGeneratorGraph::unregister_subresource(Resource &resource) {
	//print_line(String("{0}: Unregistering subresource {1}").format(varray(int64_t(this), int64_t(&resource))));
	resource.disconnect(VoxelStringNames::get_singleton().changed,
			ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorGraph, _on_subresource_changed));
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

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel(
		bool singular, std::vector<NodeProfilingInfo> *node_profiling_info) {
	std::shared_ptr<const Runtime> runtime_ptr;
	{
		RWLockRead rlock(_runtime_lock);
		runtime_ptr = _runtime;
	}
	ERR_FAIL_COND_V_MSG(runtime_ptr == nullptr, 0.f, "The graph hasn't been compiled yet");
	const VoxelGraphRuntime &runtime = runtime_ptr->runtime;

	const uint32_t cube_size = 16;
	const uint32_t cube_count = 250;
	// const uint32_t cube_size = 100;
	// const uint32_t cube_count = 1;
	const uint32_t voxel_count = cube_size * cube_size * cube_size * cube_count;
	ProfilingClock profiling_clock;
	uint64_t total_elapsed_us = 0;

	Cache &cache = _cache;

	if (singular) {
		runtime.prepare_state(cache.state, 1, false);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t z = 0; z < cube_size; ++z) {
				for (uint32_t y = 0; y < cube_size; ++y) {
					for (uint32_t x = 0; x < cube_size; ++x) {
						runtime.generate_single(cache.state, Vector3f(x, y, z), nullptr);
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

		const bool per_node_profiling = node_profiling_info != nullptr;
		runtime.prepare_state(cache.state, sx.size(), per_node_profiling);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t y = 0; y < cube_size; ++y) {
				runtime.generate_set(cache.state, sx, sy, sz, ssdf, false, nullptr);
			}

			total_elapsed_us += profiling_clock.restart();
		}

		if (per_node_profiling) {
			const VoxelGraphRuntime::ExecutionMap &execution_map = runtime.get_default_execution_map();
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

#ifdef TOOLS_ENABLED

void VoxelGeneratorGraph::get_configuration_warnings(PackedStringArray &out_warnings) const {
	if (get_nodes_count() == 0) {
		// Making a `String` explicitely because in GDExtension `get_class_static` is a `const char*`
		out_warnings.append(String(VoxelGeneratorGraph::get_class_static()) + " is empty.");
		return;
	}

	if (!is_good()) {
		// Making a `String` explicitely because in GDExtension `get_class_static` is a `const char*`
		out_warnings.append(String(VoxelGeneratorGraph::get_class_static()) + " contains errors.");
		return;
	}
}

uint64_t VoxelGeneratorGraph::get_output_graph_hash() const {
	const VoxelGraphNodeDB &type_db = VoxelGraphNodeDB::get_singleton();
	std::vector<uint32_t> terminal_nodes;

	// Not using the generic `get_terminal_nodes` function because our terminal nodes do have outputs
	_graph.for_each_node_const([&terminal_nodes, &type_db](const ProgramGraph::Node &node) {
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node.type_id);
		if (type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT) {
			terminal_nodes.push_back(node.id);
		}
	});

	// Sort for determinism
	std::sort(terminal_nodes.begin(), terminal_nodes.end());

	std::vector<uint32_t> order;
	_graph.find_dependencies(terminal_nodes, order);

	std::vector<uint64_t> node_hashes;
	uint64_t hash = hash_djb2_one_64(0);

	for (uint32_t node_id : order) {
		ProgramGraph::Node &node = _graph.get_node(node_id);
		hash = hash_djb2_one_64(node.type_id, hash);

		for (const Variant &v : node.params) {
			if (v.get_type() == Variant::OBJECT) {
				const Object *obj = v.operator Object *();
				if (obj != nullptr) {
					// Note, the obtained hash can change here even if the result is identical, because it's hard to
					// tell which properties contribute to the result. This should be rare though.
					hash = hash_djb2_one_64(get_deep_hash(*obj), hash);
				}
			} else {
				hash = hash_djb2_one_64(v.hash(), hash);
			}
		}

		for (const Variant &v : node.default_inputs) {
			hash = hash_djb2_one_64(v.hash(), hash);
		}

		for (const ProgramGraph::Port &port : node.inputs) {
			for (const ProgramGraph::PortLocation src : port.connections) {
				hash = hash_djb2_one_64(uint64_t(src.node_id) | (uint64_t(src.port_index) << 32), hash);
			}
		}
	}

	return hash;
}

#endif // TOOLS_ENABLED

// Binding land

int VoxelGeneratorGraph::_b_get_node_type_count() const {
	return VoxelGraphNodeDB::get_singleton().get_type_count();
}

Dictionary VoxelGeneratorGraph::_b_get_node_type_info(int type_id) const {
	ERR_FAIL_COND_V(!VoxelGraphNodeDB::get_singleton().is_valid_type_id(type_id), Dictionary());
	return VoxelGraphNodeDB::get_singleton().get_type_info_dict(type_id);
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
	VoxelGraphRuntime::CompilationResult res = compile(false);
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

void VoxelGeneratorGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGeneratorGraph::clear);
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"), &VoxelGeneratorGraph::create_node,
			DEFVAL(ProgramGraph::NULL_ID));
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
	ClassDB::bind_method(
			D_METHOD("set_node_param", "node_id", "param_index", "value"), &VoxelGeneratorGraph::set_node_param);
	ClassDB::bind_method(
			D_METHOD("get_node_default_input", "node_id", "input_index"), &VoxelGeneratorGraph::get_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_default_input", "node_id", "input_index", "value"),
			&VoxelGeneratorGraph::set_node_default_input);
	ClassDB::bind_method(D_METHOD("get_node_default_inputs_autoconnect", "node_id"),
			&VoxelGeneratorGraph::get_node_default_inputs_autoconnect);
	ClassDB::bind_method(D_METHOD("set_node_default_inputs_autoconnect", "node_id", "enabled"),
			&VoxelGeneratorGraph::set_node_default_inputs_autoconnect);
	ClassDB::bind_method(
			D_METHOD("set_node_param_null", "node_id", "param_index"), &VoxelGeneratorGraph::_b_set_node_param_null);
	ClassDB::bind_method(D_METHOD("get_node_gui_position", "node_id"), &VoxelGeneratorGraph::get_node_gui_position);
	ClassDB::bind_method(
			D_METHOD("set_node_gui_position", "node_id", "position"), &VoxelGeneratorGraph::set_node_gui_position);
	ClassDB::bind_method(D_METHOD("get_node_name", "node_id"), &VoxelGeneratorGraph::get_node_name);
	ClassDB::bind_method(D_METHOD("set_node_name", "node_id", "name"), &VoxelGeneratorGraph::set_node_name);
	ClassDB::bind_method(D_METHOD("set_expression_node_inputs", "node_id", "names"),
			&VoxelGeneratorGraph::set_expression_node_inputs);

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

	ClassDB::bind_method(D_METHOD("get_node_type_count"), &VoxelGeneratorGraph::_b_get_node_type_count);
	ClassDB::bind_method(D_METHOD("get_node_type_info", "type_id"), &VoxelGeneratorGraph::_b_get_node_type_info);

	//ClassDB::bind_method(D_METHOD("generate_single"), &VoxelGeneratorGraph::_b_generate_single);
	ClassDB::bind_method(
			D_METHOD("debug_analyze_range", "min_pos", "max_pos"), &VoxelGeneratorGraph::_b_debug_analyze_range);

	ClassDB::bind_method(D_METHOD("bake_sphere_bumpmap", "im", "ref_radius", "sdf_min", "sdf_max"),
			&VoxelGeneratorGraph::bake_sphere_bumpmap);
	ClassDB::bind_method(D_METHOD("bake_sphere_normalmap", "im", "ref_radius", "strength"),
			&VoxelGeneratorGraph::bake_sphere_normalmap);
	ClassDB::bind_method(D_METHOD("generate_shader"), &VoxelGeneratorGraph::generate_shader);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &VoxelGeneratorGraph::debug_load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel", "use_singular_queries"),
			&VoxelGeneratorGraph::_b_debug_measure_microseconds_per_voxel);

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
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2_3D);
#endif
	BIND_ENUM_CONSTANT(NODE_OUTPUT_SINGLE_TEXTURE);
	BIND_ENUM_CONSTANT(NODE_EXPRESSION);
	BIND_ENUM_CONSTANT(NODE_POWI);
	BIND_ENUM_CONSTANT(NODE_POW);
	BIND_ENUM_CONSTANT(NODE_INPUT_SDF);
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}

} // namespace zylann::voxel
