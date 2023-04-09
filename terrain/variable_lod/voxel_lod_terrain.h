#ifndef VOXEL_LOD_TERRAIN_HPP
#define VOXEL_LOD_TERRAIN_HPP

#include "../../engine/mesh_block_task.h"
#include "../../engine/voxel_engine.h"
#include "../../storage/voxel_data.h"
#include "../../util/godot/shader_material_pool.h"
#include "../voxel_mesh_map.h"
#include "../voxel_node.h"
#include "lod_octree.h"
#include "voxel_lod_terrain_update_data.h"
#include "voxel_mesh_block_vlt.h"

#include <map>
#include <unordered_set>

#ifdef TOOLS_ENABLED
#include "../../editor/voxel_debug.h"
#endif

namespace zylann::voxel {

class VoxelTool;
class VoxelStream;
class VoxelInstancer;

class ShaderMaterialPoolVLT : public ShaderMaterialPool {
public:
	void recycle(Ref<ShaderMaterial> material);
};

// Paged terrain made of voxel blocks of variable level of detail.
// Designed for highest view distances, preferably using smooth voxels.
// Voxels are polygonized around the viewer by distance in a very large sphere, usually extending beyond far clip.
// VoxelStream and VoxelGenerator must support LOD.
class VoxelLodTerrain : public VoxelNode {
	GDCLASS(VoxelLodTerrain, VoxelNode)
public:
	VoxelLodTerrain();
	~VoxelLodTerrain();

	Ref<Material> get_material() const;
	void set_material(Ref<Material> p_material);

	Ref<VoxelStream> get_stream() const override;
	void set_stream(Ref<VoxelStream> p_stream) override;

	Ref<VoxelGenerator> get_generator() const override;
	void set_generator(Ref<VoxelGenerator> p_stream) override;

	Ref<VoxelMesher> get_mesher() const override;
	void set_mesher(Ref<VoxelMesher> p_mesher) override;

	int get_view_distance() const;
	void set_view_distance(int p_distance_in_voxels);

	void set_lod_distance(float p_lod_distance);
	float get_lod_distance() const;

	void set_lod_count(int p_lod_count);
	int get_lod_count() const;

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const;

	// Sets up to which amount of LODs collision will generate. -1 means all of them.
	void set_collision_lod_count(int lod_count);
	int get_collision_lod_count() const;

	void set_collision_layer(int layer);
	int get_collision_layer() const;

	void set_collision_mask(int mask);
	int get_collision_mask() const;

	void set_collision_margin(float margin);
	float get_collision_margin() const;

	int get_data_block_region_extent() const;
	int get_mesh_block_region_extent() const;

	Vector3i voxel_to_data_block_position(Vector3 vpos, int lod_index) const;
	Vector3i voxel_to_mesh_block_position(Vector3 vpos, int lod_index) const;

	unsigned int get_data_block_size_pow2() const;
	unsigned int get_data_block_size() const;
	// void set_data_block_size_po2(unsigned int p_block_size_po2);

	unsigned int get_mesh_block_size_pow2() const;
	unsigned int get_mesh_block_size() const;
	void set_mesh_block_size(unsigned int mesh_block_size);

	void set_full_load_mode_enabled(bool enabled);
	bool is_full_load_mode_enabled() const;

	void set_threaded_update_enabled(bool enabled);
	bool is_threaded_update_enabled() const;

	void set_normalmap_enabled(bool enable);
	bool is_normalmap_enabled() const;

	void set_octahedral_normal_encoding(bool enable);
	bool get_octahedral_normal_encoding() const;

	void set_normalmap_tile_resolution_min(int resolution);
	int get_normalmap_tile_resolution_min() const;

	void set_normalmap_tile_resolution_max(int resolution);
	int get_normalmap_tile_resolution_max() const;

	void set_normalmap_begin_lod_index(int lod_index);
	int get_normalmap_begin_lod_index() const;

	void set_normalmap_max_deviation_degrees(int angle);
	int get_normalmap_max_deviation_degrees() const;

	void set_normalmap_generator_override(Ref<VoxelGenerator> generator_override);
	Ref<VoxelGenerator> get_normalmap_generator_override() const;

	void set_normalmap_generator_override_begin_lod_index(int lod_index);
	int get_normalmap_generator_override_begin_lod_index() const;

	void set_normalmap_use_gpu(bool enabled);
	bool get_normalmap_use_gpu() const;

	// These must be called after an edit
	void post_edit_area(Box3i p_box);
	void post_edit_modifiers(Box3i p_voxel_box);

	// TODO This still sucks atm cuz the edit will still run on the main thread
	void push_async_edit(IThreadedTask *task, Box3i box, std::shared_ptr<AsyncDependencyTracker> tracker);
	void abort_async_edits();

	void set_voxel_bounds(Box3i p_box);

	inline Box3i get_voxel_bounds() const {
		ZN_ASSERT(_data != nullptr);
		return _data->get_bounds();
	}

	void set_collision_update_delay(int delay_msec);
	int get_collision_update_delay() const;

	void set_lod_fade_duration(float seconds);
	float get_lod_fade_duration() const;

	enum ProcessCallback { //
		PROCESS_CALLBACK_IDLE = 0,
		PROCESS_CALLBACK_PHYSICS,
		PROCESS_CALLBACK_DISABLED
	};

	// This was originally added to fix a problem with rigidbody teleportation and floating world origin:
	// The player teleported at a different rate than the rest of the world due to delays in transform updates,
	// which caused the world to unload and then reload entirely over the course of 3 frames,
	// producing flickers and CPU lag. Changing process mode allows to align update rate,
	// and freeze LOD for the duration of the teleport.
	void set_process_callback(ProcessCallback mode);
	ProcessCallback get_process_callback() const {
		return _process_callback;
	}

	Ref<VoxelTool> get_voxel_tool() override;

	struct Stats {
		// Amount of octree nodes waiting for data. It should reach zero when everything is loaded.
		uint32_t blocked_lods = 0;
		// How many data blocks were rejected this frame (due to loading too late for example).
		uint32_t dropped_block_loads = 0;
		// How many mesh blocks were rejected this frame (due to loading too late for example).
		uint32_t dropped_block_meshs = 0;
		// Time spent in the last update unloading unused blocks and detecting required ones, in microseconds
		uint32_t time_detect_required_blocks = 0;
		// Time spent in the last update requesting data blocks, in microseconds
		uint32_t time_io_requests = 0;
		// Time spent in the last update requesting meshes, in microseconds
		uint32_t time_mesh_requests = 0;
		// Total time spent in the last update task, in microseconds.
		// This only includes the threadable part, not the whole `process` function.
		uint32_t time_update_task = 0;
	};

	const Stats &get_stats() const;

	void restart_stream() override;
	void remesh_all_blocks() override;

	// Debugging

	Array debug_raycast_mesh_block(Vector3 world_origin, Vector3 world_direction) const;
	Dictionary debug_get_data_block_info(Vector3 fbpos, int lod_index) const;
	Dictionary debug_get_mesh_block_info(Vector3 fbpos, int lod_index) const;
	Array debug_get_octree_positions() const;
	Array debug_get_octrees_detailed() const;

	enum DebugDrawFlag {
		DEBUG_DRAW_OCTREE_NODES = 0,
		DEBUG_DRAW_OCTREE_BOUNDS = 1,
		DEBUG_DRAW_MESH_UPDATES = 2,
		DEBUG_DRAW_EDIT_BOXES = 3,
		DEBUG_DRAW_VOLUME_BOUNDS = 4,
		DEBUG_DRAW_EDITED_BLOCKS = 5,

		DEBUG_DRAW_FLAGS_COUNT = 6
	};

	void debug_set_draw_enabled(bool enabled);
	bool debug_is_draw_enabled() const;

	void debug_set_draw_flag(DebugDrawFlag flag_index, bool enabled);
	bool debug_get_draw_flag(DebugDrawFlag flag_index) const;

	Node3D *debug_dump_as_nodes(bool include_instancer) const;

	// Editor

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &warnings) const override;
#endif // TOOLS_ENABLED

	// Internal

	void set_instancer(VoxelInstancer *instancer);
	VolumeID get_volume_id() const override {
		return _volume_id;
	}
	std::shared_ptr<StreamingDependency> get_streaming_dependency() const override {
		return _streaming_dependency;
	}

	Array get_mesh_block_surface(Vector3i block_pos, int lod_index) const;
	void get_meshed_block_positions_at_lod(int lod_index, std::vector<Vector3i> &out_positions) const;

	inline VoxelData &get_storage() const {
		ZN_ASSERT(_data != nullptr);
		return *_data;
	}

	inline std::shared_ptr<VoxelData> get_storage_shared() const {
		return _data;
	}

protected:
	void _notification(int p_what);

	void _on_gi_mode_changed() override;
	void _on_shadow_casting_changed() override;

private:
	void process(float delta);
	void apply_main_thread_update_tasks();

	void apply_mesh_update(VoxelEngine::BlockMeshOutput &ob);
	void apply_data_block_response(VoxelEngine::BlockDataOutput &ob);
	void apply_detail_texture_update(VoxelEngine::BlockDetailTextureOutput &ob);
	void apply_detail_texture_update_to_block(
			VoxelMeshBlockVLT &block, DetailTextureOutput &ob, unsigned int lod_index);
	void try_apply_parent_detail_texture_to_block(VoxelMeshBlockVLT &block, Vector3i bpos);

	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_maps();
	void reset_mesh_maps();

	Vector3 get_local_viewer_pos() const;
	void _set_lod_count(int p_lod_count);
	void set_mesh_block_active(VoxelMeshBlockVLT &block, bool active, bool with_fading);

	void _on_stream_params_changed();

	void update_shader_material_pool_template();

	void save_all_modified_blocks(bool with_copy);

	void process_deferred_collision_updates(uint32_t timeout_msec);
	void process_fading_blocks(float delta);

	struct LocalCameraInfo {
		Vector3 position;
		Vector3 forward;
	};

	LocalCameraInfo get_local_camera_info() const;

	void _b_save_modified_blocks();
	void _b_set_voxel_bounds(AABB aabb);
	AABB _b_get_voxel_bounds() const;

	Array _b_debug_print_sdf_top_down(Vector3i center, Vector3i extents);
	int _b_debug_get_mesh_block_count() const;
	int _b_debug_get_data_block_count() const;
	// TODO GDX: Can't bind functions returning a `godot::Error` enum
	int /*Error*/ _b_debug_dump_as_scene(String fpath, bool include_instancer) const;

	Dictionary _b_get_statistics() const;

#ifdef TOOLS_ENABLED
	void update_gizmos();
#endif

	static void _bind_methods();

private:
	friend class BuildTransitionMeshTask;

	VolumeID _volume_id;
	ProcessCallback _process_callback = PROCESS_CALLBACK_IDLE;

	Ref<Material> _material;

	// The main reason this pool even exists is because of this: https://github.com/godotengine/godot/issues/34741
	// Blocks need individual shader parameters for several features,
	// so a lot of ShaderMaterial copies using the same shader are created.
	// The terrain must be able to run in editor, but in that context, Godot connects a signal of Shader to
	// every ShaderMaterial using it. Godot does that in order to update properties in THE inspector if the shader
	// changes (which is debatable since only the edited material needs this, if it even is edited!).
	// The problem is, that also means every time `ShaderMaterial::duplicate()` is called, when it assigns `shader`,
	// it has to add a connection to a HUGE list. Which is very slow, enough to cause stutters.
	ShaderMaterialPoolVLT _shader_material_pool;

	FixedArray<VoxelMeshMap<VoxelMeshBlockVLT>, constants::MAX_LOD> _mesh_maps_per_lod;

	// Copies of meshes just for fading out.
	// Used when a transition mask changes. This can make holes appear if not smoothly faded.
	struct FadingOutMesh {
		// Position in space coordinates local to the volume
		Vector3 local_position;
		DirectMeshInstance mesh_instance;
		// Changing properties is the reason we may want to fade the mesh, so we may hold on a copy of the material with
		// properties before the fade starts.
		Ref<ShaderMaterial> shader_material;
		// Going from 1 to 0
		float progress;
	};

	// These are "fire and forget"
	std::vector<FadingOutMesh> _fading_out_meshes;

	unsigned int _collision_lod_count = 0;
	unsigned int _collision_layer = 1;
	unsigned int _collision_mask = 1;
	float _collision_margin = constants::DEFAULT_COLLISION_MARGIN;
	int _collision_update_delay = 0;
	FixedArray<std::vector<Vector3i>, constants::MAX_LOD> _deferred_collision_updates_per_lod;

	float _lod_fade_duration = 0.f;
	// Note, direct pointers to mesh blocks should be safe because these blocks are always destroyed from the same
	// thread that updates fading blocks. If a mesh block is destroyed, these maps should be updated at the same time.
	// TODO Optimization: use FlatMap? Need to check how many blocks get in there, probably not many
	FixedArray<std::map<Vector3i, VoxelMeshBlockVLT *>, constants::MAX_LOD> _fading_blocks_per_lod;

	struct FadingDetailTexture {
		Vector3i block_position;
		uint32_t lod_index;
		float progress;
	};

	std::vector<FadingDetailTexture> _fading_detail_textures;

	VoxelInstancer *_instancer = nullptr;

	Ref<VoxelMesher> _mesher;

	// Data stored with a shared pointer so it can be sent to asynchronous tasks
	bool _threaded_update_enabled = false;
	std::shared_ptr<VoxelData> _data;
	std::shared_ptr<VoxelLodTerrainUpdateData> _update_data;
	std::shared_ptr<StreamingDependency> _streaming_dependency;
	std::shared_ptr<MeshingDependency> _meshing_dependency;

	struct ApplyMeshUpdateTask : public ITimeSpreadTask {
		void run(TimeSpreadTaskContext &ctx) override;

		VolumeID volume_id;
		VoxelLodTerrain *self = nullptr;
		VoxelEngine::BlockMeshOutput data;
	};

	FixedArray<std::unordered_map<Vector3i, RefCount>, constants::MAX_LOD> _queued_main_thread_mesh_updates;

#ifdef TOOLS_ENABLED
	bool _debug_draw_enabled = false;
	uint8_t _debug_draw_flags = 0;
	uint8_t _edited_blocks_gizmos_lod_index = 0;

	DebugRenderer _debug_renderer;

	struct DebugMeshUpdateItem {
		static constexpr uint32_t LINGER_FRAMES = 10;
		Vector3i position;
		uint32_t lod;
		uint32_t remaining_frames;
	};

	std::vector<DebugMeshUpdateItem> _debug_mesh_update_items;

	struct DebugEditItem {
		static constexpr uint32_t LINGER_FRAMES = 10;
		Box3i voxel_box;
		uint32_t remaining_frames;
	};

	std::vector<DebugEditItem> _debug_edit_items;
#endif

	Stats _stats;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelLodTerrain::ProcessCallback)
VARIANT_ENUM_CAST(zylann::voxel::VoxelLodTerrain::DebugDrawFlag)

#endif // VOXEL_LOD_TERRAIN_HPP
