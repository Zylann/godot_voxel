#include "voxel_generator_graph.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "voxel_graph_node_db.h"

#include <core/core_string_names.h>

const char *VoxelGeneratorGraph::SIGNAL_NODE_NAME_CHANGED = "node_name_changed";

VoxelGeneratorGraph::VoxelGeneratorGraph() {
	clear();
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

bool VoxelGeneratorGraph::can_connect(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) const {
	return _graph.can_connect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
}

void VoxelGeneratorGraph::add_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	_graph.connect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
	emit_changed();
}

void VoxelGeneratorGraph::remove_connection(
		uint32_t src_node_id, uint32_t src_port_index, uint32_t dst_node_id, uint32_t dst_port_index) {
	_graph.disconnect(
			ProgramGraph::PortLocation{ src_node_id, src_port_index },
			ProgramGraph::PortLocation{ dst_node_id, dst_port_index });
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
		node->params[param_index] = value;
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

int VoxelGeneratorGraph::get_used_channels_mask() const {
	return 1 << _channel;
}

void VoxelGeneratorGraph::generate_block(VoxelBlockRequest &input) {
	if (!_runtime.has_output()) {
		return;
	}

	VoxelBuffer &out_buffer = **input.voxel_buffer;

	const Vector3i bs = out_buffer.get_size();
	const VoxelBuffer::ChannelId channel = _channel;
	const Vector3i origin = input.origin_in_voxels;

	const Vector3i rmin;
	const Vector3i rmax = bs;
	const Vector3i gmin = origin;
	const Vector3i gmax = origin + (bs << input.lod);

	// TODO This may be shared across the module
	// Storing voxels is lossy on some depth configurations. They use normalized SDF,
	// so we must scale the values to make better use of the offered resolution
	float sdf_scale;
	switch (out_buffer.get_channel_depth(VoxelBuffer::CHANNEL_SDF)) {
		// Normalized
		case VoxelBuffer::DEPTH_8_BIT:
			sdf_scale = 0.1f;
			break;
		case VoxelBuffer::DEPTH_16_BIT:
			sdf_scale = 0.002f;
			break;
		// Direct
		case VoxelBuffer::DEPTH_32_BIT:
			sdf_scale = 1.0f;
			break;
		case VoxelBuffer::DEPTH_64_BIT:
			sdf_scale = 1.0f;
			break;
		default:
			CRASH_NOW();
			break;
	}

	// TODO Add subdivide option so we can fine tune the analysis

	const Interval range = _runtime.analyze_range(gmin, gmax) * sdf_scale;
	const float clip_threshold = sdf_scale * 0.2f;
	if (range.min > clip_threshold && range.max > clip_threshold) {
		//out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, 1.f);
		// DEBUG: use this instead to fill optimized-out blocks with matter, making them stand out
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, -1.f);
		return;

	} else if (range.min < -clip_threshold && range.max < -clip_threshold) {
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, -1.f);
		return;

	} else if (range.is_single_value()) {
		out_buffer.clear_channel_f(VoxelBuffer::CHANNEL_SDF, range.min);
		return;
	}

	_slice_cache.resize(bs.x * bs.y);
	ArraySlice<float> slice_cache(_slice_cache, 0, _slice_cache.size());

	_x_cache.resize(_slice_cache.size());
	_y_cache.resize(_slice_cache.size());
	_z_cache.resize(_slice_cache.size());

	ArraySlice<float> x_cache(_x_cache, 0, _x_cache.size());
	ArraySlice<float> y_cache(_y_cache, 0, _y_cache.size());
	ArraySlice<float> z_cache(_z_cache, 0, _z_cache.size());

	const int stride = 1 << input.lod;

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

	for (int ry = rmin.y, gy = gmin.y; ry < rmax.y; ++ry, gy += (1 << input.lod)) {
		VOXEL_PROFILE_SCOPE();

		y_cache.fill(gy);

		_runtime.generate_set(x_cache, y_cache, z_cache, slice_cache, ry != rmin.y);

		// TODO Flatten this further
		unsigned int i = 0;
		for (int rz = rmin.z; rz < rmax.z; ++rz) {
			for (int rx = rmin.x; rx < rmax.x; ++rx) {
				out_buffer.set_voxel_f(sdf_scale * slice_cache[i], rx, ry, rz, channel);
				++i;
			}
		}
	}

	out_buffer.compress_uniform_channels();
}

bool VoxelGeneratorGraph::compile() {
	return _runtime.compile(_graph, Engine::get_singleton()->is_editor_hint());
}

void VoxelGeneratorGraph::generate_set(ArraySlice<float> in_x, ArraySlice<float> in_y, ArraySlice<float> in_z,
		ArraySlice<float> out_sdf) {

	_runtime.generate_set(in_x, in_y, in_z, out_sdf, false);
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

void VoxelGeneratorGraph::bake_sphere_bumpmap(Ref<Image> im, float ref_radius, float sdf_min, float sdf_max) {
	ERR_FAIL_COND(im.is_null());

	const Vector2 suv(
			1.f / static_cast<float>(im->get_width()),
			1.f / static_cast<float>(im->get_height()));

	const float nr = 1.f / (sdf_max - sdf_min);

	im->lock();

	for (int iy = 0; iy < im->get_height(); ++iy) {
		for (int ix = 0; ix < im->get_width(); ++ix) {
			const Vector2 uv = suv * Vector2(ix, iy);
			const Vector3 pos = get_3d_pos_from_panorama_uv(uv) * ref_radius;
			// TODO Need to convert this to buffers instead of using single queries
			const float sdf = _runtime.generate_single(pos);
			const float nh = (-sdf - sdf_min) * nr;
			im->set_pixel(ix, iy, Color(nh, nh, nh));
		}
	}

	im->unlock();
}

// If this generator is used to produce a planet, specifically using a spherical heightmap approach,
// then this function can be used to bake a map of the surface.
// Such maps can be used by shaders to sharpen the details of the planet when seen from far away.
void VoxelGeneratorGraph::bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength) {
	ERR_FAIL_COND(im.is_null());

	const Vector2 suv(
			1.f / static_cast<float>(im->get_width()),
			1.f / static_cast<float>(im->get_height()));

	const Vector2 normal_step = 0.5f * Vector2(1.f, 1.f) / im->get_size();
	const Vector2 normal_step_x = Vector2(normal_step.x, 0.f);
	const Vector2 normal_step_y = Vector2(0.f, normal_step.y);

	// The default for strength is 1.f
	const float e = 0.001f;
	if (strength > -e && strength < e) {
		if (strength > 0.f) {
			strength = e;
		} else {
			strength = -e;
		}
	}

	const float ns = 2.f / strength;

	im->lock();

	for (int iy = 0; iy < im->get_height(); ++iy) {
		for (int ix = 0; ix < im->get_width(); ++ix) {
			// TODO There is probably a more optimized way to do this
			const Vector2 uv = suv * Vector2(ix, iy);

			const Vector3 np_nx = get_3d_pos_from_panorama_uv(uv - normal_step_x) * ref_radius;
			const Vector3 np_px = get_3d_pos_from_panorama_uv(uv + normal_step_x) * ref_radius;
			const Vector3 np_ny = get_3d_pos_from_panorama_uv(uv - normal_step_y) * ref_radius;
			const Vector3 np_py = get_3d_pos_from_panorama_uv(uv + normal_step_y) * ref_radius;

			// TODO Need to convert this to buffers instead of using single queries
			const float h_nx = -_runtime.generate_single(np_nx);
			const float h_px = -_runtime.generate_single(np_px);
			const float h_ny = -_runtime.generate_single(np_ny);
			const float h_py = -_runtime.generate_single(np_py);

			const Vector3 normal = Vector3(h_nx - h_px, ns, h_ny - h_py).normalized();
			const Color en(
					0.5f * normal.x + 0.5f,
					-0.5f * normal.z + 0.5f,
					0.5f * normal.y + 0.5f);
			im->set_pixel(ix, iy, en);
		}
	}

	im->unlock();
}

float VoxelGeneratorGraph::generate_single(const Vector3i &position) {
	if (!_runtime.has_output()) {
		return 1.f;
	}

	return _runtime.generate_single(position.to_vec3());
}

Interval VoxelGeneratorGraph::analyze_range(Vector3i min_pos, Vector3i max_pos) {
	return _runtime.analyze_range(min_pos, max_pos);
}

Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instance();

	d->_channel = _channel;
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
			const Variant *vname = node_data.getptr("name");
			if (vname != nullptr) {
				node->name = *vname;
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

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel(bool singular) {
	if (!_runtime.has_output()) {
		return -1;
	}

	const uint32_t cube_size = 16;
	const uint32_t cube_count = 250;
	// const uint32_t cube_size = 100;
	// const uint32_t cube_count = 1;
	const uint32_t voxel_count = cube_size * cube_size * cube_size * cube_count;
	ProfilingClock profiling_clock;
	uint64_t elapsed_us = 0;

	if (singular) {
		for (int i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			// Note: intentionally iterating in XYZ instead of XZY to avoid XZ optimization to kick in
			for (uint32_t z = 0; z < cube_size; ++z) {
				for (uint32_t y = 0; y < cube_size; ++y) {
					for (uint32_t x = 0; x < cube_size; ++x) {
						_runtime.generate_single(Vector3i(x, y, z).to_vec3());
					}
				}
			}

			elapsed_us += profiling_clock.restart();
		}

	} else {
		std::vector<float> dst;
		dst.resize(cube_size * cube_size);
		std::vector<float> src_x;
		std::vector<float> src_y;
		std::vector<float> src_z;
		src_x.resize(dst.size());
		src_y.resize(dst.size());
		src_z.resize(dst.size());
		ArraySlice<float> sx(src_x, 0, src_x.size());
		ArraySlice<float> sy(src_y, 0, src_y.size());
		ArraySlice<float> sz(src_z, 0, src_z.size());
		ArraySlice<float> sdst(dst, 0, dst.size());

		for (int i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t y = 0; y < cube_size; ++y) {
				_runtime.generate_set(sx, sy, sz, sdst, false);
			}

			elapsed_us += profiling_clock.restart();
		}
	}

	float us = static_cast<double>(elapsed_us) / voxel_count;
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

	ClassDB::bind_method(D_METHOD("compile"), &VoxelGeneratorGraph::compile);

	ClassDB::bind_method(D_METHOD("get_node_type_count"), &VoxelGeneratorGraph::_b_get_node_type_count);
	ClassDB::bind_method(D_METHOD("get_node_type_info", "type_id"), &VoxelGeneratorGraph::_b_get_node_type_info);

	ClassDB::bind_method(D_METHOD("generate_single"), &VoxelGeneratorGraph::_b_generate_single);

	ClassDB::bind_method(D_METHOD("bake_sphere_bumpmap", "im", "ref_radius", "sdf_min", "sdf_max"),
			&VoxelGeneratorGraph::bake_sphere_bumpmap);
	ClassDB::bind_method(D_METHOD("bake_sphere_normalmap", "im", "ref_radius", "strength"),
			&VoxelGeneratorGraph::bake_sphere_normalmap);

	ClassDB::bind_method(D_METHOD("debug_load_waves_preset"), &VoxelGeneratorGraph::debug_load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel", "use_singular_queries"),
			&VoxelGeneratorGraph::debug_measure_microseconds_per_voxel);

	ClassDB::bind_method(D_METHOD("_set_graph_data", "data"), &VoxelGeneratorGraph::load_graph_from_variant_data);
	ClassDB::bind_method(D_METHOD("_get_graph_data"), &VoxelGeneratorGraph::get_graph_as_variant_data);

	// ClassDB::bind_method(D_METHOD("_on_subresource_changed"), &VoxelGeneratorGraph::_on_subresource_changed);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "graph_data", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
			"_set_graph_data", "_get_graph_data");

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
	BIND_ENUM_CONSTANT(NODE_NORMALIZE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_3D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_2D);
	BIND_ENUM_CONSTANT(NODE_FAST_NOISE_GRADIENT_3D);
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}
