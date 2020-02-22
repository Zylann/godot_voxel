#include "voxel_generator_graph.h"
#include "../../util/profiling_clock.h"
#include "../../voxel_string_names.h"
#include <modules/opensimplex/open_simplex_noise.h>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

namespace {
VoxelGeneratorGraph::NodeTypeDB *g_node_type_db = nullptr;
}

VoxelGeneratorGraph::NodeTypeDB *VoxelGeneratorGraph::NodeTypeDB::get_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	return g_node_type_db;
}

void VoxelGeneratorGraph::NodeTypeDB::create_singleton() {
	CRASH_COND(g_node_type_db != nullptr);
	g_node_type_db = memnew(NodeTypeDB());
}

void VoxelGeneratorGraph::NodeTypeDB::destroy_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	memdelete(g_node_type_db);
	g_node_type_db = nullptr;
}

VoxelGeneratorGraph::NodeTypeDB::NodeTypeDB() {
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CONSTANT];
		t.outputs.push_back(Port("Value"));
		t.params.push_back(Param("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_X];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Y];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Z];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_OUTPUT_SDF];
		t.inputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ADD];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SUBTRACT];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MULTIPLY];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Product"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SINE];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_FLOOR];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ABS];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SQRT];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_2D];
		t.inputs.push_back(Port("X0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("X1"));
		t.inputs.push_back(Port("Y1"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_3D];
		t.inputs.push_back(Port("X0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("X1"));
		t.inputs.push_back(Port("Y1"));
		t.inputs.push_back(Port("Z1"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CLAMP];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min", -1.f));
		t.params.push_back(Param("Max", 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MIX];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.inputs.push_back(Port("Ratio"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_REMAP];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min0", -1.f));
		t.params.push_back(Param("Max0", 1.f));
		t.params.push_back(Param("Min1", -1.f));
		t.params.push_back(Param("Max1", 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CURVE];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Curve"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_2D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_3D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.inputs.push_back(Port("Z"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_IMAGE_2D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Image"));
	}
}

VoxelGeneratorGraph::VoxelGeneratorGraph() {
	clear_bounds();
	_bounds.min = Vector3i(-128);
	_bounds.max = Vector3i(128);
	load_waves_preset();
	compile();
}

VoxelGeneratorGraph::~VoxelGeneratorGraph() {
	clear();
}

void VoxelGeneratorGraph::clear() {
	const uint32_t *key = nullptr;
	while ((key = _nodes.next(key))) {
		Node *node = _nodes.get(*key);
		CRASH_COND(node == nullptr);
		memdelete(node);
	}
	_nodes.clear();
	_graph.clear();

	_program.clear();
	_memory.clear();
}

uint32_t VoxelGeneratorGraph::create_node(NodeTypeID type_id) {
	const NodeTypeDB::NodeType &type = NodeTypeDB::get_singleton()->types[type_id];

	ProgramGraph::Node *pg_node = _graph.create_node();
	pg_node->inputs.resize(type.inputs.size());
	pg_node->outputs.resize(type.outputs.size());

	Node *node = memnew(Node);
	node->type = type_id;
	node->params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		node->params[i] = type.params[i].default_value;
	}
	_nodes[pg_node->id] = node;

	return pg_node->id;
}

void VoxelGeneratorGraph::remove_node(uint32_t node_id) {
	_graph.remove_node(node_id);

	Node **pptr = _nodes.getptr(node_id);
	if (pptr != nullptr) {
		Node *node = *pptr;
		memdelete(node);
		_nodes.erase(node_id);
	}
}

void VoxelGeneratorGraph::node_connect(ProgramGraph::PortLocation src, ProgramGraph::PortLocation dst) {
	_graph.connect(src, dst);
}

void VoxelGeneratorGraph::node_set_param(uint32_t node_id, uint32_t param_index, Variant value) {
	Node **pptr = _nodes.getptr(node_id);
	ERR_FAIL_COND(pptr == nullptr);
	Node *node = *pptr;
	node->params[param_index] = value;
}

int VoxelGeneratorGraph::get_used_channels_mask() const {
	return 1 << _channel;
}

void VoxelGeneratorGraph::load_waves_preset() {
	clear();
	// This is mostly for testing

	uint32_t n_x = create_node(NODE_INPUT_X);
	uint32_t n_y = create_node(NODE_INPUT_Y);
	uint32_t n_z = create_node(NODE_INPUT_Z);
	uint32_t n_o = create_node(NODE_OUTPUT_SDF);
	uint32_t n_sin0 = create_node(NODE_SINE);
	uint32_t n_sin1 = create_node(NODE_SINE);
	uint32_t n_add = create_node(NODE_ADD);
	uint32_t n_mul0 = create_node(NODE_MULTIPLY);
	uint32_t n_mul1 = create_node(NODE_MULTIPLY);
	uint32_t n_mul2 = create_node(NODE_MULTIPLY);
	uint32_t n_c0 = create_node(NODE_CONSTANT);
	uint32_t n_c1 = create_node(NODE_CONSTANT);
	uint32_t n_sub = create_node(NODE_SUBTRACT);

	node_set_param(n_c0, 0, 1.f / 20.f);
	node_set_param(n_c1, 0, 10.f);

	/*
	 *    X --- * --- sin           Y
	 *         /         \           \
	 *       1/20         + --- * --- - --- O
	 *         \         /     /
	 *    Z --- * --- sin    10.0
	*/

	typedef ProgramGraph::PortLocation PL;

	node_connect(PL{ n_x, 0 }, PL{ n_mul0, 0 });
	node_connect(PL{ n_z, 0 }, PL{ n_mul1, 0 });
	node_connect(PL{ n_c0, 0 }, PL{ n_mul0, 1 });
	node_connect(PL{ n_c0, 0 }, PL{ n_mul1, 1 });
	node_connect(PL{ n_mul0, 0 }, PL{ n_sin0, 0 });
	node_connect(PL{ n_mul1, 0 }, PL{ n_sin1, 0 });
	node_connect(PL{ n_sin0, 0 }, PL{ n_add, 0 });
	node_connect(PL{ n_sin1, 0 }, PL{ n_add, 1 });
	node_connect(PL{ n_add, 0 }, PL{ n_mul2, 0 });
	node_connect(PL{ n_c1, 0 }, PL{ n_mul2, 1 });
	node_connect(PL{ n_y, 0 }, PL{ n_sub, 0 });
	node_connect(PL{ n_mul2, 0 }, PL{ n_sub, 1 });
	node_connect(PL{ n_sub, 0 }, PL{ n_o, 0 });
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
	// TODO Range analysis
	// TODO XZ-only dependency optimization

	for (rpos.z = rmin.z, gpos.z = gmin.z; rpos.z < rmax.z; ++rpos.z, gpos.z += stride) {
		for (rpos.x = rmin.x, gpos.x = gmin.x; rpos.x < rmax.x; ++rpos.x, gpos.x += stride) {
			for (rpos.y = rmin.y, gpos.y = gmin.y; rpos.y < rmax.y; ++rpos.y, gpos.y += stride) {

				out_buffer.set_voxel_f(generate_single(gpos), rpos.x, rpos.y, rpos.z, channel);
			}
		}
	}

	out_buffer.compress_uniform_channels();
}

template <typename T>
inline void write_static(std::vector<uint8_t> &mem, uint32_t p, const T &v) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p + sizeof(T) >= mem.size());
#endif
	*(T *)(&mem[p]) = v;
}

template <typename T>
inline void append(std::vector<uint8_t> &mem, const T &v) {
	size_t p = mem.size();
	mem.resize(p + sizeof(T));
	*(T *)(&mem[p]) = v;
}

template <typename T>
inline const T &read(const std::vector<uint8_t> &mem, uint32_t &p) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p + sizeof(T) > mem.size());
#endif
	const T *v = (const T *)&mem[p];
	p += sizeof(T);
	return *v;
}

inline float get_pixel_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

inline float squared(float x) {
	return x * x;
}

Interval get_curve_range(Curve &curve, uint8_t &is_monotonic_increasing) {
	// TODO Would be nice to have the cache directly
	const int res = curve.get_bake_resolution();
	Interval range;
	float prev_v = curve.interpolate_baked(0.f);
	if (curve.interpolate_baked(1.f) > prev_v) {
		is_monotonic_increasing = 1;
	}
	for (int i = 0; i < res; ++i) {
		const float v = curve.interpolate_baked(static_cast<float>(i) / res);
		range.add_point(v);
		if (v < prev_v) {
			is_monotonic_increasing = 0;
		}
		prev_v = v;
	}
	return range;
}

Interval get_heightmap_range(Image &im) {
	switch (im.get_format()) {
		case Image::FORMAT_R8:
		case Image::FORMAT_RG8:
		case Image::FORMAT_RGB8:
		case Image::FORMAT_RGBA8:
		case Image::FORMAT_RH:
		case Image::FORMAT_RGH:
		case Image::FORMAT_RGBH:
		case Image::FORMAT_RGBAH:
		case Image::FORMAT_RF:
		case Image::FORMAT_RGF:
		case Image::FORMAT_RGBF:
		case Image::FORMAT_RGBAF: {
			Interval r;
			im.lock();
			for (int y = 0; y < im.get_height(); ++y) {
				for (int x = 0; x < im.get_width(); ++x) {
					r.add_point(im.get_pixel(x, y).r);
				}
			}
			im.unlock();
			return r;
		} break;

		default:
			ERR_FAIL_V_MSG(Interval(), "Image format not supported");
			break;
	}
	return Interval();
}

void VoxelGeneratorGraph::compile() {
	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;

	_graph.find_terminal_nodes(terminal_nodes);
	// For now only 1 end is supported
	ERR_FAIL_COND(terminal_nodes.size() != 1);

	_graph.find_dependencies(terminal_nodes.back(), order);

	_program.clear();

	// Main inputs X, Y, Z
	_memory.resize(3);

	std::vector<uint8_t> &program = _program;
	const NodeTypeDB &type_db = *NodeTypeDB::get_singleton();
	HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> output_port_addresses;
	bool has_output = false;

	for (size_t i = 0; i < order.size(); ++i) {
		const uint32_t node_id = order[i];
		const ProgramGraph::Node *pg_node = _graph.get_node(node_id);
		const Node *node = _nodes[node_id];
		const NodeTypeDB::NodeType &type = type_db.types[node->type];

		CRASH_COND(node == nullptr);
		CRASH_COND(pg_node->inputs.size() != type.inputs.size());
		CRASH_COND(pg_node->outputs.size() != type.outputs.size());

		switch (node->type) {
			case NODE_CONSTANT: {
				CRASH_COND(type.outputs.size() != 1);
				CRASH_COND(type.params.size() != 1);
				uint16_t a = _memory.size();
				_memory.push_back(node->params[0].operator float());
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
			} break;

			case NODE_INPUT_X:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 0;
				break;

			case NODE_INPUT_Y:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 1;
				break;

			case NODE_INPUT_Z:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 2;
				break;

			case NODE_OUTPUT_SDF:
				// TODO Multiple outputs may be supported if we get branching
				CRASH_COND(has_output);
				has_output = true;
				break;

			default: {
				// Add actual operation
				append(program, static_cast<uint8_t>(node->type));

				// Add inputs
				for (size_t j = 0; j < type.inputs.size(); ++j) {
					uint16_t a;

					if (pg_node->inputs[j].connections.size() == 0) {
						// No input, default it
						// TODO Take param value if specified
						a = _memory.size();
						_memory.push_back(0);

					} else {
						ProgramGraph::PortLocation src_port = pg_node->inputs[j].connections[0];
						const uint16_t *aptr = output_port_addresses.getptr(src_port);
						// Previous node ports must have been registered
						CRASH_COND(aptr == nullptr);
						a = *aptr;
					}

					append(program, a);
				}

				// Add outputs
				for (size_t j = 0; j < type.outputs.size(); ++j) {
					const uint16_t a = _memory.size();
					_memory.push_back(0);

					// This will be used by next nodes
					const ProgramGraph::PortLocation op{ node_id, static_cast<uint32_t>(j) };
					output_port_addresses[op] = a;

					append(program, a);
				}

				// Add special params
				switch (node->type) {

					case NODE_CURVE: {
						Ref<Curve> curve = node->params[0];
						CRASH_COND(curve.is_null());
						uint8_t is_monotonic_increasing;
						Interval range = get_curve_range(**curve, is_monotonic_increasing);
						append(program, is_monotonic_increasing);
						append(program, range.min);
						append(program, range.max);
						append(program, *curve);
					} break;

					case NODE_IMAGE_2D: {
						Ref<Image> im = node->params[0];
						CRASH_COND(im.is_null());
						Interval range = get_heightmap_range(**im);
						append(program, range.min);
						append(program, range.max);
						append(program, *im);
					} break;

					case NODE_NOISE_2D:
					case NODE_NOISE_3D: {
						Ref<OpenSimplexNoise> noise = node->params[0];
						CRASH_COND(noise.is_null());
						append(program, *noise);
					} break;

					// TODO Worth it?
					case NODE_CLAMP:
						append(program, node->params[0].operator float());
						append(program, node->params[1].operator float());
						break;

					case NODE_REMAP: {
						float min0 = node->params[0].operator float();
						float max0 = node->params[1].operator float();
						float min1 = node->params[2].operator float();
						float max1 = node->params[3].operator float();
						append(program, -min0);
						append(program, Math::is_equal_approx(max0, min0) ? 99999.f : 1.f / (max0 - min0));
						append(program, min1);
						append(program, max1 - min1);
					} break;

				} // switch special params

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
				// Append a special value after each operation
				append(program, VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif

			} break; // default

		} // switch type
	}

	if (_memory.size() < 4) {
		// In case there is nothing
		_memory.resize(4, 0);
	}

	// Reserve space for range analysis
	_memory.resize(_memory.size() * 2);
	// Make it a copy to keep eventual constants at consistent adresses
	const size_t half_size = _memory.size() / 2;
	for (size_t i = 0, j = half_size; i < half_size; ++i, ++j) {
		_memory[j] = _memory[i];
	}

	print_line(String("Compiled voxel graph. Program size: {0}b, memory size: {1}b")
					   .format(varray(_program.size() * sizeof(float), _memory.size() * sizeof(float))));

	CRASH_COND(!has_output);
}

// The order of fields in the following structs matters.
// Inputs go first, then outputs, then params (if applicable at runtime).
// TODO Think about alignment

struct PNodeBinop {
	uint16_t a_i0;
	uint16_t a_i1;
	uint16_t a_out;
};

struct PNodeMonoFunc {
	uint16_t a_in;
	uint16_t a_out;
};

struct PNodeDistance2D {
	uint16_t a_x0;
	uint16_t a_y0;
	uint16_t a_x1;
	uint16_t a_y1;
	uint16_t a_out;
};

struct PNodeDistance3D {
	uint16_t a_x0;
	uint16_t a_y0;
	uint16_t a_z0;
	uint16_t a_x1;
	uint16_t a_y1;
	uint16_t a_z1;
	uint16_t a_out;
};

struct PNodeClamp {
	uint16_t a_x;
	uint16_t a_out;
	float p_min;
	float p_max;
};

struct PNodeMix {
	uint16_t a_i0;
	uint16_t a_i1;
	uint16_t a_ratio;
	uint16_t a_out;
};

struct PNodeRemap {
	uint16_t a_x;
	uint16_t a_out;
	float p_c0;
	float p_m0;
	float p_c1;
	float p_m1;
};

struct PNodeCurve {
	uint16_t a_in;
	uint16_t a_out;
	uint8_t is_monotonic_increasing;
	float min_value;
	float max_value;
	Curve *p_curve;
};

struct PNodeNoise2D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_out;
	OpenSimplexNoise *p_noise;
};

struct PNodeNoise3D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out;
	OpenSimplexNoise *p_noise;
};

struct PNodeImage2D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_out;
	float min_value;
	float max_value;
	Image *p_image;
};

float VoxelGeneratorGraph::generate_single(const Vector3i &position) {
	// This part must be optimized for speed

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

	ArraySlice<float> memory(_memory, 0, _memory.size() / 2);
	memory[0] = position.x;
	memory[1] = position.y;
	memory[2] = position.z;

	// STL is unreadable on debug builds of Godot, because _DEBUG isn't defined
	//#ifdef DEBUG_ENABLED
	//	const size_t memory_size = memory.size();
	//	const size_t program_size = _program.size();
	//	const float *memory_raw = memory.data();
	//	const uint8_t *program_raw = (const uint8_t *)_program.data();
	//#endif

	uint32_t pc = 0;
	while (pc < _program.size()) {

		const uint8_t opid = _program[pc++];

		switch (opid) {
			case NODE_CONSTANT:
			case NODE_INPUT_X:
			case NODE_INPUT_Y:
			case NODE_INPUT_Z:
			case NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;

			case NODE_ADD: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] + memory[n.a_i1];
			} break;

			case NODE_SUBTRACT: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] - memory[n.a_i1];
			} break;

			case NODE_MULTIPLY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] * memory[n.a_i1];
			} break;

			case NODE_SINE: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::sin(Math_PI * memory[n.a_in]);
			} break;

			case NODE_FLOOR: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::floor(memory[n.a_in]);
			} break;

			case NODE_ABS: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::abs(memory[n.a_in]);
			} break;

			case NODE_SQRT: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::sqrt(memory[n.a_in]);
			} break;

			case NODE_DISTANCE_2D: {
				const PNodeDistance2D &n = read<PNodeDistance2D>(_program, pc);
				memory[n.a_out] = Math::sqrt(squared(memory[n.a_x1] - memory[n.a_x0]) +
											 squared(memory[n.a_y1] - memory[n.a_y0]));
			} break;

			case NODE_DISTANCE_3D: {
				const PNodeDistance3D &n = read<PNodeDistance3D>(_program, pc);
				memory[n.a_out] = Math::sqrt(squared(memory[n.a_x1] - memory[n.a_x0]) +
											 squared(memory[n.a_y1] - memory[n.a_y0]) +
											 squared(memory[n.a_z1] - memory[n.a_z0]));
			} break;

			case NODE_MIX: {
				const PNodeMix &n = read<PNodeMix>(_program, pc);
				memory[n.a_out] = Math::lerp(memory[n.a_i0], memory[n.a_i1], memory[n.a_ratio]);
			} break;

			case NODE_CLAMP: {
				const PNodeClamp &n = read<PNodeClamp>(_program, pc);
				memory[n.a_out] = clamp(memory[n.a_x], memory[n.p_min], memory[n.p_max]);
			} break;

			case NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				memory[n.a_out] = ((memory[n.a_x] - n.p_c0) * n.p_m0) * n.p_m1 + n.p_c1;
			} break;

			case NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				memory[n.a_out] = n.p_curve->interpolate_baked(memory[n.a_in]);
			} break;

			case NODE_NOISE_2D: {
				const PNodeNoise2D &n = read<PNodeNoise2D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_2d(memory[n.a_x], memory[n.a_y]);
			} break;

			case NODE_NOISE_3D: {
				const PNodeNoise3D &n = read<PNodeNoise3D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_3d(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
			} break;

			case NODE_IMAGE_2D: {
				const PNodeImage2D &n = read<PNodeImage2D>(_program, pc);
				// TODO Not great, but in Godot 4.0 we won't need to lock anymore. Otherwise, need to do it in a pre-run and post-run
				n.p_image->lock();
				memory[n.a_out] = get_pixel_repeat(*n.p_image, memory[n.a_x], memory[n.a_y]);
				n.p_image->unlock();
			} break;

			default:
				CRASH_NOW();
				break;
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	return memory[memory.size() - 1] * _iso_scale;
}

Interval VoxelGeneratorGraph::analyze_range(Vector3i min_pos, Vector3i max_pos) {

	ArraySlice<float> min_memory(_memory, 0, _memory.size() / 2);
	ArraySlice<float> max_memory(_memory, _memory.size() / 2, _memory.size());
	min_memory[0] = min_pos.x;
	min_memory[1] = min_pos.y;
	min_memory[2] = min_pos.z;
	max_memory[0] = max_pos.x;
	max_memory[1] = max_pos.y;
	max_memory[2] = max_pos.z;

	uint32_t pc = 0;
	while (pc < _program.size()) {

		const uint8_t opid = _program[pc++];

		switch (opid) {
			case NODE_CONSTANT:
			case NODE_INPUT_X:
			case NODE_INPUT_Y:
			case NODE_INPUT_Z:
			case NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;

			case NODE_ADD: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				min_memory[n.a_out] = min_memory[n.a_i0] + min_memory[n.a_i1];
				max_memory[n.a_out] = max_memory[n.a_i0] + max_memory[n.a_i1];
			} break;

			case NODE_SUBTRACT: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				min_memory[n.a_out] = min_memory[n.a_i0] - max_memory[n.a_i1];
				max_memory[n.a_out] = max_memory[n.a_i0] - min_memory[n.a_i1];
			} break;

			case NODE_MULTIPLY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				Interval r = Interval(min_memory[n.a_i0], max_memory[n.a_i0]) *
							 Interval(min_memory[n.a_i1], max_memory[n.a_i1]);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_SINE: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				if (min_memory[n.a_in] == max_memory[n.a_in]) {
					float s = Math::sin(min_memory[n.a_in]);
					min_memory[n.a_out] = s;
					max_memory[n.a_out] = s;
				} else {
					// Simplified
					min_memory[n.a_out] = -1.f;
					max_memory[n.a_out] = 1.f;
				}
			} break;

			case NODE_FLOOR: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				min_memory[n.a_out] = Math::floor(min_memory[n.a_in]);
				max_memory[n.a_out] = Math::floor(max_memory[n.a_in]); // ceil?
			} break;

			case NODE_ABS: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				Interval r = Interval(min_memory[n.a_in], max_memory[n.a_in]).abs();
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_SQRT: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				Interval r = Interval(min_memory[n.a_in], max_memory[n.a_in]).sqrt();
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_DISTANCE_2D: {
				const PNodeDistance2D &n = read<PNodeDistance2D>(_program, pc);
				Interval x0(min_memory[n.a_x0], max_memory[n.a_x0]);
				Interval y0(min_memory[n.a_y0], max_memory[n.a_y0]);
				Interval x1(min_memory[n.a_x1], max_memory[n.a_x1]);
				Interval y1(min_memory[n.a_y1], max_memory[n.a_y1]);
				Interval dx = x1 - x0;
				Interval dy = y1 - y0;
				Interval r = (dx * dx + dy * dy).sqrt();
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_DISTANCE_3D: {
				const PNodeDistance3D &n = read<PNodeDistance3D>(_program, pc);
				Interval x0(min_memory[n.a_x0], max_memory[n.a_x0]);
				Interval y0(min_memory[n.a_y0], max_memory[n.a_y0]);
				Interval z0(min_memory[n.a_z0], max_memory[n.a_z0]);
				Interval x1(min_memory[n.a_x1], max_memory[n.a_x1]);
				Interval y1(min_memory[n.a_y1], max_memory[n.a_y1]);
				Interval z1(min_memory[n.a_z1], max_memory[n.a_z1]);
				Interval dx = x1 - x0;
				Interval dy = y1 - y0;
				Interval dz = z1 - z0;
				Interval r = (dx * dx + dy * dy + dz * dz).sqrt();
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_MIX: {
				const PNodeMix &n = read<PNodeMix>(_program, pc);
				Interval a(min_memory[n.a_i0], max_memory[n.a_i0]);
				Interval b(min_memory[n.a_i1], max_memory[n.a_i1]);
				Interval t(min_memory[n.a_ratio], max_memory[n.a_ratio]);
				if (t.min == t.max) {
					min_memory[n.a_out] = Math::lerp(a.min, b.min, t.min);
					max_memory[n.a_out] = Math::lerp(a.max, b.max, t.min);
				} else {
					Interval r = a + t * (b - a);
					min_memory[n.a_out] = r.min;
					max_memory[n.a_out] = r.max;
				}
			} break;

			case NODE_CLAMP: {
				const PNodeClamp &n = read<PNodeClamp>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				// TODO We may want to have wirable min and max later
				Interval cmin = Interval::from_single_value(n.p_min);
				Interval cmax = Interval::from_single_value(n.p_max);
				Interval r = x.clamp(cmin, cmax);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				Interval r = ((x - n.p_c0) * n.p_m0) * n.p_m1 + n.p_c1;
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				if (min_memory[n.a_in] == max_memory[n.a_in]) {
					float v = n.p_curve->interpolate_baked(min_memory[n.a_in]);
					min_memory[n.a_out] = v;
					max_memory[n.a_out] = v;
				} else if (n.is_monotonic_increasing) {
					min_memory[n.a_out] = n.p_curve->interpolate_baked(min_memory[n.a_in]);
					max_memory[n.a_out] = n.p_curve->interpolate_baked(max_memory[n.a_in]);
				} else {
					// TODO Segment the curve?
					min_memory[n.a_out] = n.min_value;
					max_memory[n.a_out] = n.max_value;
				}
			} break;

			case NODE_NOISE_2D: {
				const PNodeNoise2D &n = read<PNodeNoise2D>(_program, pc);
				if (
						min_memory[n.a_x] == max_memory[n.a_x] &&
						min_memory[n.a_y] == max_memory[n.a_y]) {

					float h = n.p_noise->get_noise_2d(
							min_memory[n.a_x],
							min_memory[n.a_y]);

					min_memory[n.a_out] = h;
					max_memory[n.a_out] = h;

				} else {
					min_memory[n.a_out] = -1.f;
					max_memory[n.a_out] = 1.f;
				}
			} break;

			case NODE_NOISE_3D: {
				const PNodeNoise3D &n = read<PNodeNoise3D>(_program, pc);
				if (
						min_memory[n.a_x] == max_memory[n.a_x] &&
						min_memory[n.a_y] == max_memory[n.a_y] &&
						min_memory[n.a_z] == max_memory[n.a_z]) {

					float h = n.p_noise->get_noise_3d(
							min_memory[n.a_x],
							min_memory[n.a_y],
							min_memory[n.a_z]);

					min_memory[n.a_out] = h;
					max_memory[n.a_out] = h;

				} else {
					min_memory[n.a_out] = -1.f;
					max_memory[n.a_out] = 1.f;
				}
			} break;

			case NODE_IMAGE_2D: {
				const PNodeImage2D &n = read<PNodeImage2D>(_program, pc);
				// TODO Segment image?
				min_memory[n.a_out] = n.min_value;
				max_memory[n.a_out] = n.max_value;
			} break;

			default:
				CRASH_NOW();
				break;
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	return Interval(min_memory[min_memory.size() - 1], max_memory[max_memory.size() - 1]) * _iso_scale;
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

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel() {
	Vector3i pos(1, 1, 1);
	float v;
	const uint32_t iterations = 1000000;
	ProfilingClock profiling_clock;
	profiling_clock.restart();
	for (uint32_t i = 0; i < iterations; ++i) {
		v = generate_single(pos);
	}
	uint64_t ius = profiling_clock.restart();
	float us = static_cast<double>(ius) / iterations;
	//	print_line(String("Time: {0}us").format(varray(us)));
	//	print_line(String("Value: {0}").format(varray(v)));
	return us;
}

bool VoxelGeneratorGraph::_set(const StringName &p_name, const Variant &p_value) {
	const String name = p_name;

	struct L {
		inline static bool set_xyz(char c, Vector3i &p, int v) {
			int i = c - 'x';
			ERR_FAIL_COND_V(i < 0 || i >= Vector3i::AXIS_COUNT, false);
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
			ERR_FAIL_COND_V(i < 0 || i >= Vector3i::AXIS_COUNT, false);
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

void VoxelGeneratorGraph::_bind_methods() {

	ClassDB::bind_method(D_METHOD("clear"), &VoxelGeneratorGraph::clear);
	ClassDB::bind_method(D_METHOD("compile"), &VoxelGeneratorGraph::compile);
	ClassDB::bind_method(D_METHOD("load_waves_preset"), &VoxelGeneratorGraph::load_waves_preset);
	ClassDB::bind_method(D_METHOD("debug_measure_microseconds_per_voxel"), &VoxelGeneratorGraph::debug_measure_microseconds_per_voxel);
}
