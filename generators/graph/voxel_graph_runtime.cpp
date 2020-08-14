#include "voxel_graph_runtime.h"
#include "../../util/macros.h"
#include "range_utility.h"
#include "voxel_generator_graph.h"
#include "voxel_graph_node_db.h"

#include <unordered_set>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

//template <typename T>
//inline void write_static(std::vector<uint8_t> &mem, uint32_t p, const T &v) {
//#ifdef DEBUG_ENABLED
//	CRASH_COND(p + sizeof(T) >= mem.size());
//#endif
//	*(T *)(&mem[p]) = v;
//}

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

template <typename T>
T &get_or_create(std::vector<uint8_t> &program, uint32_t offset) {
	CRASH_COND(offset >= program.size());
	const size_t required_size = offset + sizeof(T);
	if (required_size >= program.size()) {
		program.resize(required_size);
	}
	return *(T *)&program[offset];
}

inline float get_pixel_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

// Runtime data structs:
// The order of fields in the following structs matters.
// They map the layout produced by the compilation.
// Inputs go first, then outputs, then params (if applicable at runtime).

struct PNodeBinop {
	uint16_t a_i0;
	uint16_t a_i1;
	uint16_t a_out;
};

struct PNodeMonop {
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

struct PNodeSmoothstep {
	uint16_t a_x;
	uint16_t a_out;
	float p_edge0;
	float p_edge1;
};

struct PNodeCurve {
	uint16_t a_in;
	uint16_t a_out;
	uint8_t is_monotonic_increasing;
	float min_value;
	float max_value;
	Curve *p_curve;
};

struct PNodeSelect {
	uint16_t a_i0;
	uint16_t a_i1;
	uint16_t a_threshold;
	uint16_t a_t;
	uint16_t a_out;
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

struct PNodeSdfBox {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_sx;
	uint16_t a_sy;
	uint16_t a_sz;
	uint16_t a_out;
};

struct PNodeSdfSphere {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_r;
	uint16_t a_out;
};

struct PNodeSdfTorus {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_r0;
	uint16_t a_r1;
	uint16_t a_out;
};

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
	_memory.resize(8, 0);
	_xzy_program_start = 0;
	_last_x = std::numeric_limits<int>::max();
	_last_z = std::numeric_limits<int>::max();
	_output_port_addresses.clear();
	_sdf_output_address = -1;
}

void VoxelGraphRuntime::compile(const ProgramGraph &graph, bool debug) {
	_output_port_addresses.clear();

	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;

	graph.find_terminal_nodes(terminal_nodes);

	if (!debug) {
		// Exclude debug nodes
		unordered_remove_if(terminal_nodes, [&graph](uint32_t node_id) {
			const ProgramGraph::Node *node = graph.get_node(node_id);
			const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node->type_id);
			return type.debug_only;
		});
	}

	graph.find_dependencies(terminal_nodes, order);

	uint32_t xzy_start_index = 0;

	// Optimize parts of the graph that only depend on X and Z,
	// so they can be moved in the outer loop when blocks are generated, running less times.
	// Moves them all at the beginning.
	{
		std::vector<uint32_t> immediate_deps;
		std::unordered_set<uint32_t> nodes_depending_on_y;
		std::vector<uint32_t> order_xz;
		std::vector<uint32_t> order_xzy;

		for (size_t i = 0; i < order.size(); ++i) {
			const uint32_t node_id = order[i];
			const ProgramGraph::Node *node = graph.get_node(node_id);

			bool depends_on_y = false;

			if (node->type_id == VoxelGeneratorGraph::NODE_INPUT_Y) {
				nodes_depending_on_y.insert(node_id);
				depends_on_y = true;
			}

			if (!depends_on_y) {
				immediate_deps.clear();
				graph.find_immediate_dependencies(node_id, immediate_deps);

				for (size_t j = 0; j < immediate_deps.size(); ++j) {
					const uint32_t dep_node_id = immediate_deps[j];

					if (nodes_depending_on_y.find(dep_node_id) != nodes_depending_on_y.end()) {
						depends_on_y = true;
						nodes_depending_on_y.insert(node_id);
						break;
					}
				}
			}

			if (depends_on_y) {
				order_xzy.push_back(node_id);
			} else {
				order_xz.push_back(node_id);
			}
		}

		xzy_start_index = order_xz.size();

		//#ifdef DEBUG_ENABLED
		//		const uint32_t order_xz_raw_size = order_xz.size();
		//		const uint32_t *order_xz_raw = order_xz.data();
		//		const uint32_t order_xzy_raw_size = order_xzy.size();
		//		const uint32_t *order_xzy_raw = order_xzy.data();
		//#endif

		size_t i = 0;
		for (size_t j = 0; j < order_xz.size(); ++j) {
			order[i++] = order_xz[j];
		}
		for (size_t j = 0; j < order_xzy.size(); ++j) {
			order[i++] = order_xzy[j];
		}
	}

	//#ifdef DEBUG_ENABLED
	//	const uint32_t order_raw_size = order.size();
	//	const uint32_t *order_raw = order.data();
	//#endif

	_program.clear();

	_xzy_program_start = 0;
	_last_x = std::numeric_limits<int>::max();
	_last_z = std::numeric_limits<int>::max();
	_sdf_output_address = -1;

	// Main inputs X, Y, Z
	_memory.resize(3);

	std::vector<uint8_t> &program = _program;
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();

	// Run through each node in order, and turn them into program instructions
	for (size_t i = 0; i < order.size(); ++i) {
		const uint32_t node_id = order[i];
		const ProgramGraph::Node *node = graph.get_node(node_id);
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node->type_id);

		CRASH_COND(node == nullptr);
		CRASH_COND(node->inputs.size() != type.inputs.size());
		CRASH_COND(node->outputs.size() != type.outputs.size());

		if (i == xzy_start_index) {
			_xzy_program_start = _program.size();
		}

		switch (node->type_id) {
			case VoxelGeneratorGraph::NODE_CONSTANT: {
				CRASH_COND(type.outputs.size() != 1);
				CRASH_COND(type.params.size() != 1);
				const uint16_t a = _memory.size();
				_memory.push_back(node->params[0].operator float());
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
			} break;

			case VoxelGeneratorGraph::NODE_INPUT_X:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 0;
				break;

			case VoxelGeneratorGraph::NODE_INPUT_Y:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 1;
				break;

			case VoxelGeneratorGraph::NODE_INPUT_Z:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 2;
				break;

			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// TODO Multiple outputs may be supported if we get branching
				if (_sdf_output_address != -1) {
					ERR_PRINT("Voxel graph has multiple SDF outputs");
				}
				if (_memory.size() > 0) {
					_sdf_output_address = _memory.size() - 1;
				}
				break;

			case VoxelGeneratorGraph::NODE_SDF_PREVIEW:
				break;

			default: {
				// Add actual operation
				CRASH_COND(node->type_id > 0xff);
				append(program, static_cast<uint8_t>(node->type_id));
				const size_t offset = program.size();

				// Inputs and outputs use a convention so we can have generic code for them.
				// Parameters are more specific, and may be affected by alignment so better just do them by hand

				// Add inputs
				for (size_t j = 0; j < type.inputs.size(); ++j) {
					uint16_t a;

					if (node->inputs[j].connections.size() == 0) {
						// No input, default it
						CRASH_COND(j >= node->default_inputs.size());
						float defval = node->default_inputs[j];
						a = _memory.size();
						_memory.push_back(defval);

					} else {
						ProgramGraph::PortLocation src_port = node->inputs[j].connections[0];
						const uint16_t *aptr = _output_port_addresses.getptr(src_port);
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
					_output_port_addresses[op] = a;

					append(program, a);
				}

				// Add params (only nodes having some)
				switch (node->type_id) {
					case VoxelGeneratorGraph::NODE_CLAMP: {
						// TODO Worth it?
						PNodeClamp &n = get_or_create<PNodeClamp>(program, offset);
						n.p_min = node->params[0].operator float();
						n.p_max = node->params[1].operator float();
					} break;

					case VoxelGeneratorGraph::NODE_REMAP: {
						PNodeRemap &n = get_or_create<PNodeRemap>(program, offset);
						const float min0 = node->params[0].operator float();
						const float max0 = node->params[1].operator float();
						const float min1 = node->params[2].operator float();
						const float max1 = node->params[3].operator float();
						n.p_c0 = -min0;
						n.p_m0 = Math::is_equal_approx(max0, min0) ? 99999.f : 1.f / (max0 - min0);
						n.p_c1 = min1;
						n.p_m1 = max1 - min1;
					} break;

					case VoxelGeneratorGraph::NODE_SMOOTHSTEP: {
						PNodeSmoothstep &n = get_or_create<PNodeSmoothstep>(program, offset);
						n.p_edge0 = node->params[0].operator float();
						n.p_edge1 = node->params[1].operator float();
					} break;

					case VoxelGeneratorGraph::NODE_CURVE: {
						PNodeCurve &n = get_or_create<PNodeCurve>(program, offset);
						Ref<Curve> curve = node->params[0];
						CRASH_COND(curve.is_null());
						uint8_t is_monotonic_increasing;
						const Interval range = get_curve_range(**curve, is_monotonic_increasing);
						n.is_monotonic_increasing = is_monotonic_increasing;
						n.min_value = range.min;
						n.max_value = range.max;
						n.p_curve = *curve;
					} break;

					case VoxelGeneratorGraph::NODE_NOISE_2D: {
						PNodeNoise2D &n = get_or_create<PNodeNoise2D>(program, offset);
						Ref<OpenSimplexNoise> noise = node->params[0];
						CRASH_COND(noise.is_null());
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_NOISE_3D: {
						PNodeNoise3D &n = get_or_create<PNodeNoise3D>(program, offset);
						Ref<OpenSimplexNoise> noise = node->params[0];
						CRASH_COND(noise.is_null());
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_IMAGE_2D: {
						PNodeImage2D &n = get_or_create<PNodeImage2D>(program, offset);
						Ref<Image> im = node->params[0];
						CRASH_COND(im.is_null());
						const Interval range = get_heightmap_range(**im);
						n.min_value = range.min;
						n.max_value = range.max;
						n.p_image = *im;
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

	PRINT_VERBOSE(String("Compiled voxel graph. Program size: {0}b, memory size: {1}b")
						  .format(varray(_program.size() * sizeof(float), _memory.size() * sizeof(float))));

	//ERR_FAIL_COND(_sdf_output_address == -1);
}

inline Interval get_length(const Interval &x, const Interval &y) {
	return sqrt(x * x + y * y);
}

inline Interval get_length(const Interval &x, const Interval &y, const Interval &z) {
	return sqrt(x * x + y * y + z * z);
}

// For more, see https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
// TODO Move these to VoxelMath once we have a proper namespace, so they can be used in VoxelTool too

inline float sdf_box(const Vector3 pos, const Vector3 extents) {
	Vector3 d = pos.abs() - extents;
	return min(max(d.x, max(d.y, d.z)), 0.f) +
		   Vector3(max(d.x, 0.f), max(d.y, 0.f), max(d.z, 0.f)).length();
}

inline Interval sdf_box(
		const Interval &x, const Interval &y, const Interval &z,
		const Interval &sx, const Interval &sy, const Interval &sz) {
	Interval dx = abs(x) - sx;
	Interval dy = abs(y) - sy;
	Interval dz = abs(z) - sz;
	return min_interval(max_interval(dx, max_interval(dy, dz)), 0.f) +
		   get_length(max_interval(dx, 0.f), max_interval(dy, 0.f), max_interval(dz, 0.f));
}

inline float sdf_torus(const Vector3 pos, float r0, float r1) {
	Vector2 q = Vector2(Vector2(pos.x, pos.z).length() - r0, pos.y);
	return q.length() - r1;
}

inline Interval sdf_torus(const Interval &x, const Interval &y, const Interval &z, const Interval r0, const Interval r1) {
	Interval qx = get_length(x, z) - r0;
	return get_length(qx, y) - r1;
}

inline float select(float a, float b, float threshold, float t) {
	return t < threshold ? a : b;
}

inline Interval select(const Interval &a, const Interval &b, const Interval &threshold, const Interval &t) {
	if (t.max < threshold.min) {
		return a;
	}
	if (t.min >= threshold.max) {
		return b;
	}
	return Interval(min(a.min, b.min), max(a.max, b.max));
}

float VoxelGraphRuntime::generate_single(const Vector3i &position) {
	// This part must be optimized for speed

#ifdef DEBUG_ENABLED
	CRASH_COND(_memory.size() == 0);
#endif
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(_sdf_output_address == -1, 0.0, "The graph has no SDF output");
#endif

	ArraySlice<float> memory(_memory, 0, _memory.size() / 2);
	memory[0] = position.x;
	memory[1] = position.y;
	memory[2] = position.z;

	uint32_t pc;
	if (position.x == _last_x && position.z == _last_z) {
		pc = _xzy_program_start;
	} else {
		pc = 0;
	}

	// STL is unreadable on debug builds of Godot, because _DEBUG isn't defined
	//#ifdef DEBUG_ENABLED
	//	const size_t memory_size = memory.size();
	//	const size_t program_size = _program.size();
	//	const float *memory_raw = memory.data();
	//	const uint8_t *program_raw = (const uint8_t *)_program.data();
	//#endif

	while (pc < _program.size()) {
		const uint8_t opid = _program[pc++];

		switch (opid) {
			case VoxelGeneratorGraph::NODE_CONSTANT:
			case VoxelGeneratorGraph::NODE_INPUT_X:
			case VoxelGeneratorGraph::NODE_INPUT_Y:
			case VoxelGeneratorGraph::NODE_INPUT_Z:
			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;

			case VoxelGeneratorGraph::NODE_ADD: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] + memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_SUBTRACT: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] - memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_MULTIPLY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] * memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_DIVIDE: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				float d = memory[n.a_i1];
				memory[n.a_out] = d == 0.f ? 0.f : memory[n.a_i0] / d;
			} break;

			case VoxelGeneratorGraph::NODE_SIN: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				memory[n.a_out] = Math::sin(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_FLOOR: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				memory[n.a_out] = Math::floor(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_ABS: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				memory[n.a_out] = Math::abs(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_SQRT: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				memory[n.a_out] = Math::sqrt(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_FRACT: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				const float x = memory[n.a_in];
				memory[n.a_out] = x - Math::floor(x);
			} break;

			case VoxelGeneratorGraph::NODE_STEPIFY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = Math::stepify(memory[n.a_i0], memory[n.a_i1]);
			} break;

			case VoxelGeneratorGraph::NODE_WRAP: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = wrapf(memory[n.a_i0], memory[n.a_i1]);
			} break;

			case VoxelGeneratorGraph::NODE_MIN: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = ::min(memory[n.a_i0], memory[n.a_i1]);
			} break;

			case VoxelGeneratorGraph::NODE_MAX: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = ::max(memory[n.a_i0], memory[n.a_i1]);
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_2D: {
				const PNodeDistance2D &n = read<PNodeDistance2D>(_program, pc);
				memory[n.a_out] = Math::sqrt(squared(memory[n.a_x1] - memory[n.a_x0]) +
											 squared(memory[n.a_y1] - memory[n.a_y0]));
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_3D: {
				const PNodeDistance3D &n = read<PNodeDistance3D>(_program, pc);
				memory[n.a_out] = Math::sqrt(squared(memory[n.a_x1] - memory[n.a_x0]) +
											 squared(memory[n.a_y1] - memory[n.a_y0]) +
											 squared(memory[n.a_z1] - memory[n.a_z0]));
			} break;

			case VoxelGeneratorGraph::NODE_MIX: {
				const PNodeMix &n = read<PNodeMix>(_program, pc);
				memory[n.a_out] = Math::lerp(memory[n.a_i0], memory[n.a_i1], memory[n.a_ratio]);
			} break;

			case VoxelGeneratorGraph::NODE_CLAMP: {
				const PNodeClamp &n = read<PNodeClamp>(_program, pc);
				memory[n.a_out] = clamp(memory[n.a_x], memory[n.p_min], memory[n.p_max]);
			} break;

			case VoxelGeneratorGraph::NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				memory[n.a_out] = ((memory[n.a_x] - n.p_c0) * n.p_m0) * n.p_m1 + n.p_c1;
			} break;

			case VoxelGeneratorGraph::NODE_SMOOTHSTEP: {
				const PNodeSmoothstep &n = read<PNodeSmoothstep>(_program, pc);
				memory[n.a_out] = smoothstep(n.p_edge0, n.p_edge1, memory[n.a_x]);
			} break;

			case VoxelGeneratorGraph::NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				memory[n.a_out] = n.p_curve->interpolate_baked(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_SELECT: {
				const PNodeSelect &n = read<PNodeSelect>(_program, pc);
				memory[n.a_out] = select(memory[n.a_i0], memory[n.a_i1], memory[n.a_threshold], memory[n.a_t]);
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_2D: {
				const PNodeNoise2D &n = read<PNodeNoise2D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_2d(memory[n.a_x], memory[n.a_y]);
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_3D: {
				const PNodeNoise3D &n = read<PNodeNoise3D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_3d(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
			} break;

			case VoxelGeneratorGraph::NODE_IMAGE_2D: {
				const PNodeImage2D &n = read<PNodeImage2D>(_program, pc);
				// TODO Not great, but in Godot 4.0 we won't need to lock anymore. Otherwise, need to do it in a pre-run and post-run
				n.p_image->lock();
				memory[n.a_out] = get_pixel_repeat(*n.p_image, memory[n.a_x], memory[n.a_y]);
				n.p_image->unlock();
			} break;

			// TODO Alias to Subtract?
			case VoxelGeneratorGraph::NODE_SDF_PLANE: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				memory[n.a_out] = memory[n.a_i0] - memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_SDF_BOX: {
				const PNodeSdfBox &n = read<PNodeSdfBox>(_program, pc);
				// TODO Could read raw?
				const Vector3 pos(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
				const Vector3 extents(memory[n.a_sx], memory[n.a_sy], memory[n.a_sz]);
				memory[n.a_out] = sdf_box(pos, extents);
			} break;

			case VoxelGeneratorGraph::NODE_SDF_SPHERE: {
				const PNodeSdfSphere &n = read<PNodeSdfSphere>(_program, pc);
				// TODO Could read raw?
				const Vector3 pos(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
				memory[n.a_out] = pos.length() - memory[n.a_r];
			} break;

			case VoxelGeneratorGraph::NODE_SDF_TORUS: {
				const PNodeSdfTorus &n = read<PNodeSdfTorus>(_program, pc);
				// TODO Could read raw?
				const Vector3 pos(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
				memory[n.a_out] = sdf_torus(pos, memory[n.a_r0], memory[n.a_r1]);
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

	return memory[_sdf_output_address];
}

Interval VoxelGraphRuntime::analyze_range(Vector3i min_pos, Vector3i max_pos) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(_sdf_output_address == -1, Interval(), "The graph has no SDF output");
#endif

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
			case VoxelGeneratorGraph::NODE_CONSTANT:
			case VoxelGeneratorGraph::NODE_INPUT_X:
			case VoxelGeneratorGraph::NODE_INPUT_Y:
			case VoxelGeneratorGraph::NODE_INPUT_Z:
			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;

			case VoxelGeneratorGraph::NODE_ADD: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				min_memory[n.a_out] = min_memory[n.a_i0] + min_memory[n.a_i1];
				max_memory[n.a_out] = max_memory[n.a_i0] + max_memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_SUBTRACT: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				min_memory[n.a_out] = min_memory[n.a_i0] - max_memory[n.a_i1];
				max_memory[n.a_out] = max_memory[n.a_i0] - min_memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_MULTIPLY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				Interval r = Interval(min_memory[n.a_i0], max_memory[n.a_i0]) *
							 Interval(min_memory[n.a_i1], max_memory[n.a_i1]);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_DIVIDE: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				Interval r = Interval(min_memory[n.a_i0], max_memory[n.a_i0]) /
							 Interval(min_memory[n.a_i1], max_memory[n.a_i1]);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SIN: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				Interval r = sin(Interval(min_memory[n.a_in], max_memory[n.a_in]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_FLOOR: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				Interval r = floor(Interval(min_memory[n.a_in], max_memory[n.a_in]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_ABS: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				Interval r = abs(Interval(min_memory[n.a_in], max_memory[n.a_in]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SQRT: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				Interval r = sqrt(Interval(min_memory[n.a_in], max_memory[n.a_in]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_FRACT: {
				const PNodeMonop &n = read<PNodeMonop>(_program, pc);
				Interval r = Interval(min_memory[n.a_in], max_memory[n.a_in]);
				r = r - floor(r);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_STEPIFY: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				const Interval r = stepify(
						Interval(min_memory[n.a_i0], max_memory[n.a_i0]),
						Interval(min_memory[n.a_i1], max_memory[n.a_i1]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_WRAP: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				const Interval r = wrapf(
						Interval(min_memory[n.a_i0], max_memory[n.a_i0]),
						Interval(min_memory[n.a_i1], max_memory[n.a_i1]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_MIN: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				const Interval r = min_interval(
						Interval(min_memory[n.a_i0], max_memory[n.a_i0]),
						Interval(min_memory[n.a_i1], max_memory[n.a_i1]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_MAX: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				const Interval r = max_interval(
						Interval(min_memory[n.a_i0], max_memory[n.a_i0]),
						Interval(min_memory[n.a_i1], max_memory[n.a_i1]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_2D: {
				const PNodeDistance2D &n = read<PNodeDistance2D>(_program, pc);
				Interval x0(min_memory[n.a_x0], max_memory[n.a_x0]);
				Interval y0(min_memory[n.a_y0], max_memory[n.a_y0]);
				Interval x1(min_memory[n.a_x1], max_memory[n.a_x1]);
				Interval y1(min_memory[n.a_y1], max_memory[n.a_y1]);
				Interval dx = x1 - x0;
				Interval dy = y1 - y0;
				Interval r = sqrt(dx * dx + dy * dy);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_3D: {
				const PNodeDistance3D &n = read<PNodeDistance3D>(_program, pc);
				Interval x0(min_memory[n.a_x0], max_memory[n.a_x0]);
				Interval y0(min_memory[n.a_y0], max_memory[n.a_y0]);
				Interval z0(min_memory[n.a_z0], max_memory[n.a_z0]);
				Interval x1(min_memory[n.a_x1], max_memory[n.a_x1]);
				Interval y1(min_memory[n.a_y1], max_memory[n.a_y1]);
				Interval z1(min_memory[n.a_z1], max_memory[n.a_z1]);
				Interval r = get_length(x1 - x0, y1 - y0, z1 - z0);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_MIX: {
				const PNodeMix &n = read<PNodeMix>(_program, pc);
				Interval a(min_memory[n.a_i0], max_memory[n.a_i0]);
				Interval b(min_memory[n.a_i1], max_memory[n.a_i1]);
				Interval t(min_memory[n.a_ratio], max_memory[n.a_ratio]);
				Interval r = lerp(a, b, t);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_CLAMP: {
				const PNodeClamp &n = read<PNodeClamp>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				// TODO We may want to have wirable min and max later
				Interval cmin = Interval::from_single_value(n.p_min);
				Interval cmax = Interval::from_single_value(n.p_max);
				Interval r = clamp(x, cmin, cmax);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				Interval r = ((x - n.p_c0) * n.p_m0) * n.p_m1 + n.p_c1;
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SMOOTHSTEP: {
				const PNodeSmoothstep &n = read<PNodeSmoothstep>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				Interval r = smoothstep(n.p_edge0, n.p_edge1, x);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				if (min_memory[n.a_in] == max_memory[n.a_in]) {
					const float v = n.p_curve->interpolate_baked(min_memory[n.a_in]);
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

			case VoxelGeneratorGraph::NODE_SELECT: {
				const PNodeSelect &n = read<PNodeSelect>(_program, pc);
				const Interval a(min_memory[n.a_i0], max_memory[n.a_i0]);
				const Interval b(min_memory[n.a_i1], max_memory[n.a_i1]);
				const Interval threshold(min_memory[n.a_threshold], max_memory[n.a_threshold]);
				const Interval t(min_memory[n.a_t], max_memory[n.a_t]);
				const Interval r = select(a, b, threshold, t);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_2D: {
				const PNodeNoise2D &n = read<PNodeNoise2D>(_program, pc);
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				Interval r = get_osn_range_2d(n.p_noise, x, y);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_3D: {
				const PNodeNoise3D &n = read<PNodeNoise3D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval r = get_osn_range_3d(n.p_noise, x, y, z);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_IMAGE_2D: {
				const PNodeImage2D &n = read<PNodeImage2D>(_program, pc);
				// TODO Segment image?
				min_memory[n.a_out] = n.min_value;
				max_memory[n.a_out] = n.max_value;
			} break;

			case VoxelGeneratorGraph::NODE_SDF_PLANE: {
				const PNodeBinop &n = read<PNodeBinop>(_program, pc);
				min_memory[n.a_out] = min_memory[n.a_i0] - max_memory[n.a_i1];
				max_memory[n.a_out] = max_memory[n.a_i0] - min_memory[n.a_i1];
			} break;

			case VoxelGeneratorGraph::NODE_SDF_BOX: {
				const PNodeSdfBox &n = read<PNodeSdfBox>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval sx(min_memory[n.a_sx], max_memory[n.a_sx]);
				const Interval sy(min_memory[n.a_sy], max_memory[n.a_sy]);
				const Interval sz(min_memory[n.a_sz], max_memory[n.a_sz]);
				const Interval r = sdf_box(x, y, z, sx, sy, sz);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SDF_SPHERE: {
				const PNodeSdfSphere &n = read<PNodeSdfSphere>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval radius(min_memory[n.a_r], max_memory[n.a_r]);
				const Interval r = get_length(x, y, z) - radius;
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SDF_TORUS: {
				const PNodeSdfTorus &n = read<PNodeSdfTorus>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval radius1(min_memory[n.a_r0], max_memory[n.a_r0]);
				const Interval radius2(min_memory[n.a_r1], max_memory[n.a_r1]);
				const Interval r = sdf_torus(x, y, z, radius1, radius2);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
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

	return Interval(min_memory[_sdf_output_address], max_memory[_sdf_output_address]);
}

uint16_t VoxelGraphRuntime::get_output_port_address(ProgramGraph::PortLocation port) const {
	const uint16_t *aptr = _output_port_addresses.getptr(port);
	ERR_FAIL_COND_V(aptr == nullptr, 0);
	return *aptr;
}

float VoxelGraphRuntime::get_memory_value(uint16_t address) const {
	CRASH_COND(address >= _memory.size());
	return _memory[address];
}
