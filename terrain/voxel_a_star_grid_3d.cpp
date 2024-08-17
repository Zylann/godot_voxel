#include "voxel_a_star_grid_3d.h"
#include "../terrain/fixed_lod/voxel_terrain.h"
// #include "../util/string/format.h"
#include "../constants/voxel_string_names.h"
#include "../util/math/conv.h"

namespace zylann::voxel {

VoxelAStarGrid3DInternal::VoxelAStarGrid3DInternal() : _voxel_buffer(VoxelBuffer::ALLOCATOR_POOL) {}

void VoxelAStarGrid3DInternal::init_cache() {
	_grid_cache_size = math::ceildiv(get_region().size, Chunk::SIZE);
	_grid_cache.resize(_grid_cache_size.x * _grid_cache_size.y * _grid_cache_size.z);
	_grid_chunk_states.resize_no_init(_grid_cache.size());
	_grid_chunk_states.fill(false);
	_voxel_buffer.create(Vector3iUtil::create(Chunk::SIZE));

	// const Box3i region = get_region();
	// for (int y = region.pos.y; y < region.pos.y + region.size.y; ++y) {
	// 	println(format("Y={}", y));
	// 	for (int z = region.pos.z; z < region.pos.z + region.size.z; ++z) {
	// 		std::string s;
	// 		for (int x = region.pos.x; x < region.pos.x + region.size.x; ++x) {
	// 			if (is_solid(Vector3i(x, y, z))) {
	// 				s += " O";
	// 			} else {
	// 				s += " -";
	// 			}
	// 		}
	// 		println(s);
	// 	}
	// }
}

bool VoxelAStarGrid3DInternal::is_solid(Vector3i pos) {
	// TODO We could align the cache with the voxel chunk grid to avoid more expensive copies across chunk borders
	const Vector3i gpos = pos - get_region().position;
	const Vector3i cpos = gpos >> Chunk::SIZE_PO2;
	const Vector3i rpos = gpos & Chunk::SIZE_MASK;

	const unsigned int chunk_loc = Vector3iUtil::get_zxy_index(cpos, _grid_cache_size);
	Chunk chunk;

	if (_grid_chunk_states.get(chunk_loc)) {
		chunk = _grid_cache[chunk_loc];

	} else {
		ZN_PROFILE_SCOPE_NAMED("Caching voxels");
		ZN_ASSERT(data != nullptr);

		const VoxelBuffer::ChannelId channel_index = VoxelBuffer::CHANNEL_TYPE;
		const Vector3i copy_origin = (cpos << Chunk::SIZE_PO2) + get_region().position;
		data->copy(copy_origin, _voxel_buffer, 1 << channel_index);

		if (_voxel_buffer.get_channel_compression(channel_index) == VoxelBuffer::COMPRESSION_UNIFORM) {
			if (_voxel_buffer.get_voxel(0, 0, 0, channel_index) != 0) {
				chunk.solid_bits = 0xffffffffffffffff;
			} else {
				chunk.solid_bits = 0;
			}

		} else {
			chunk.solid_bits = 0;

			switch (_voxel_buffer.get_channel_depth(channel_index)) {
				case VoxelBuffer::DEPTH_8_BIT: {
					Span<const uint8_t> values;
					ZN_ASSERT(_voxel_buffer.get_channel_data(channel_index, values));
					uint64_t i = 0;
					// Assuming ZXY loop order
					for (const uint8_t v : values) {
						chunk.solid_bits |= (v == 0 ? uint64_t(0) : (uint64_t(1) << i));
						++i;
					}
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					Span<const uint16_t> values;
					ZN_ASSERT(_voxel_buffer.get_channel_data(channel_index, values));
					uint64_t i = 0;
					for (const uint16_t v : values) {
						chunk.solid_bits |= (v == 0 ? uint64_t(0) : (uint64_t(1) << i));
						++i;
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled channel depth");
					break;
			}

			_grid_chunk_states.set(chunk_loc);
		}

		_grid_cache[chunk_loc] = chunk;
	}

	return chunk.get_solid_bit(rpos);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelAStarGrid3D::set_terrain(VoxelTerrain *node) {
	ZN_ASSERT_RETURN(node != nullptr);
	// Can't modify the pathfinder while it is running in a different thread
	ZN_ASSERT_RETURN(_is_running_async == false);
	_path_finder.data = node->get_storage_shared();
}

TypedArray<Vector3i> VoxelAStarGrid3D::find_path(Vector3i from_position, Vector3i to_position) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN_V(_is_running_async == false, TypedArray<Vector3i>());
#ifdef DEBUG_ENABLED
	check_params(from_position, to_position);
#endif
	return find_path_internal(from_position, to_position);
}

#ifdef DEBUG_ENABLED
void VoxelAStarGrid3D::check_params(Vector3i from_position, Vector3i to_position) {
	if (get_region().size == Vector3i()) {
		ZN_PRINT_WARNING("The region is empty or not defined, no path will be found");
	}
	if (!get_region().contains(from_position)) {
		ZN_PRINT_WARNING("The current region does not contain the source position, no path will be found");
	}
	if (!get_region().contains(from_position)) {
		ZN_PRINT_WARNING("The current region does not contain the destination, no path will be found");
	}
}
#endif

namespace {
TypedArray<Vector3i> to_typed_array(Span<const Vector3i> items) {
	TypedArray<Vector3i> typed_array;
	typed_array.resize(items.size());
	for (unsigned int i = 0; i < items.size(); ++i) {
		typed_array[i] = items[i];
	}
	return typed_array;
}
} // namespace

TypedArray<Vector3i> VoxelAStarGrid3D::find_path_internal(Vector3i from_position, Vector3i to_position) {
	_path_finder.start(from_position, to_position);
	_path_finder.init_cache();

	while (_path_finder.is_running()) {
		_path_finder.step();
	}

	Span<const Vector3i> path = _path_finder.get_path();
	return to_typed_array(path);
}

void VoxelAStarGrid3D::set_region(Box3i region) {
	ZN_ASSERT_RETURN(_is_running_async == false);
	_path_finder.set_region(region);
}

Box3i VoxelAStarGrid3D::get_region() {
	return _path_finder.get_region();
}

void VoxelAStarGrid3D::find_path_async(Vector3i from_position, Vector3i to_position) {
	ZN_ASSERT_RETURN(_is_running_async == false);

#ifdef DEBUG_ENABLED
	check_params(from_position, to_position);
#endif

	_is_running_async = true;

	class Task : public IThreadedTask {
	public:
		Ref<VoxelAStarGrid3D> astar;
		Vector3i from_position;
		Vector3i to_position;

		void run(ThreadedTaskContext &ctx) override {
			ZN_ASSERT(astar.is_valid());
			TypedArray<Vector3i> path = astar->find_path_internal(from_position, to_position);
			astar->call_deferred(VoxelStringNames::get_singleton()._on_async_search_completed, path);
		}

		const char *get_debug_name() const override {
			return "VoxelAStarGrid3DTask";
		}
	};

	Task *task = ZN_NEW(Task);
	task->astar = Ref<VoxelAStarGrid3D>(this);
	task->from_position = from_position;
	task->to_position = to_position;

	VoxelEngine::get_singleton().push_async_task(task);
}

bool VoxelAStarGrid3D::is_running_async() const {
	return _is_running_async;
}

TypedArray<Vector3i> VoxelAStarGrid3D::debug_get_visited_positions() const {
	ZN_ASSERT_RETURN_V(_is_running_async == false, TypedArray<Vector3i>());
	StdVector<Vector3i> positions;
	_path_finder.debug_get_visited_points(positions);
	return to_typed_array(to_span(positions));
}

void VoxelAStarGrid3D::_b_set_region(AABB aabb) {
	set_region(Box3i(math::floor_to_int(aabb.position), math::floor_to_int(aabb.size)));
}

AABB VoxelAStarGrid3D::_b_get_region() {
	const Box3i region = get_region();
	return AABB(to_vec3(region.position), to_vec3(region.size));
}

// Intermediate method to enforce the signal to be emitted on the main thread
void VoxelAStarGrid3D::_b_on_async_search_completed(TypedArray<Vector3i> path) {
	_is_running_async = false;
	emit_signal(VoxelStringNames::get_singleton().async_search_completed, path);
}

void VoxelAStarGrid3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain", "terrain"), &VoxelAStarGrid3D::set_terrain);

	ClassDB::bind_method(D_METHOD("set_region", "box"), &VoxelAStarGrid3D::_b_set_region);
	ClassDB::bind_method(D_METHOD("get_region"), &VoxelAStarGrid3D::_b_get_region);

	ClassDB::bind_method(D_METHOD("find_path", "from_position", "to_position"), &VoxelAStarGrid3D::find_path);
	ClassDB::bind_method(
			D_METHOD("find_path_async", "from_position", "to_position"), &VoxelAStarGrid3D::find_path_async);
	ClassDB::bind_method(D_METHOD("is_running_async"), &VoxelAStarGrid3D::is_running_async);

	ClassDB::bind_method(D_METHOD("debug_get_visited_positions"), &VoxelAStarGrid3D::debug_get_visited_positions);

	// Internal
	ClassDB::bind_method(
			D_METHOD("_on_async_search_completed", "path"), &VoxelAStarGrid3D::_b_on_async_search_completed);

	ADD_SIGNAL(MethodInfo(
			"async_search_completed", PropertyInfo(Variant::ARRAY, "path", PROPERTY_HINT_ARRAY_TYPE, "Vector3i")));
}

} // namespace zylann::voxel
