#include "voxel_graph_runtime.h"
#include "../../util/macros.h"
#include "../../util/noise/fast_noise_lite.h"
#include "image_range_grid.h"
#include "range_utility.h"
#include "voxel_generator_graph.h"
#include "voxel_graph_node_db.h"

//#include <core/image.h>
#include <core/math/math_funcs.h>
#include <modules/opensimplex/open_simplex_noise.h>
#include <scene/resources/curve.h>
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

inline float get_pixel_repeat(const Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

inline float get_pixel_repeat_linear(const Image &im, float x, float y) {
	const int x0 = int(Math::floor(x));
	const int y0 = int(Math::floor(y));

	const float xf = x - x0;
	const float yf = y - y0;

	const float h00 = get_pixel_repeat(im, x0, y0);
	const float h10 = get_pixel_repeat(im, x0 + 1, y0);
	const float h01 = get_pixel_repeat(im, x0, y0 + 1);
	const float h11 = get_pixel_repeat(im, x0 + 1, y0 + 1);

	// Bilinear filter
	const float h = Math::lerp(Math::lerp(h00, h10, xf), Math::lerp(h01, h11, xf), yf);

	return h;
}

// TODO bicubic sampling
// inline float get_pixel_repeat_bicubic(const Image &im, float x, float y) {
// }

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
	// TODO Should be `const` but isn't because it auto-bakes, and it's a concern for multithreading
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
	// TODO Should be `const` but isn't because of an oversight in Godot
	OpenSimplexNoise *p_noise;
};

struct PNodeNoise3D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out;
	// TODO Should be `const` but isn't because of an oversight in Godot
	OpenSimplexNoise *p_noise;
};

struct PNodeImage2D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_out;
	Image *p_image;
	const ImageRangeGrid *p_image_range_grid;
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

// TODO `filter` param?
struct PNodeSphereHeightmap {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out;
	float p_radius;
	float p_factor;
	float min_height;
	float max_height;
	float norm_x;
	float norm_y;
	// TODO Should be `const` but isn't because of `lock()`
	Image *p_image;
	const ImageRangeGrid *p_image_range_grid;
};

struct PNodeNormalize3D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out_nx;
	uint16_t a_out_ny;
	uint16_t a_out_nz;
	uint16_t a_out_len;
};

struct PNodeFastNoise2D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_out;
	const FastNoiseLite *p_noise;
};

struct PNodeFastNoise3D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out;
	const FastNoiseLite *p_noise;
};

struct PNodeFastNoiseGradient2D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_out_x;
	uint16_t a_out_y;
	const FastNoiseLiteGradient *p_noise;
};

struct PNodeFastNoiseGradient3D {
	uint16_t a_x;
	uint16_t a_y;
	uint16_t a_z;
	uint16_t a_out_x;
	uint16_t a_out_y;
	uint16_t a_out_z;
	const FastNoiseLiteGradient *p_noise;
};

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

VoxelGraphRuntime::~VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
	_memory.resize(8, 0);
	_xzy_program_start = 0;

	const float fmax = std::numeric_limits<float>::max();
	_last_x = fmax;
	_last_z = fmax;

	_xz_last_min = Vector2(fmax, fmax);
	_xz_last_max = Vector2(fmax, fmax);

	_buffer_size = 0;
	for (auto it = _buffers.begin(); it != _buffers.end(); ++it) {
		Buffer &b = *it;
		if (b.data != nullptr) {
			memfree(b.data);
		}
	}
	_buffers.clear();

	_output_port_addresses.clear();
	_sdf_output_address = -1;
	_compilation_result = CompilationResult();

	for (size_t i = 0; i < _image_range_grids.size(); ++i) {
		ImageRangeGrid *im = _image_range_grids[i];
		memdelete(im);
	}
	_image_range_grids.clear();
}

bool VoxelGraphRuntime::compile(const ProgramGraph &graph, bool debug) {
	const bool success = _compile(graph, debug);
	if (success == false) {
		const CompilationResult result = _compilation_result;
		clear();
		_compilation_result = result;
	}
	return success;
}

bool VoxelGraphRuntime::_compile(const ProgramGraph &graph, bool debug) {
	clear();

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

	_memory.clear();

	struct MemoryHelper {
		std::vector<float> &value_mem;
		std::vector<Buffer> &buffer_mem;

		uint16_t add_var() {
			uint16_t a = value_mem.size();
			value_mem.push_back(0.f);
			Buffer b;
			b.data = nullptr;
			b.is_constant = false;
			buffer_mem.push_back(b);
			return a;
		}

		uint16_t add_constant(float v) {
			uint16_t a = value_mem.size();
			value_mem.push_back(v);
			Buffer b;
			b.data = nullptr;
			b.is_constant = true;
			b.constant_value = v;
			buffer_mem.push_back(b);
			return a;
		}
	};

	MemoryHelper mem{ _memory, _buffers };

	// Main inputs X, Y, Z
	mem.add_var();
	mem.add_var();
	mem.add_var();

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
				const uint16_t a = mem.add_constant(node->params[0].operator float());
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
					_compilation_result.success = false;
					_compilation_result.message = "Multiple SDF outputs are not supported";
					_compilation_result.node_id = node_id;
					return false;
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
						a = mem.add_constant(defval);

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
					const uint16_t a = mem.add_var();

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
						n.p_m0 = (max1 - min1) * (Math::is_equal_approx(max0, min0) ? 99999.f : 1.f / (max0 - min0));
						n.p_c1 = min1;
					} break;

					case VoxelGeneratorGraph::NODE_SMOOTHSTEP: {
						PNodeSmoothstep &n = get_or_create<PNodeSmoothstep>(program, offset);
						n.p_edge0 = node->params[0].operator float();
						n.p_edge1 = node->params[1].operator float();
					} break;

					case VoxelGeneratorGraph::NODE_CURVE: {
						PNodeCurve &n = get_or_create<PNodeCurve>(program, offset);
						Ref<Curve> curve = node->params[0];
						if (curve.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "Curve instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						// Make sure it is baked. We don't want multithreading to bail out because of a write operation
						// happening in `interpolate_baked`...
						curve->bake();
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
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "OpenSimplexNoise instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_NOISE_3D: {
						PNodeNoise3D &n = get_or_create<PNodeNoise3D>(program, offset);
						Ref<OpenSimplexNoise> noise = node->params[0];
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "OpenSimplexNoise instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_IMAGE_2D: {
						PNodeImage2D &n = get_or_create<PNodeImage2D>(program, offset);
						Ref<Image> im = node->params[0];
						if (im.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "Image instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						ImageRangeGrid *im_range = memnew(ImageRangeGrid);
						im_range->generate(**im);
						n.p_image = *im;
						n.p_image_range_grid = im_range;
						_image_range_grids.push_back(im_range);
					} break;

					case VoxelGeneratorGraph::NODE_SDF_SPHERE_HEIGHTMAP: {
						PNodeSphereHeightmap &n = get_or_create<PNodeSphereHeightmap>(program, offset);
						Ref<Image> im = node->params[0];
						const float radius = node->params[1];
						const float factor = node->params[2];
						if (im.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "Image instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						ImageRangeGrid *im_range = memnew(ImageRangeGrid);
						im_range->generate(**im);
						const Interval range = im_range->get_range() * factor;
						n.min_height = range.min;
						n.max_height = range.max;
						n.p_image = *im;
						n.p_image_range_grid = im_range;
						n.p_radius = radius;
						n.p_factor = factor;
						n.norm_x = im->get_width();
						n.norm_y = im->get_height();
						_image_range_grids.push_back(im_range);
					} break;

					case VoxelGeneratorGraph::NODE_FAST_NOISE_2D: {
						PNodeFastNoise2D &n = get_or_create<PNodeFastNoise2D>(program, offset);
						Ref<FastNoiseLite> noise = node->params[0];
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "FastNoiseLite instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_FAST_NOISE_3D: {
						PNodeFastNoise3D &n = get_or_create<PNodeFastNoise3D>(program, offset);
						Ref<FastNoiseLite> noise = node->params[0];
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "FastNoiseLite instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_2D: {
						PNodeFastNoiseGradient2D &n = get_or_create<PNodeFastNoiseGradient2D>(program, offset);
						Ref<FastNoiseLiteGradient> noise = node->params[0];
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "FastNoiseLiteGradient instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

					case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_3D: {
						PNodeFastNoiseGradient3D &n = get_or_create<PNodeFastNoiseGradient3D>(program, offset);
						Ref<FastNoiseLiteGradient> noise = node->params[0];
						if (noise.is_null()) {
							_compilation_result.success = false;
							_compilation_result.message = "FastNoiseLiteGradient instance is null";
							_compilation_result.node_id = node_id;
							return false;
						}
						n.p_noise = *noise;
					} break;

				} // switch special params

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
				// Append a special value after each operation
				append(program, VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif

			} break; // default

		} // switch type
	}

	// In case there is nothing
	while (_memory.size() < 4) {
		mem.add_var();
	}

	// Reserve space for range analysis
	_memory.resize(_memory.size() * 2);
	// Make it a copy to keep eventual constants at consistent adresses
	const size_t half_size = _memory.size() / 2;
	for (size_t i = 0, j = half_size; i < half_size; ++i, ++j) {
		_memory[j] = _memory[i];
	}

	CRASH_COND(_buffers.size() != half_size);

	PRINT_VERBOSE(String("Compiled voxel graph. Program size: {0}b, buffers: {1}")
						  .format(varray(
								  SIZE_T_TO_VARIANT(_program.size() * sizeof(float)),
								  SIZE_T_TO_VARIANT(_buffers.size()))));

	//ERR_FAIL_COND(_sdf_output_address == -1);
	_compilation_result.success = true;
	return true;
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

template <typename T>
inline T skew3(T x) {
	return (x * x * x + x) * 0.5f;
}

/*inline float cbrt(float x) {
	return Math::pow(x, 1.f / 3.f);
}

inline float unskew3(float x) {
	const float sqrt3 = 1.73205080757f; // Math::sqrt(3.f);
	const float cbrt12 = 2.28942848511f; // cbrt(12);
	const float cbrt18 = 2.62074139421f; // cbrt(18);
	x *= 2.f;
	const float a = -9.f * x + sqrt3 * Math::sqrt(27.f * x * x + 4.f);
	const float n = -cbrt12 * Math::pow(a, 2.f / 3.f) + 2.f * cbrt18;
	const float d = 6.f * cbrt(a);
	return n / d;
}*/

// This is mostly useful for generating planets from an existing heightmap
inline float sdf_sphere_heightmap(float x, float y, float z, float r, float m, Image &im, float min_h, float max_h,
		float norm_x, float norm_y) {

	const float d = Math::sqrt(x * x + y * y + z * z) + 0.0001f;
	const float sd = d - r;
	// Optimize when far enough from heightmap.
	// This introduces a discontinuity but it should be ok for clamped storage
	const float margin = 1.2f * (max_h - min_h);
	if (sd > max_h + margin || sd < min_h - margin) {
		return sd;
	}
	const float nx = x / d;
	const float ny = y / d;
	const float nz = z / d;
	// TODO Could use fast atan2, it doesn't have to be precise
	// https://github.com/ducha-aiki/fast_atan2/blob/master/fast_atan.cpp
	const float uvx = -Math::atan2(nz, nx) * VoxelConstants::INV_TAU + 0.5f;
	// This is an approximation of asin(ny)/(PI/2)
	// TODO It may be desirable to use the real function though,
	// in cases where we want to combine the same map in shaders
	const float ys = skew3(ny);
	const float uvy = -0.5f * ys + 0.5f;
	// TODO Not great, but in Godot 4.0 we won't need to lock anymore.
	// TODO Could use bicubic interpolation when the image is sampled at lower resolution than voxels
	const float h = get_pixel_repeat_linear(im, uvx * norm_x, uvy * norm_y);
	return sd - m * h;
}

inline Interval sdf_sphere_heightmap(Interval x, Interval y, Interval z, float r, float m,
		const ImageRangeGrid *im_range, float norm_x, float norm_y) {

	const Interval d = get_length(x, y, z) + 0.0001f;
	const Interval sd = d - r;
	// TODO There is a discontinuity here due to the optimization done in the regular function
	// Not sure yet how to implement it here. Worst case scenario, we remove it

	const Interval nx = x / d;
	const Interval ny = y / d;
	const Interval nz = z / d;

	const Interval ys = skew3(ny);
	const Interval uvy = -0.5f * ys + 0.5f;

	// atan2 returns results between -PI and PI but sometimes the angle can wrap, we have to account for this
	OptionalInterval atan_r1;
	const Interval atan_r0 = atan2(nz, nx, &atan_r1);

	Interval h;
	{
		const Interval uvx = -atan_r0 * VoxelConstants::INV_TAU + 0.5f;
		h = im_range->get_range(uvx * norm_x, uvy * norm_y);
	}
	if (atan_r1.valid) {
		const Interval uvx = -atan_r1.value * VoxelConstants::INV_TAU + 0.5f;
		h.add_interval(im_range->get_range(uvx * norm_x, uvy * norm_y));
	}

	return sd - m * h;
}

float VoxelGraphRuntime::generate_single(const Vector3 &position) {
	// This part must be optimized for speed

#ifdef DEBUG_ENABLED
	CRASH_COND(_memory.size() == 0);
#endif
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(!has_output(), 0.0, "The graph has no SDF output");
#endif

	ArraySlice<float> memory(_memory, 0, _memory.size() / 2);
	memory[0] = position.x;
	memory[1] = position.y;
	memory[2] = position.z;

	uint32_t pc;
	// Note, when sampling beyond negative or positive 16,777,216, this optimization may cease to work
	if (position.x == _last_x && position.z == _last_z) {
		pc = _xzy_program_start;
	} else {
		pc = 0;
	}

	_last_x = position.x;
	_last_z = position.z;

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
#ifdef DEBUG_ENABLED
			case VoxelGeneratorGraph::NODE_CONSTANT:
			case VoxelGeneratorGraph::NODE_INPUT_X:
			case VoxelGeneratorGraph::NODE_INPUT_Y:
			case VoxelGeneratorGraph::NODE_INPUT_Z:
			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;
#endif

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
				memory[n.a_out] = clamp(memory[n.a_x], n.p_min, n.p_max);
			} break;

			case VoxelGeneratorGraph::NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				memory[n.a_out] = (memory[n.a_x] - n.p_c0) * n.p_m0 + n.p_c1;
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
				// TODO Not great, but in Godot 4.0 we won't need to lock anymore.
				// Otherwise, need to do it in a pre-run and post-run
				n.p_image->lock();
				// TODO Allow to use bilinear filtering?
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

			case VoxelGeneratorGraph::NODE_SDF_SPHERE_HEIGHTMAP: {
				const PNodeSphereHeightmap &n = read<PNodeSphereHeightmap>(_program, pc);
				n.p_image->lock();
				memory[n.a_out] = sdf_sphere_heightmap(memory[n.a_x], memory[n.a_y], memory[n.a_z],
						n.p_radius, n.p_factor, *n.p_image, n.min_height, n.max_height, n.norm_x, n.norm_y);
				n.p_image->unlock();
			} break;

			case VoxelGeneratorGraph::NODE_NORMALIZE_3D: {
				const PNodeNormalize3D n = read<PNodeNormalize3D>(_program, pc);
				const float x = memory[n.a_x];
				const float y = memory[n.a_y];
				const float z = memory[n.a_z];
				float len = Math::sqrt(x * x + y * y + z * z);
				const float inv_len = 1.f / len;
				memory[n.a_out_nx] = x * inv_len;
				memory[n.a_out_ny] = y * inv_len;
				memory[n.a_out_nz] = z * inv_len;
				memory[n.a_out_len] = len;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_2D: {
				const PNodeFastNoise2D &n = read<PNodeFastNoise2D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_2d(memory[n.a_x], memory[n.a_y]);
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_3D: {
				const PNodeFastNoise3D &n = read<PNodeFastNoise3D>(_program, pc);
				memory[n.a_out] = n.p_noise->get_noise_3d(memory[n.a_x], memory[n.a_y], memory[n.a_z]);
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_2D: {
				const PNodeFastNoiseGradient2D &n = read<PNodeFastNoiseGradient2D>(_program, pc);
				float x = memory[n.a_x];
				float y = memory[n.a_y];
				n.p_noise->warp_2d(x, y);
				memory[n.a_out_x] = x;
				memory[n.a_out_y] = y;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_3D: {
				const PNodeFastNoiseGradient3D &n = read<PNodeFastNoiseGradient3D>(_program, pc);
				float x = memory[n.a_x];
				float y = memory[n.a_y];
				float z = memory[n.a_z];
				n.p_noise->warp_3d(x, y, z);
				memory[n.a_out_x] = x;
				memory[n.a_out_y] = y;
				memory[n.a_out_z] = z;
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

template <typename F>
inline void do_monop(const std::vector<uint8_t> &program, uint32_t &pc,
		ArraySlice<VoxelGraphRuntime::Buffer> buffers, uint32_t buffer_size, F f) {

	const PNodeMonop &n = read<PNodeMonop>(program, pc);
	VoxelGraphRuntime::Buffer &out = buffers[n.a_out];
	if (out.is_constant) {
		return;
	}
	const VoxelGraphRuntime::Buffer &a = buffers[n.a_in];
	for (uint32_t i = 0; i < buffer_size; ++i) {
		out.data[i] = f(a.data[i]);
	}
}

template <typename F>
inline void do_binop(const std::vector<uint8_t> &program, uint32_t &pc,
		ArraySlice<VoxelGraphRuntime::Buffer> buffers, uint32_t buffer_size, F f) {

	const PNodeBinop &n = read<PNodeBinop>(program, pc);
	VoxelGraphRuntime::Buffer &out = buffers[n.a_out];
	if (out.is_constant) {
		return;
	}

	const VoxelGraphRuntime::Buffer &a = buffers[n.a_i0];
	const VoxelGraphRuntime::Buffer &b = buffers[n.a_i1];

	if (a.is_constant || b.is_constant) {
		float c;
		const float *v;

		if (a.is_constant) {
			c = a.constant_value;
			v = b.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(c, v[i]);
			}
		} else {
			c = b.constant_value;
			v = a.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(v[i], c);
			}
		}

	} else {
		for (uint32_t i = 0; i < buffer_size; ++i) {
			out.data[i] = f(a.data[i], b.data[i]);
		}
	}
}

inline void do_division(const std::vector<uint8_t> &program, uint32_t pc,
		ArraySlice<VoxelGraphRuntime::Buffer> buffers, uint32_t buffer_size) {

	const PNodeBinop &n = read<PNodeBinop>(program, pc);
	VoxelGraphRuntime::Buffer &out = buffers[n.a_out];
	if (out.is_constant) {
		return;
	}

	const VoxelGraphRuntime::Buffer &a = buffers[n.a_i0];
	const VoxelGraphRuntime::Buffer &b = buffers[n.a_i1];

	if (a.is_constant || b.is_constant) {
		float c;
		const float *v;

		if (a.is_constant) {
			c = a.constant_value;
			v = b.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				if (b.data[i] == 0.f) {
					out.data[i] = 0.f;
				} else {
					out.data[i] = c / v[i];
				}
			}

		} else {
			c = b.constant_value;
			v = a.data;
			if (c == 0.f) {
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = 0.f;
				}
			} else {
				c = 1.f / c;
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = v[i] * c;
				}
			}
		}

	} else {
		for (uint32_t i = 0; i < buffer_size; ++i) {
			if (b.data[i] == 0.f) {
				out.data[i] = 0.f;
			} else {
				out.data[i] = a.data[i] / b.data[i];
			}
		}
	}
}

void VoxelGraphRuntime::generate_xz_slice(ArraySlice<float> dst, Vector2i dst_size, Vector2 min, Vector2 max,
		float y, int stride) {

	struct L {
		static inline void alloc_buffer(Buffer &buffer, int buffer_size) {
			// TODO Use pool?
			if (buffer.data == nullptr) {
				buffer.data = reinterpret_cast<float *>(memrealloc(buffer.data, buffer_size * sizeof(float)));
			} else {
				buffer.data = reinterpret_cast<float *>(memalloc(buffer_size * sizeof(float)));
			}
		}
	};

#ifdef DEBUG_ENABLED
	CRASH_COND(_buffers.size() == 0);
	CRASH_COND(_buffers.size() != _memory.size() / 2);
#endif
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_MSG(!has_output(), "The graph has no SDF output");
#endif

	ArraySlice<Buffer> buffers(_buffers, 0, _buffers.size());
	const uint32_t buffer_size = dst_size.x * dst_size.y;
	const bool buffer_size_changed = buffer_size != _buffer_size;
	_buffer_size = buffer_size;

	// Input buffers
	{
		// TODO When not using range analysis, it should be possible to fill the input buffers with whatever we want.
		// So eventually we should put these buffers out

		// TODO User-provided buffers may not need to be owned, or even recomputed

		Buffer &x_buffer = buffers[0];
		Buffer &y_buffer = buffers[1];
		Buffer &z_buffer = buffers[2];

		x_buffer.is_constant = false;
		z_buffer.is_constant = false;

		// Y is fixed
		y_buffer.is_constant = true;
		y_buffer.constant_value = y;

		if (buffer_size_changed) {
			L::alloc_buffer(x_buffer, buffer_size);
			L::alloc_buffer(y_buffer, buffer_size);
			L::alloc_buffer(z_buffer, buffer_size);
		}

		for (auto i = 0; i < buffer_size; ++i) {
			y_buffer.data[i] = y;
		}

		int i = 0;

		if (stride == 1) {
			for (int zi = 0; zi < dst_size.y; ++zi) {
				for (int xi = 0; xi < dst_size.x; ++xi) {
					x_buffer.data[i] = xi;
					z_buffer.data[i] = zi;
					++i;
				}
			}

		} else if (stride > 0) {
			for (int zi = 0; zi < dst_size.y; ++zi) {
				const float z = min.y + zi * stride;
				for (int xi = 0; xi < dst_size.x; ++xi) {
					x_buffer.data[i] = min.x + xi * stride;
					z_buffer.data[i] = z;
					++i;
				}
			}

		} else {
			// Slowest generic method
			for (int zi = 0; zi < dst_size.y; ++zi) {
				float zt = static_cast<float>(zi) / static_cast<float>(dst_size.y);
				float z = Math::lerp(min.y, max.y, zt);
				for (int xi = 0; xi < dst_size.x; ++xi) {
					float xt = static_cast<float>(xi) / static_cast<float>(dst_size.x);
					x_buffer.data[i] = Math::lerp(min.x, max.x, xt);
					z_buffer.data[i] = z;
					++i;
				}
			}
		}
	}

	// Prepare buffers
	{
		if (buffer_size_changed) {
			// We ignore input buffers, these are supposed to be setup already
			for (size_t i = 3; i < _buffers.size(); ++i) {
				Buffer &buffer = _buffers[i];

				L::alloc_buffer(buffer, buffer_size);

				if (buffer.is_constant) {
					for (auto i = 0; i < buffer_size; ++i) {
						buffer.data[i] = buffer.constant_value;
					}
				}
			}
		}

		/*if (use_range_analysis) {
			// TODO To be really worth it, we may need a runtime graph traversal pass,
			// where we build an execution map of nodes that are worthy 🔨

			const float ra_min = _memory[i];
			const float ra_max = _memory[i + _memory.size() / 2];

			buffer.is_constant = (ra_min == ra_max);
			if (buffer.is_constant) {
				buffer.constant_value = ra_min;
			}
		}*/
	}

	uint32_t pc;
	// Note, when sampling beyond negative or positive 16,777,216, this optimization may cease to work
	if (_xz_last_min == min && _xz_last_max == max && !buffer_size_changed) {
		pc = _xzy_program_start;
	} else {
		pc = 0;
	}

	_xz_last_min = min;
	_xz_last_max = max;

	// STL is unreadable on debug builds of Godot, because _DEBUG isn't defined
	//#ifdef DEBUG_ENABLED
	//	const size_t memory_size = memory.size();
	//	const size_t program_size = _program.size();
	//	const float *memory_raw = memory.data();
	//	const uint8_t *program_raw = (const uint8_t *)_program.data();
	//#endif

	// TODO Most operations need SIMD support
	// TODO This mode may be better implemented with function pointers

	while (pc < _program.size()) {
		const uint8_t opid = _program[pc++];

		switch (opid) {
#ifdef DEBUG_ENABLED
			case VoxelGeneratorGraph::NODE_CONSTANT:
			case VoxelGeneratorGraph::NODE_INPUT_X:
			case VoxelGeneratorGraph::NODE_INPUT_Y:
			case VoxelGeneratorGraph::NODE_INPUT_Z:
			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				// Not part of the runtime
				CRASH_NOW();
				break;
#endif

			case VoxelGeneratorGraph::NODE_ADD: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return a + b; });
			} break;

			case VoxelGeneratorGraph::NODE_SUBTRACT: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return a - b; });
			} break;

			case VoxelGeneratorGraph::NODE_MULTIPLY: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return a * b; });
			} break;

			case VoxelGeneratorGraph::NODE_DIVIDE: {
				do_division(_program, pc, buffers, buffer_size);
			} break;

			case VoxelGeneratorGraph::NODE_SIN: {
				do_monop(_program, pc, buffers, buffer_size, [](float a) { return Math::sin(a); });
			} break;

			case VoxelGeneratorGraph::NODE_FLOOR: {
				do_monop(_program, pc, buffers, buffer_size, [](float a) { return Math::floor(a); });
			} break;

			case VoxelGeneratorGraph::NODE_ABS: {
				do_monop(_program, pc, buffers, buffer_size, [](float a) { return Math::abs(a); });
			} break;

			case VoxelGeneratorGraph::NODE_SQRT: {
				do_monop(_program, pc, buffers, buffer_size, [](float a) { return Math::sqrt(a); });
			} break;

			case VoxelGeneratorGraph::NODE_FRACT: {
				do_monop(_program, pc, buffers, buffer_size, [](float a) { return a - Math::floor(a); });
			} break;

			case VoxelGeneratorGraph::NODE_STEPIFY: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return Math::stepify(a, b); });
			} break;

			case VoxelGeneratorGraph::NODE_WRAP: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return wrapf(a, b); });
			} break;

			case VoxelGeneratorGraph::NODE_MIN: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return ::min(a, b); });
			} break;

			case VoxelGeneratorGraph::NODE_MAX: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return ::max(a, b); });
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_2D: {
				const PNodeDistance2D &n = read<PNodeDistance2D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x0 = buffers[n.a_x0];
				const Buffer &y0 = buffers[n.a_y0];
				const Buffer &x1 = buffers[n.a_x1];
				const Buffer &y1 = buffers[n.a_y1];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = Math::sqrt(
							squared(x1.data[i] - x0.data[i]) +
							squared(y1.data[i] - y0.data[i]));
				}
			} break;

			case VoxelGeneratorGraph::NODE_DISTANCE_3D: {
				const PNodeDistance3D &n = read<PNodeDistance3D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x0 = buffers[n.a_x0];
				const Buffer &y0 = buffers[n.a_y0];
				const Buffer &z0 = buffers[n.a_z0];
				const Buffer &x1 = buffers[n.a_x1];
				const Buffer &y1 = buffers[n.a_y1];
				const Buffer &z1 = buffers[n.a_z1];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = Math::sqrt(
							squared(x1.data[i] - x0.data[i]) +
							squared(y1.data[i] - y0.data[i]) +
							squared(z1.data[i] - z0.data[i]));
				}
			} break;

			case VoxelGeneratorGraph::NODE_MIX: {
				const PNodeMix &n = read<PNodeMix>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_i0];
				const Buffer &b = buffers[n.a_i1];
				const Buffer &r = buffers[n.a_ratio];
				if (a.is_constant) {
					const float ca = a.constant_value;
					if (b.is_constant) {
						const float cb = b.constant_value;
						for (uint32_t i = 0; i < buffer_size; ++i) {
							out.data[i] = Math::lerp(ca, cb, r.data[i]);
						}
					} else {
						for (uint32_t i = 0; i < buffer_size; ++i) {
							out.data[i] = Math::lerp(ca, b.data[i], r.data[i]);
						}
					}
				} else if (b.is_constant) {
					const float cb = b.constant_value;
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = Math::lerp(a.data[i], cb, r.data[i]);
					}
				} else {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = Math::lerp(a.data[i], b.data[i], r.data[i]);
					}
				}
			} break;

			case VoxelGeneratorGraph::NODE_CLAMP: {
				const PNodeClamp &n = read<PNodeClamp>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_x];
				const float cmin = n.p_min;
				const float cmax = n.p_max;
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = clamp(a.data[i], cmin, cmax);
				}
			} break;

			case VoxelGeneratorGraph::NODE_REMAP: {
				const PNodeRemap &n = read<PNodeRemap>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_x];
				const float c0 = n.p_c0;
				const float m0 = n.p_m0;
				const float c1 = n.p_c1;
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = (a.data[i] - c0) * m0 + c1;
				}
			} break;

			case VoxelGeneratorGraph::NODE_SMOOTHSTEP: {
				const PNodeSmoothstep &n = read<PNodeSmoothstep>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_x];
				const float edge0 = n.p_edge0;
				const float edge1 = n.p_edge1;
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = smoothstep(edge0, edge1, a.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_CURVE: {
				const PNodeCurve &n = read<PNodeCurve>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_in];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = n.p_curve->interpolate_baked(a.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_SELECT: {
				const PNodeSelect &n = read<PNodeSelect>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &a = buffers[n.a_i0];
				const Buffer &b = buffers[n.a_i1];
				const Buffer &threshold = buffers[n.a_threshold];
				const Buffer &t = buffers[n.a_t];
				if (t.is_constant && threshold.is_constant) {
					const float *src = t.constant_value < threshold.constant_value ? a.data : b.data;
					for (uint32_t i = 0; i < buffer_size; ++i) {
						memcpy(out.data, src, buffer_size * sizeof(float));
					}
				} else if (a.is_constant && b.is_constant && a.constant_value == b.constant_value) {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						memcpy(out.data, a.data, buffer_size * sizeof(float));
					}
				} else {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = select(a.data[i], b.data[i], threshold.data[i], t.data[i]);
					}
				}
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_2D: {
				const PNodeNoise2D &n = read<PNodeNoise2D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = n.p_noise->get_noise_2d(x.data[i], y.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_NOISE_3D: {
				const PNodeNoise3D &n = read<PNodeNoise3D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = n.p_noise->get_noise_3d(x.data[i], y.data[i], z.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_IMAGE_2D: {
				const PNodeImage2D &n = read<PNodeImage2D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				Image &im = *n.p_image;
				im.lock();
				// TODO Allow to use bilinear filtering?
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = get_pixel_repeat(im, x.data[i], y.data[i]);
				}
				im.unlock();
			} break;

			case VoxelGeneratorGraph::NODE_SDF_PLANE: {
				do_binop(_program, pc, buffers, buffer_size, [](float a, float b) { return a - b; });
			} break;

			case VoxelGeneratorGraph::NODE_SDF_BOX: {
				const PNodeSdfBox &n = read<PNodeSdfBox>(_program, pc);
				// TODO We should really move origin away, or make it params
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				const Buffer &sx = buffers[n.a_sx];
				const Buffer &sy = buffers[n.a_sy];
				const Buffer &sz = buffers[n.a_sz];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = sdf_box(
							Vector3(x.data[i], y.data[i], z.data[i]),
							Vector3(sx.data[i], sy.data[i], sz.data[i]));
				}
			} break;

			case VoxelGeneratorGraph::NODE_SDF_SPHERE: {
				const PNodeSdfSphere &n = read<PNodeSdfSphere>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				// TODO Move radius to param?
				const Buffer &r = buffers[n.a_r];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					const Vector3 pos(x.data[i], y.data[i], z.data[i]);
					out.data[i] = pos.length() - r.data[i];
				}
			} break;

			case VoxelGeneratorGraph::NODE_SDF_TORUS: {
				const PNodeSdfTorus &n = read<PNodeSdfTorus>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				// TODO Move radii to param?
				const Buffer &r0 = buffers[n.a_r0];
				const Buffer &r1 = buffers[n.a_r1];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					const Vector3 pos(x.data[i], y.data[i], z.data[i]);
					out.data[n.a_out] = sdf_torus(pos, r0.data[i], r1.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_SDF_SPHERE_HEIGHTMAP: {
				const PNodeSphereHeightmap &n = read<PNodeSphereHeightmap>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				n.p_image->lock();
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = sdf_sphere_heightmap(x.data[i], y.data[i], z.data[i],
							n.p_radius, n.p_factor, *n.p_image, n.min_height, n.max_height, n.norm_x, n.norm_y);
				}
				n.p_image->unlock();
			} break;

			case VoxelGeneratorGraph::NODE_NORMALIZE_3D: {
				const PNodeNormalize3D n = read<PNodeNormalize3D>(_program, pc);
				Buffer &out_nx = buffers[n.a_out_nx];
				Buffer &out_ny = buffers[n.a_out_ny];
				Buffer &out_nz = buffers[n.a_out_nz];
				Buffer &out_len = buffers[n.a_out_len];
				const Buffer &xb = buffers[n.a_x];
				const Buffer &yb = buffers[n.a_y];
				const Buffer &zb = buffers[n.a_z];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					const float x = xb.data[i];
					const float y = yb.data[i];
					const float z = zb.data[i];
					float len = Math::sqrt(squared(x) + squared(y) + squared(z));
					out_nx.data[i] = x / len;
					out_ny.data[i] = y / len;
					out_nz.data[i] = z / len;
					out_len.data[i] = len;
				}
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_2D: {
				const PNodeFastNoise2D &n = read<PNodeFastNoise2D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = n.p_noise->get_noise_2d(x.data[i], y.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_3D: {
				const PNodeFastNoise3D &n = read<PNodeFastNoise3D>(_program, pc);
				Buffer &out = buffers[n.a_out];
				if (out.is_constant) {
					return;
				}
				const Buffer &x = buffers[n.a_x];
				const Buffer &y = buffers[n.a_y];
				const Buffer &z = buffers[n.a_z];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = n.p_noise->get_noise_3d(x.data[i], y.data[i], z.data[i]);
				}
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_2D: {
				const PNodeFastNoiseGradient2D &n = read<PNodeFastNoiseGradient2D>(_program, pc);
				Buffer &out_x = buffers[n.a_out_x];
				Buffer &out_y = buffers[n.a_out_y];
				const Buffer &xb = buffers[n.a_x];
				const Buffer &yb = buffers[n.a_y];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					float x = xb.data[i];
					float y = yb.data[i];
					n.p_noise->warp_2d(x, y);
					out_x.data[i] = x;
					out_y.data[i] = y;
				}
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_3D: {
				const PNodeFastNoiseGradient3D &n = read<PNodeFastNoiseGradient3D>(_program, pc);
				Buffer &out_x = buffers[n.a_out_x];
				Buffer &out_y = buffers[n.a_out_y];
				Buffer &out_z = buffers[n.a_out_z];
				const Buffer &xb = buffers[n.a_x];
				const Buffer &yb = buffers[n.a_y];
				const Buffer &zb = buffers[n.a_z];
				for (uint32_t i = 0; i < buffer_size; ++i) {
					float x = xb.data[i];
					float y = yb.data[i];
					float z = zb.data[i];
					n.p_noise->warp_3d(x, y, z);
					out_x.data[i] = x;
					out_y.data[i] = y;
					out_z.data[i] = z;
				}
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

	// Populate output buffers
	Buffer &sdf_output_buffer = buffers[_sdf_output_address];
	if (sdf_output_buffer.is_constant) {
		for (int i = 0; i < buffer_size; ++i) {
			dst[i] = sdf_output_buffer.constant_value;
		}
	} else {
		memcpy(dst.data(), sdf_output_buffer.data, buffer_size * sizeof(float));
	}
}

Interval VoxelGraphRuntime::analyze_range(Vector3i min_pos, Vector3i max_pos) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(!has_output(), Interval(), "The graph has no SDF output");
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
				Interval r = (x - n.p_c0) * n.p_m0 + n.p_c1;
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
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval r = n.p_image_range_grid->get_range(x, y);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
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

			case VoxelGeneratorGraph::NODE_SDF_SPHERE_HEIGHTMAP: {
				const PNodeSphereHeightmap &n = read<PNodeSphereHeightmap>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval r = sdf_sphere_heightmap(x, y, z, n.p_radius, n.p_factor, n.p_image_range_grid,
						n.norm_x, n.norm_y);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_NORMALIZE_3D: {
				const PNodeNormalize3D n = read<PNodeNormalize3D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval len = sqrt(x * x + y * y + z * z);
				const Interval nx = x / len;
				const Interval ny = y / len;
				const Interval nz = z / len;
				min_memory[n.a_out_nx] = nx.min;
				min_memory[n.a_out_ny] = ny.min;
				min_memory[n.a_out_nz] = nz.min;
				min_memory[n.a_out_len] = len.min;
				max_memory[n.a_out_nx] = nx.max;
				max_memory[n.a_out_ny] = ny.max;
				max_memory[n.a_out_nz] = nz.max;
				max_memory[n.a_out_len] = len.max;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_2D: {
				const PNodeFastNoise2D &n = read<PNodeFastNoise2D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval r = get_fnl_range_2d(n.p_noise, x, y);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_3D: {
				const PNodeFastNoise3D &n = read<PNodeFastNoise3D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval r = get_fnl_range_3d(n.p_noise, x, y, z);
				min_memory[n.a_out] = r.min;
				max_memory[n.a_out] = r.max;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_2D: {
				const PNodeFastNoiseGradient2D &n = read<PNodeFastNoiseGradient2D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval2 r = get_fnl_gradient_range_2d(n.p_noise, x, y);
				min_memory[n.a_out_x] = r.x.min;
				max_memory[n.a_out_x] = r.x.max;
				min_memory[n.a_out_y] = r.y.min;
				max_memory[n.a_out_y] = r.y.max;
			} break;

			case VoxelGeneratorGraph::NODE_FAST_NOISE_GRADIENT_3D: {
				const PNodeFastNoiseGradient3D &n = read<PNodeFastNoiseGradient3D>(_program, pc);
				const Interval x(min_memory[n.a_x], max_memory[n.a_x]);
				const Interval y(min_memory[n.a_y], max_memory[n.a_y]);
				const Interval z(min_memory[n.a_z], max_memory[n.a_z]);
				const Interval3 r = get_fnl_gradient_range_3d(n.p_noise, x, y, z);
				min_memory[n.a_out_x] = r.x.min;
				max_memory[n.a_out_x] = r.x.max;
				min_memory[n.a_out_y] = r.y.min;
				max_memory[n.a_out_y] = r.y.max;
				min_memory[n.a_out_z] = r.z.min;
				max_memory[n.a_out_z] = r.z.max;
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
