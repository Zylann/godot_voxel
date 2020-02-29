#include "voxel_graph_runtime.h"
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

inline float get_pixel_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
	_memory.resize(8, 0);
	_xzy_program_start = 0;
	_last_x = INT_MAX;
	_last_z = INT_MAX;
}

void VoxelGraphRuntime::compile(const ProgramGraph &graph) {
	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;

	graph.find_terminal_nodes(terminal_nodes);
	// For now only 1 end is supported
	ERR_FAIL_COND(terminal_nodes.size() != 1);

	graph.find_dependencies(terminal_nodes.back(), order);

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
	_last_x = INT_MAX;
	_last_z = INT_MAX;

	// Main inputs X, Y, Z
	_memory.resize(3);

	std::vector<uint8_t> &program = _program;
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();
	HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> output_port_addresses;
	bool has_output = false;

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
				uint16_t a = _memory.size();
				_memory.push_back(node->params[0].operator float());
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
			} break;

			case VoxelGeneratorGraph::NODE_INPUT_X:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 0;
				break;

			case VoxelGeneratorGraph::NODE_INPUT_Y:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 1;
				break;

			case VoxelGeneratorGraph::NODE_INPUT_Z:
				output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = 2;
				break;

			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// TODO Multiple outputs may be supported if we get branching
				CRASH_COND(has_output);
				has_output = true;
				break;

			default: {
				// Add actual operation
				CRASH_COND(node->type_id > 0xff);
				append(program, static_cast<uint8_t>(node->type_id));

				// Add inputs
				for (size_t j = 0; j < type.inputs.size(); ++j) {
					uint16_t a;

					if (node->inputs[j].connections.size() == 0) {
						// No input, default it
						// TODO Take param value if specified
						a = _memory.size();
						_memory.push_back(0);

					} else {
						ProgramGraph::PortLocation src_port = node->inputs[j].connections[0];
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
				switch (node->type_id) {

					case VoxelGeneratorGraph::NODE_CURVE: {
						Ref<Curve> curve = node->params[0];
						CRASH_COND(curve.is_null());
						uint8_t is_monotonic_increasing;
						Interval range = get_curve_range(**curve, is_monotonic_increasing);
						append(program, is_monotonic_increasing);
						append(program, range.min);
						append(program, range.max);
						append(program, *curve);
					} break;

					case VoxelGeneratorGraph::NODE_IMAGE_2D: {
						Ref<Image> im = node->params[0];
						CRASH_COND(im.is_null());
						Interval range = get_heightmap_range(**im);
						append(program, range.min);
						append(program, range.max);
						append(program, *im);
					} break;

					case VoxelGeneratorGraph::NODE_NOISE_2D:
					case VoxelGeneratorGraph::NODE_NOISE_3D: {
						Ref<OpenSimplexNoise> noise = node->params[0];
						CRASH_COND(noise.is_null());
						append(program, *noise);
					} break;

					// TODO Worth it?
					case VoxelGeneratorGraph::NODE_CLAMP:
						append(program, node->params[0].operator float());
						append(program, node->params[1].operator float());
						break;

					case VoxelGeneratorGraph::NODE_REMAP: {
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
// They map the layout produced by the compilation.
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

float VoxelGraphRuntime::generate_single(const Vector3i &position) {
	// This part must be optimized for speed

#ifdef DEBUG_ENABLED
	CRASH_COND(_memory.size() == 0);
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

			case VoxelGeneratorGraph::NODE_SINE: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::sin(Math_PI * memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_FLOOR: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::floor(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_ABS: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::abs(memory[n.a_in]);
			} break;

			case VoxelGeneratorGraph::NODE_SQRT: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				memory[n.a_out] = Math::sqrt(memory[n.a_in]);
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

			case VoxelGeneratorGraph::NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				memory[n.a_out] = n.p_curve->interpolate_baked(memory[n.a_in]);
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

			default:
				CRASH_NOW();
				break;
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	return memory[memory.size() - 1];
}

Interval VoxelGraphRuntime::analyze_range(Vector3i min_pos, Vector3i max_pos) {
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

			case VoxelGeneratorGraph::NODE_SINE: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				Interval r = sin(Interval(min_memory[n.a_in], max_memory[n.a_in]) * Math_PI);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_FLOOR: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				// Floor is monotonic so I guess we can just do that
				min_memory[n.a_out] = Math::floor(min_memory[n.a_in]);
				max_memory[n.a_out] = Math::floor(max_memory[n.a_in]); // ceil?
			} break;

			case VoxelGeneratorGraph::NODE_ABS: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				Interval r = abs(Interval(min_memory[n.a_in], max_memory[n.a_in]));
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_SQRT: {
				const PNodeMonoFunc &n = read<PNodeMonoFunc>(_program, pc);
				Interval r = sqrt(Interval(min_memory[n.a_in], max_memory[n.a_in]));
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
				Interval dx = x1 - x0;
				Interval dy = y1 - y0;
				Interval dz = z1 - z0;
				Interval r = sqrt(dx * dx + dy * dy + dz * dz);
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
				Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				Interval r = get_osn_range_3d(n.p_noise, x, y, z);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_IMAGE_2D: {
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

	return Interval(min_memory[min_memory.size() - 1], max_memory[max_memory.size() - 1]);
}
