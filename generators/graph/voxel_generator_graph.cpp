#include "voxel_generator_graph.h"
#include "../../util/profiling_clock.h"
#include "voxel_graph_node_db.h"

#include <core/core_string_names.h>

VoxelGeneratorGraph::VoxelGeneratorGraph() {
	clear();
	clear_bounds();
	_bounds.min = Vector3i(-128);
	_bounds.max = Vector3i(128);
}

VoxelGeneratorGraph::~VoxelGeneratorGraph() {
	clear();
}

void VoxelGeneratorGraph::clear() {
	_graph.clear();
	_runtime.clear();
}

uint32_t VoxelGeneratorGraph::create_node(NodeTypeID type_id, Vector2 position, uint32_t id) {
	const ProgramGraph::Node *node = create_node_internal(type_id, position, id);
	ERR_FAIL_COND_V(node == nullptr, ProgramGraph::NULL_ID);
	return node->id;
}

ProgramGraph::Node *VoxelGeneratorGraph::create_node_internal(NodeTypeID type_id, Vector2 position, uint32_t id) {
	const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(type_id);

	ProgramGraph::Node *node = _graph.create_node(type_id, id);
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

void VoxelGeneratorGraph::remove_node(uint32_t node_id) {
	_graph.remove_node(node_id);
	emit_changed();
}

bool VoxelGeneratorGraph::can_connect(uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	return _graph.can_connect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
}

void VoxelGeneratorGraph::add_connection(uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	_graph.connect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
	emit_changed();
}

void VoxelGeneratorGraph::remove_connection(uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	_graph.disconnect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
	emit_changed();
}

void VoxelGeneratorGraph::get_connections(std::vector<ProgramGraph::Connection> &connections) const {
	_graph.get_connections(connections);
}

//void VoxelGeneratorGraph::get_connections_from_and_to(std::vector<ProgramGraph::Connection> &connections, uint32_t node_id) const {
//	_graph.get_connections_from_and_to(connections, node_id);
//}

bool VoxelGeneratorGraph::try_get_connection_to(ProgramGraph::PortLocation dst, ProgramGraph::PortLocation &out_src) const {
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

void VoxelGeneratorGraph::set_node_param(uint32_t node_id, uint32_t param_index, Variant value) {
	ProgramGraph::Node *node = _graph.try_get_node(node_id);
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_INDEX(param_index, node->params.size());

	// TODO Changing sub-resources won't trigger a change signal
	// It's actually very annoying to setup and keep correct. Needs to be done cautiously.

	// Ref<Resource> res = node->params[param_index];
	// if (res.is_valid()) {
	// 	res->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_subresource_changed");
	// }

	if (node->params[param_index] != value) {
		node->params[param_index] = value;
		emit_changed();
	}

	// res = value;
	// if (res.is_valid()) {
	// 	res->connect(CoreStringNames::get_singleton()->changed, this, "_on_subresource_changed");
	// }
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
		// Moving nodes around doesn't functionally change the graph
		//emit_changed();
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

int VoxelGeneratorGraph::get_used_channels_mask() const {
	return 1 << _channel;
}

void VoxelGeneratorGraph::generate_block(VoxelBlockRequest &input) {
	VoxelBuffer &out_buffer = **input.voxel_buffer;

	const Vector3i bs = out_buffer.get_size();
	const VoxelBuffer::ChannelId channel = _channel;
	const Vector3i origin = input.origin_in_voxels;

	const Vector3i rmin;
	const Vector3i rmax = bs;
	const Vector3i gmin = origin;
	const Vector3i gmax = origin + (bs << input.lod);

	switch (_bounds.type) {
		case BOUNDS_NONE:
			break;

		case BOUNDS_VERTICAL:
			if (origin.y > _bounds.max.y) {
				out_buffer.clear_channel(VoxelBuffer::CHANNEL_TYPE, _bounds.type_value1);
				out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, _bounds.sdf_value1);
				return;

			} else if (origin.y + (bs.y << input.lod) < _bounds.min.y) {
				out_buffer.clear_channel(VoxelBuffer::CHANNEL_TYPE, _bounds.type_value0);
				out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, _bounds.sdf_value0);
				return;
			}
			// TODO Not sure if it's actually worth doing this? Here we can even have bounds in the same block
			//
			//			rmax.y = clamp((_bounds.max.y - origin.y) >> input.lod, 0, bs.y);
			//			rmin.y = clamp((_bounds.min.y - origin.y) >> input.lod, 0, bs.y);
			//			gmin += rmin << input.lod;
			break;

		case BOUNDS_BOX:
			if (!Rect3i::from_min_max(_bounds.min, _bounds.max).intersects(Rect3i(origin, bs << input.lod))) {
				out_buffer.clear_channel(VoxelBuffer::CHANNEL_TYPE, _bounds.type_value0);
				out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, _bounds.sdf_value0);
				return;
			}
			//			rmin.x = clamp((_bounds.min.x - origin.x) >> input.lod, 0, bs.x);
			//			rmin.y = clamp((_bounds.min.y - origin.y) >> input.lod, 0, bs.y);
			//			rmin.z = clamp((_bounds.min.z - origin.z) >> input.lod, 0, bs.z);
			//			rmax.x = clamp((_bounds.max.x - origin.x) >> input.lod, 0, bs.x);
			//			rmax.y = clamp((_bounds.max.y - origin.y) >> input.lod, 0, bs.y);
			//			rmax.z = clamp((_bounds.max.z - origin.z) >> input.lod, 0, bs.z);
			//			gmin += rmin << input.lod;
			break;

		default:
			CRASH_NOW();
			break;
	}

	Interval range = analyze_range(gmin, gmax);
	const float clip_threshold = 1.f;
	if (range.min > clip_threshold && range.max > clip_threshold) {
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, 1.f);
		return;

	} else if (range.min < -clip_threshold && range.max < -clip_threshold) {
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, -1.f);
		return;

	} else if (range.is_single_value()) {
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, range.min);
		return;
	}

	const int stride = 1 << input.lod;

	Vector3i rpos;
	Vector3i gpos;
	// Loads of possible optimization from there

	for (rpos.z = rmin.z, gpos.z = gmin.z; rpos.z < rmax.z; ++rpos.z, gpos.z += stride) {
		for (rpos.x = rmin.x, gpos.x = gmin.x; rpos.x < rmax.x; ++rpos.x, gpos.x += stride) {
			for (rpos.y = rmin.y, gpos.y = gmin.y; rpos.y < rmax.y; ++rpos.y, gpos.y += stride) {
				out_buffer.set_voxel_f(generate_single(gpos), rpos.x, rpos.y, rpos.z, channel);
			}
		}
	}

	out_buffer.compress_uniform_channels();
}

void VoxelGeneratorGraph::compile() {
	_runtime.compile(_graph, Engine::get_singleton()->is_editor_hint());
}

float VoxelGeneratorGraph::generate_single(const Vector3i &position) {
	switch (_bounds.type) {
		case BOUNDS_NONE:
			break;

		case BOUNDS_VERTICAL:
			if (position.y >= _bounds.max.y) {
				return _bounds.sdf_value1;
			}
			if (position.y < _bounds.min.y) {
				return _bounds.sdf_value0;
			}
			break;

		case BOUNDS_BOX:
			if (
					position.x < _bounds.min.x ||
					position.y < _bounds.min.y ||
					position.z < _bounds.min.z ||
					position.x >= _bounds.max.x ||
					position.y >= _bounds.max.y ||
					position.z >= _bounds.max.z) {

				return _bounds.sdf_value0;
			}
			break;

		default:
			CRASH_NOW();
			break;
	}

	return _runtime.generate_single(position) * _iso_scale;
}

Interval VoxelGeneratorGraph::analyze_range(Vector3i min_pos, Vector3i max_pos) {
	return _runtime.analyze_range(min_pos, max_pos) * _iso_scale;
}

void VoxelGeneratorGraph::clear_bounds() {
	_bounds.type = BOUNDS_NONE;
}

void VoxelGeneratorGraph::set_vertical_bounds(int min_y, int max_y,
		float bottom_sdf_value, float top_sdf_value,
		uint64_t bottom_type_value, uint64_t top_type_value) {

	_bounds.type = BOUNDS_VERTICAL;
	_bounds.min = Vector3i(0, min_y, 0);
	_bounds.max = Vector3i(0, max_y, 0);
	_bounds.sdf_value0 = bottom_sdf_value;
	_bounds.sdf_value1 = top_sdf_value;
	_bounds.type_value0 = bottom_type_value;
	_bounds.type_value1 = top_type_value;
}

void VoxelGeneratorGraph::set_box_bounds(Vector3i min, Vector3i max, float sdf_value, uint64_t type_value) {
	Vector3i::sort_min_max(min, max);
	_bounds.type = BOUNDS_BOX;
	_bounds.min = min;
	_bounds.max = max;
	_bounds.sdf_value0 = sdf_value;
	_bounds.sdf_value1 = type_value;
}

Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instance();

	d->_channel = _channel;
	d->_iso_scale = _iso_scale;
	d->_bounds = _bounds;
	d->_graph.copy_from(_graph, p_subresources);
	// Program not copied, as it may contain pointers to the resources we are duplicating

	d->compile();

	return d;
}

Dictionary VoxelGeneratorGraph::get_graph_as_variant_data() {
	Dictionary nodes_data;
	PoolVector<int> node_ids = _graph.get_node_ids();
	{
		PoolVector<int>::Read r = node_ids.read();
		for (int i = 0; i < node_ids.size(); ++i) {
			uint32_t node_id = r[i];
			const ProgramGraph::Node *node = _graph.get_node(node_id);
			ERR_FAIL_COND_V(node == nullptr, Dictionary());

			Dictionary node_data;

			const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node->type_id);
			node_data["type"] = type.name;
			node_data["gui_position"] = node->gui_position;

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
	_graph.get_connections(connections);
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

static bool var_to_id(Variant v, uint32_t &out_id, uint32_t min = 0) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT, false);
	int i = v;
	ERR_FAIL_COND_V(i < 0 || (unsigned int)i < min, false);
	out_id = i;
	return true;
}

void VoxelGeneratorGraph::load_graph_from_variant_data(Dictionary data) {
	Dictionary nodes_data = data["nodes"];
	Array connections_data = data["connections"];
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();

	const Variant *id_key = nullptr;
	while ((id_key = nodes_data.next(id_key))) {
		const String id_str = *id_key;
		ERR_FAIL_COND(!id_str.is_valid_integer());
		const int sid = id_str.to_int();
		ERR_FAIL_COND(sid < static_cast<int>(ProgramGraph::NULL_ID));
		const uint32_t id = sid;

		Dictionary node_data = nodes_data[*id_key];

		const String type_name = node_data["type"];
		const Vector2 gui_position = node_data["gui_position"];
		NodeTypeID type_id;
		ERR_FAIL_COND(!type_db.try_get_type_id_from_name(type_name, type_id));
		ProgramGraph::Node *node = create_node_internal(type_id, gui_position, id);
		ERR_FAIL_COND(node == nullptr);

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
		}
	}

	for (int i = 0; i < connections_data.size(); ++i) {
		Array con_data = connections_data[i];
		ERR_FAIL_COND(con_data.size() != 4);
		ProgramGraph::PortLocation src;
		ProgramGraph::PortLocation dst;
		ERR_FAIL_COND(!var_to_id(con_data[0], src.node_id, ProgramGraph::NULL_ID));
		ERR_FAIL_COND(!var_to_id(con_data[1], src.port_index));
		ERR_FAIL_COND(!var_to_id(con_data[2], dst.node_id, ProgramGraph::NULL_ID));
		ERR_FAIL_COND(!var_to_id(con_data[3], dst.port_index));
		_graph.connect(src, dst);
	}

	// It's possible to auto-compile on load because `graph_data` is the only property set by the loader,
	// which is enough to have all information we need
	compile();
}

// Debug land

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel() {
	// Need to query varying positions to avoid some optimizations to kick in
	FixedArray<Vector3i, 2> random_positions;
	random_positions[0] = Vector3i(1, 1, 1);
	random_positions[1] = Vector3i(2, 2, 2);

	const uint32_t iterations = 1000000;
	ProfilingClock profiling_clock;
	profiling_clock.restart();

	for (uint32_t i = 0; i < iterations; ++i) {
		generate_single(random_positions[i & 1]);
	}

	uint64_t ius = profiling_clock.restart();
	float us = static_cast<double>(ius) / iterations;
	// print_line(String("Time: {0}us").format(varray(us)));
	return us;
}

void VoxelGeneratorGraph::debug_load_waves_preset() {
	clear();
	// This is mostly for testing

	const Vector2 k(35, 50);

	uint32_t n_x = create_node(NODE_INPUT_X, Vector2(11, 1) * k); // 1
	uint32_t n_y = create_node(NODE_INPUT_Y, Vector2(37, 1) * k); // 2
	uint32_t n_z = create_node(NODE_INPUT_Z, Vector2(11, 5) * k); // 3
	uint32_t n_o = create_node(NODE_OUTPUT_SDF, Vector2(45, 3) * k); // 4
	uint32_t n_sin0 = create_node(NODE_SIN, Vector2(23, 1) * k); // 5
	uint32_t n_sin1 = create_node(NODE_SIN, Vector2(23, 5) * k); // 6
	uint32_t n_add = create_node(NODE_ADD, Vector2(27, 3) * k); // 7
	uint32_t n_mul0 = create_node(NODE_MULTIPLY, Vector2(17, 1) * k); // 8
	uint32_t n_mul1 = create_node(NODE_MULTIPLY, Vector2(17, 5) * k); // 9
	uint32_t n_mul2 = create_node(NODE_MULTIPLY, Vector2(33, 3) * k); // 10
	uint32_t n_c0 = create_node(NODE_CONSTANT, Vector2(14, 3) * k); // 11
	uint32_t n_c1 = create_node(NODE_CONSTANT, Vector2(30, 5) * k); // 12
	uint32_t n_sub = create_node(NODE_SUBTRACT, Vector2(39, 3) * k); // 13

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

bool VoxelGeneratorGraph::_set(const StringName &p_name, const Variant &p_value) {
	const String name = p_name;

	struct L {
		inline static bool set_xyz(char c, Vector3i &p, int v) {
			int i = c - 'x';
			ERR_FAIL_COND_V(i < 0 || (unsigned int)i >= Vector3i::AXIS_COUNT, false);
			p[i] = v;
			return true;
		}
	};

	if (name.begins_with("bounds/")) {
		const String sub = name.right(7);

		if (sub == "type") {
			const BoundsType type = (BoundsType)(p_value.operator int());
			ERR_FAIL_INDEX_V(type, BOUNDS_TYPE_COUNT, false);
			_bounds.type = type;
			_change_notify();
			return true;

		} else if (sub.begins_with("min_") && sub.length() == 5) {
			// Not using Vector3 because floats can't contain big integer values
			ERR_FAIL_COND_V(!L::set_xyz(sub[4], _bounds.min, p_value), false);
			Vector3i::sort_min_max(_bounds.min, _bounds.max);
			return true;

		} else if (sub.begins_with("max_") && sub.length() == 5) {
			ERR_FAIL_COND_V(!L::set_xyz(sub[4], _bounds.max, p_value), false);
			Vector3i::sort_min_max(_bounds.min, _bounds.max);
			return true;

		} else if (sub == "sdf_value" || sub == "bottom_sdf_value") {
			_bounds.sdf_value0 = p_value;
			return true;

		} else if (sub == "type_value" || sub == "bottom_type_value") {
			_bounds.type_value0 = p_value;
			return true;

		} else if (sub == "top_sdf_value") {
			_bounds.sdf_value1 = p_value;
			return true;

		} else if (sub == "top_type_value1") {
			_bounds.type_value1 = p_value;
			return true;
		}
	}

	return false;
}

bool VoxelGeneratorGraph::_get(const StringName &p_name, Variant &r_ret) const {
	const String name = p_name;

	struct L {
		inline static bool get_xyz(char c, const Vector3i &p, Variant &r) {
			int i = c - 'x';
			ERR_FAIL_COND_V(i < 0 || (unsigned int)i >= Vector3i::AXIS_COUNT, false);
			r = p[i];
			return true;
		}
	};

	if (name.begins_with("bounds/")) {
		const String sub = name.right(7);

		if (sub == "type") {
			r_ret = _bounds.type;
			return true;

		} else if (sub.begins_with("min_") && sub.length() == 5) {
			return L::get_xyz(sub[4], _bounds.min, r_ret);

		} else if (sub.begins_with("max_") && sub.length() == 5) {
			return L::get_xyz(sub[4], _bounds.max, r_ret);

		} else if (sub == "sdf_value" || sub == "bottom_sdf_value") {
			r_ret = _bounds.sdf_value0;
			return true;

		} else if (sub == "type_value" || sub == "bottom_type_value") {
			r_ret = _bounds.type_value0;
			return true;

		} else if (sub == "top_sdf_value") {
			r_ret = _bounds.sdf_value1;
			return true;

		} else if (sub == "top_type_value1") {
			r_ret = _bounds.type_value1;
			return true;
		}
	}

	return false;
}

void VoxelGeneratorGraph::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "bounds/type", PROPERTY_HINT_ENUM, "None,Vertical,Box"));

	switch (_bounds.type) {
		case BOUNDS_NONE:
			break;

		case BOUNDS_VERTICAL:
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/min_y"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/max_y"));
			p_list->push_back(PropertyInfo(Variant::REAL, "bounds/top_sdf_value"));
			p_list->push_back(PropertyInfo(Variant::REAL, "bounds/bottom_sdf_value"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/top_type_value"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/bottom_type_value"));
			break;

		case BOUNDS_BOX:
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/min_x"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/min_y"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/min_z"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/max_x"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/max_y"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/max_z"));
			p_list->push_back(PropertyInfo(Variant::REAL, "bounds/sdf_value"));
			p_list->push_back(PropertyInfo(Variant::INT, "bounds/type_value"));
			break;

		default:
			CRASH_NOW();
			break;
	}
}

int VoxelGeneratorGraph::_b_get_node_type_count() const {
	return VoxelGraphNodeDB::get_singleton()->get_type_count();
}

Dictionary VoxelGeneratorGraph::_b_get_node_type_info(int type_id) const {
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

float VoxelGeneratorGraph::_b_generate_single(Vector3 pos) {
	return generate_single(Vector3i(pos));
}

// void VoxelGeneratorGraph::_on_subresource_changed() {
// 	emit_changed();
// }

void VoxelGeneratorGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &VoxelGeneratorGraph::clear);
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"), &VoxelGeneratorGraph::create_node, DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("remove_node", "node_id"), &VoxelGeneratorGraph::remove_node);
	ClassDB::bind_method(D_METHOD("can_connect", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"), &VoxelGeneratorGraph::can_connect);
	ClassDB::bind_method(D_METHOD("add_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"), &VoxelGeneratorGraph::add_connection);
	ClassDB::bind_method(D_METHOD("remove_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"), &VoxelGeneratorGraph::remove_connection);
	ClassDB::bind_method(D_METHOD("get_connections"), &VoxelGeneratorGraph::_b_get_connections);
	ClassDB::bind_method(D_METHOD("get_node_ids"), &VoxelGeneratorGraph::get_node_ids);

	ClassDB::bind_method(D_METHOD("get_node_type_id", "node_id"), &VoxelGeneratorGraph::get_node_type_id);
	ClassDB::bind_method(D_METHOD("get_node_param", "node_id", "param_index"), &VoxelGeneratorGraph::get_node_param);
	ClassDB::bind_method(D_METHOD("set_node_param", "node_id", "param_index", "value"), &VoxelGeneratorGraph::set_node_param);
	ClassDB::bind_method(D_METHOD("get_node_default_input", "node_id", "input_index"), &VoxelGeneratorGraph::get_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_default_input", "node_id", "input_index", "value"), &VoxelGeneratorGraph::set_node_default_input);
	ClassDB::bind_method(D_METHOD("set_node_param_null", "node_id", "param_index"), &VoxelGeneratorGraph::_b_set_node_param_null);
	ClassDB::bind_method(D_METHOD("get_node_gui_position", "node_id"), &VoxelGeneratorGraph::get_node_gui_position);
	ClassDB::bind_method(D_METHOD("set_node_gui_position", "node_id", "position"), &VoxelGeneratorGraph::set_node_gui_position);

	ClassDB::bind_method(D_METHOD("compile"), &VoxelGeneratorGraph::compile);

	ClassDB::bind_method(D_METHOD("get_node_type_count"), &VoxelGeneratorGraph::_b_get_node_type_count);
	ClassDB::bind_method(D_METHOD("get_node_type_info", "type_id"), &VoxelGeneratorGraph::_b_get_node_type_info);

	ClassDB::bind_method(D_METHOD("generate_single"), &VoxelGeneratorGraph::_b_generate_single);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &VoxelGeneratorGraph::debug_load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel"), &VoxelGeneratorGraph::debug_measure_microseconds_per_voxel);

	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &VoxelGeneratorGraph::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &VoxelGeneratorGraph::get_graph_as_variant_data);

	// ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGeneratorGraph::_on_subresource_changed);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

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
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}
