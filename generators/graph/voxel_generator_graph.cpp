#include "voxel_generator_graph.h"
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
	const ProgramGraph::Node *node = create_node_internal(_graph, type_id, position, id);
	ERR_FAIL_COND_V(node == nullptr, ProgramGraph::NULL_ID);
	return node->id;
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
	return 1 << VoxelBuffer::CHANNEL_SDF;
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

void VoxelGeneratorGraph::generate_block(VoxelBlockRequest &input) {
	std::shared_ptr<VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}

	if (runtime == nullptr || !runtime->has_output()) {
		return;
	}

	VoxelBuffer &out_buffer = **input.voxel_buffer;

	const Vector3i bs = out_buffer.get_size();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	const Vector3i origin = input.origin_in_voxels;

	// TODO This may be shared across the module
	// Storing voxels is lossy on some depth configurations. They use normalized SDF,
	// so we must scale the values to make better use of the offered resolution
	const float sdf_scale = VoxelBuffer::get_sdf_quantization_scale(
			out_buffer.get_channel_depth(out_buffer.get_channel_depth(channel)));

	const int stride = 1 << input.lod;

	// Clip threshold must be higher for higher lod indexes because distances for one sampled voxel are also larger
	const float clip_threshold = sdf_scale * _sdf_clip_threshold * stride;

	// TODO Allow non-cubic block size when not using subdivision
	const int section_size = _use_subdivision ? _subdivision_size : min(min(bs.x, bs.y), bs.z);
	// Block size must be a multiple of section size
	ERR_FAIL_COND(bs.x % section_size != 0);
	ERR_FAIL_COND(bs.y % section_size != 0);
	ERR_FAIL_COND(bs.z % section_size != 0);

	Cache &cache = _cache;

	const unsigned int slice_buffer_size = section_size * section_size;
	runtime->prepare_state(cache.state, slice_buffer_size);

	cache.slice_cache.resize(slice_buffer_size);
	ArraySlice<float> slice_cache(cache.slice_cache, 0, cache.slice_cache.size());

	cache.x_cache.resize(cache.slice_cache.size());
	cache.y_cache.resize(cache.slice_cache.size());
	cache.z_cache.resize(cache.slice_cache.size());

	ArraySlice<float> x_cache(cache.x_cache, 0, cache.x_cache.size());
	ArraySlice<float> y_cache(cache.y_cache, 0, cache.y_cache.size());
	ArraySlice<float> z_cache(cache.z_cache, 0, cache.z_cache.size());

	const float air_sdf = _debug_clipped_blocks ? -1.f : 1.f;
	const float matter_sdf = _debug_clipped_blocks ? 1.f : -1.f;

	// For each subdivision of the block
	for (int sz = 0; sz < bs.z; sz += section_size) {
		for (int sy = 0; sy < bs.y; sy += section_size) {
			for (int sx = 0; sx < bs.x; sx += section_size) {
				VOXEL_PROFILE_SCOPE_NAMED("Section");

				const Vector3i rmin(sx, sy, sz);
				const Vector3i rmax = rmin + Vector3i(section_size);
				const Vector3i gmin = origin + (rmin << input.lod);
				const Vector3i gmax = origin + (rmax << input.lod);

				const Interval range = runtime->analyze_range(cache.state, gmin, gmax) * sdf_scale;
				if (range.min > clip_threshold && range.max > clip_threshold) {
					out_buffer.fill_area_f(air_sdf, rmin, rmax, channel);
					continue;

				} else if (range.min < -clip_threshold && range.max < -clip_threshold) {
					out_buffer.fill_area_f(matter_sdf, rmin, rmax, channel);
					continue;

				} else if (range.is_single_value()) {
					out_buffer.fill_area_f(range.min, rmin, rmax, channel);
					continue;
				}

				// The section may have the surface in it, we have to calculate it

				if (_use_optimized_execution_map) {
					// Optimize out branches of the graph that won't contribute to the result
					runtime->generate_optimized_execution_map(cache.state, false);
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
					VOXEL_PROFILE_SCOPE_NAMED("Slice");

					y_cache.fill(gy);

					// TODO Option to disable Y dependency caching
					runtime->generate_set(cache.state, x_cache, y_cache, z_cache, slice_cache,
							_use_xz_caching && ry != rmin.y, _use_optimized_execution_map);

					// TODO Flatten this further
					{
						VOXEL_PROFILE_SCOPE_NAMED("Copy to block");
						unsigned int i = 0;
						for (int rz = rmin.z; rz < rmax.z; ++rz) {
							for (int rx = rmin.x; rx < rmax.x; ++rx) {
								out_buffer.set_voxel_f(sdf_scale * slice_cache[i], rx, ry, rz, channel);
								++i;
							}
						}
					}
				}
			}
		}
	}

	out_buffer.compress_uniform_channels();
}

VoxelGraphRuntime::CompilationResult VoxelGeneratorGraph::compile() {
	std::shared_ptr<VoxelGraphRuntime> r = std::make_shared<VoxelGraphRuntime>();
	const VoxelGraphRuntime::CompilationResult result = r->compile(_graph, Engine::get_singleton()->is_editor_hint());

	if (result.success) {
		RWLockWrite wlock(_runtime_lock);
		_runtime = r;
	}

	return result;
}

// This is an external API which involves locking so better not use this internally
bool VoxelGeneratorGraph::is_good() const {
	RWLockRead rlock(_runtime_lock);
	return _runtime != nullptr && _runtime->has_output();
}

void VoxelGeneratorGraph::generate_set(ArraySlice<float> in_x, ArraySlice<float> in_y, ArraySlice<float> in_z,
		ArraySlice<float> out_sdf) {

	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND(_runtime == nullptr || !_runtime->has_output());
	Cache &cache = _cache;
	_runtime->prepare_state(cache.state, in_x.size());
	_runtime->generate_set(cache.state, in_x, in_y, in_z, out_sdf, false, false);
}

const VoxelGraphRuntime::State &VoxelGeneratorGraph::get_last_state_from_current_thread() {
	return _cache.state;
}

bool VoxelGeneratorGraph::try_get_output_port_address(ProgramGraph::PortLocation port, uint32_t &out_address) const {
	RWLockRead rlock(_runtime_lock);
	ERR_FAIL_COND_V(_runtime == nullptr || !_runtime->has_output(), false);
	uint16_t addr;
	const bool res = _runtime->try_get_output_port_address(port, addr);
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

	std::shared_ptr<const VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}

	ERR_FAIL_COND(runtime == nullptr || !runtime->has_output());

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> sdf_values;
		Ref<Image> im;
		std::shared_ptr<const VoxelGraphRuntime> runtime;
		VoxelGraphRuntime::State &state;
		const float ref_radius;
		const float sdf_min;
		const float sdf_max;

		ProcessChunk(VoxelGraphRuntime::State &p_state, float p_ref_radius, float p_sdf_min, float p_sdf_max) :
				state(p_state),
				ref_radius(p_ref_radius),
				sdf_min(p_sdf_min),
				sdf_max(p_sdf_max) {}

		void operator()(int x0, int y0, int width, int height) {
			VOXEL_PROFILE_SCOPE();

			const unsigned int area = width * height;
			x_coords.resize(area);
			y_coords.resize(area);
			z_coords.resize(area);
			sdf_values.resize(area);
			runtime->prepare_state(state, area);

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

			runtime->generate_set(state,
					to_slice(x_coords), to_slice(y_coords), to_slice(z_coords), to_slice(sdf_values), false, false);

			// Calculate final pixels
			im->lock();
			i = 0;
			for (int iy = y0; iy < ymax; ++iy) {
				for (int ix = x0; ix < xmax; ++ix) {
					const float sdf = sdf_values[i];
					const float nh = (-sdf - sdf_min) * nr;
					im->set_pixel(ix, iy, Color(nh, nh, nh));
					++i;
				}
			}
			im->unlock();
		}
	};

	Cache &cache = _cache;

	ProcessChunk pc(cache.state, ref_radius, sdf_min, sdf_max);
	pc.runtime = runtime;
	pc.im = im;
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

// If this generator is used to produce a planet, specifically using a spherical heightmap approach,
// then this function can be used to bake a map of the surface.
// Such maps can be used by shaders to sharpen the details of the planet when seen from far away.
void VoxelGeneratorGraph::bake_sphere_normalmap(Ref<Image> im, float ref_radius, float strength) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(im.is_null());

	std::shared_ptr<const VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}

	ERR_FAIL_COND(runtime == nullptr || !runtime->has_output());

	// This process would use too much memory if run over the entire image at once,
	// so we'll subdivide the load in smaller chunks
	struct ProcessChunk {
		std::vector<float> x_coords;
		std::vector<float> y_coords;
		std::vector<float> z_coords;
		std::vector<float> sdf_values_p; // TODO Could be used at the same time to get bump?
		std::vector<float> sdf_values_px;
		std::vector<float> sdf_values_py;
		Ref<Image> im;
		std::shared_ptr<const VoxelGraphRuntime> runtime;
		VoxelGraphRuntime::State &state;
		const float strength;
		const float ref_radius;

		ProcessChunk(VoxelGraphRuntime::State &p_state, float p_strength, float p_ref_radius) :
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
			runtime->prepare_state(state, area);

			const float ns = 2.f / strength;

			const Vector2 suv = Vector2(
					1.f / static_cast<float>(im->get_width()),
					1.f / static_cast<float>(im->get_height()));

			const Vector2 normal_step = 0.5f * Vector2(1.f, 1.f) / im->get_size();
			const Vector2 normal_step_x = Vector2(normal_step.x, 0.f);
			const Vector2 normal_step_y = Vector2(0.f, normal_step.y);

			const int xmax = x0 + width;
			const int ymax = y0 + height;

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
			runtime->generate_set(state,
					to_slice(x_coords), to_slice(y_coords), to_slice(z_coords), to_slice(sdf_values_p), false, false);

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
			runtime->generate_set(state,
					to_slice(x_coords), to_slice(y_coords), to_slice(z_coords), to_slice(sdf_values_px), false, false);

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
			runtime->generate_set(state,
					to_slice(x_coords), to_slice(y_coords), to_slice(z_coords), to_slice(sdf_values_py), false, false);

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

	ProcessChunk pc(cache.state, strength, ref_radius);
	pc.runtime = runtime;
	pc.im = im;
	for_chunks_2d(im->get_width(), im->get_height(), 32, pc);
}

// TODO This function isn't used yet, but whatever uses it should probably put locking and cache outside
float VoxelGeneratorGraph::generate_single(const Vector3i &position) {
	std::shared_ptr<const VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}
	ERR_FAIL_COND_V(runtime == nullptr || !runtime->has_output(), 0.f);
	Cache &cache = _cache;
	runtime->prepare_state(cache.state, 1);
	return runtime->generate_single(cache.state, position.to_vec3(), false);
}

Interval VoxelGeneratorGraph::analyze_range(Vector3i min_pos, Vector3i max_pos,
		bool optimize_execution_map, bool debug) const {
	std::shared_ptr<const VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}
	ERR_FAIL_COND_V(runtime == nullptr || !runtime->has_output(), Interval::from_single_value(0.f));
	Cache &cache = _cache;
	// Note, buffer size is irrelevant here, because range analysis doesn't use buffers
	runtime->prepare_state(cache.state, 1);
	Interval res = runtime->analyze_range(cache.state, min_pos, max_pos);
	if (optimize_execution_map) {
		runtime->generate_optimized_execution_map(cache.state, debug);
	}
	return res;
}

Ref<Resource> VoxelGeneratorGraph::duplicate(bool p_subresources) const {
	Ref<VoxelGeneratorGraph> d;
	d.instance();

	d->_graph.copy_from(_graph, p_subresources);
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
	if (::load_graph_from_variant_data(_graph, data)) {
		// It's possible to auto-compile on load because `graph_data` is the only property set by the loader,
		// which is enough to have all information we need
		compile();
	} else {
		_graph.clear();
	}
}

// Debug land

float VoxelGeneratorGraph::debug_measure_microseconds_per_voxel(bool singular) {
	std::shared_ptr<const VoxelGraphRuntime> runtime;
	{
		RWLockRead rlock(_runtime_lock);
		runtime = _runtime;
	}
	ERR_FAIL_COND_V(runtime == nullptr || !runtime->has_output(), 0.f);

	const uint32_t cube_size = 16;
	const uint32_t cube_count = 250;
	// const uint32_t cube_size = 100;
	// const uint32_t cube_count = 1;
	const uint32_t voxel_count = cube_size * cube_size * cube_size * cube_count;
	ProfilingClock profiling_clock;
	uint64_t elapsed_us = 0;

	Cache &cache = _cache;

	if (singular) {
		runtime->prepare_state(cache.state, 1);

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t z = 0; z < cube_size; ++z) {
				for (uint32_t y = 0; y < cube_size; ++y) {
					for (uint32_t x = 0; x < cube_size; ++x) {
						runtime->generate_single(cache.state, Vector3i(x, y, z).to_vec3(), false);
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

		runtime->prepare_state(cache.state, sx.size());

		for (uint32_t i = 0; i < cube_count; ++i) {
			profiling_clock.restart();

			for (uint32_t y = 0; y < cube_size; ++y) {
				runtime->generate_set(cache.state, sx, sy, sz, sdst, false, false);
			}

			elapsed_us += profiling_clock.restart();
		}
	}

	float us = static_cast<double>(elapsed_us) / voxel_count;
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

Vector2 VoxelGeneratorGraph::_b_analyze_range(Vector3 min_pos, Vector3 max_pos) const {
	const Interval r = analyze_range(Vector3i::from_floored(min_pos), Vector3i::from_floored(max_pos), false, false);
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
	ClassDB::bind_method(D_METHOD("analyze_range", "min_pos", "max_pos"), &VoxelGeneratorGraph::_b_analyze_range);

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
	BIND_ENUM_CONSTANT(NODE_TYPE_COUNT);
}
