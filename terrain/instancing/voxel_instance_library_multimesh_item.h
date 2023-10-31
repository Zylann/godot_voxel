#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H

#include "../../util/containers/span.h"
#include "../../util/godot/classes/geometry_instance_3d.h"
#include "../../util/godot/classes/material.h"
#include "../../util/godot/classes/mesh.h"
#include "../../util/godot/classes/packed_scene.h"
#include "../../util/godot/classes/rendering_server.h"
#include "../../util/godot/classes/shape_3d.h"
#include "voxel_instance_library_item.h"

namespace zylann::voxel {

// Settings for a model that can be used by VoxelInstancer
class VoxelInstanceLibraryMultiMeshItem : public VoxelInstanceLibraryItem {
	GDCLASS(VoxelInstanceLibraryMultiMeshItem, VoxelInstanceLibraryItem)
public:
	static const int MAX_MESH_LODS = 4;
	static const char *MANUAL_SETTINGS_GROUP_NAME;
	static const char *SCENE_SETTINGS_GROUP_NAME;

	static constexpr float MIN_DISTANCE_RATIO = 0.f;
	// Can be higher than 1 because when used with VoxelTerrain it is based on the half-extents of the visible area,
	// which is square, so the circular area covered by mesh lods can actually extend a bit further if desired.
	static constexpr float MAX_DISTANCE_RATIO = 2.f;

	struct CollisionShapeInfo {
		Transform3D transform;
		Ref<Shape3D> shape;
	};

	VoxelInstanceLibraryMultiMeshItem();

	void set_mesh(Ref<Mesh> mesh, int mesh_lod_index);
	Ref<Mesh> get_mesh(int mesh_lod_index) const;

	void set_mesh_lod_distance_ratio(int mesh_lod_index, float ratio);
	float get_mesh_lod_distance_ratio(int mesh_lod_index) const;

	int get_mesh_lod_count() const;

	void set_render_layer(int render_layer);
	int get_render_layer() const;

	void set_material_override(Ref<Material> material);
	Ref<Material> get_material_override() const;

	void set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode);
	RenderingServer::ShadowCastingSetting get_cast_shadows_setting() const;

	void set_gi_mode(GeometryInstance3D::GIMode mode);
	GeometryInstance3D::GIMode get_gi_mode() const;

	void set_collision_layer(int collision_layer);
	int get_collision_layer() const;

	void set_collision_mask(int collision_mask);
	int get_collision_mask() const;

	void set_collider_group_names(TypedArray<StringName> names);
	TypedArray<StringName> get_collider_group_names() const;

	// TODO GDX: it seems binding a method taking a `Node*` fails to compile. It is supposed to be working.
#if defined(ZN_GODOT)
	void setup_from_template(Node *root);
#elif defined(ZN_GODOT_EXTENSION)
	void setup_from_template(Object *root_o);
#endif

	void set_scene(Ref<PackedScene> scene);
	Ref<PackedScene> get_scene() const;

	bool get_hide_beyond_max_lod() const;
	void set_hide_beyond_max_lod(bool enabled);

	// Internal

	struct Settings {
		FixedArray<Ref<Mesh>, MAX_MESH_LODS> mesh_lods;
		unsigned int mesh_lod_count = 1;
		int render_layer = 1;

		// It is preferred to have materials on the mesh already,
		// but this is in case OBJ meshes are used, which often dont have a material of their own
		Ref<Material> material_override;

		RenderingServer::ShadowCastingSetting shadow_casting_setting = RenderingServer::SHADOW_CASTING_SETTING_ON;
		GeometryInstance3D::GIMode gi_mode = GeometryInstance3D::GIMode::GI_MODE_STATIC;

		int collision_mask = 1;
		int collision_layer = 1;
		std::vector<CollisionShapeInfo> collision_shapes;
		// Groups that will be added to colliders if they use nodes
		std::vector<StringName> group_names;
	};

	// If a scene is assigned to the item, returns settings converted from it.
	// If no scene is assigned, returns manual settings.
	const Settings &get_multimesh_settings() const;

	inline Span<const CollisionShapeInfo> get_collision_shapes() const {
		return to_span_const(get_multimesh_settings().collision_shapes);
	}

	inline Span<const float> get_mesh_lod_distance_ratios() const {
		return to_span(_mesh_lod_max_distance_ratios);
	}

	Array serialize_multimesh_item_properties() const;
	void deserialize_multimesh_item_properties(Array a);

private:
	// bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	void _b_set_collision_shapes(Array shape_infos);
	Array _b_get_collision_shapes() const;

	Ref<Mesh> _b_get_mesh_lod0() const {
		return get_mesh(0);
	}
	Ref<Mesh> _b_get_mesh_lod1() const {
		return get_mesh(1);
	}
	Ref<Mesh> _b_get_mesh_lod2() const {
		return get_mesh(2);
	}
	Ref<Mesh> _b_get_mesh_lod3() const {
		return get_mesh(3);
	}

	void _b_set_mesh_lod0(Ref<Mesh> mesh) {
		set_mesh(mesh, 0);
	}
	void _b_set_mesh_lod1(Ref<Mesh> mesh) {
		set_mesh(mesh, 1);
	}
	void _b_set_mesh_lod2(Ref<Mesh> mesh) {
		set_mesh(mesh, 2);
	}
	void _b_set_mesh_lod3(Ref<Mesh> mesh) {
		set_mesh(mesh, 3);
	}

	void _b_set_mesh_lod0_distance_ratio(float ratio) {
		set_mesh_lod_distance_ratio(0, ratio);
	}
	void _b_set_mesh_lod1_distance_ratio(float ratio) {
		set_mesh_lod_distance_ratio(1, ratio);
	}
	void _b_set_mesh_lod2_distance_ratio(float ratio) {
		set_mesh_lod_distance_ratio(2, ratio);
	}
	void _b_set_mesh_lod3_distance_ratio(float ratio) {
		set_mesh_lod_distance_ratio(3, ratio);
	}

	float _b_get_mesh_lod0_distance_ratio() const {
		return get_mesh_lod_distance_ratio(0);
	}
	float _b_get_mesh_lod1_distance_ratio() const {
		return get_mesh_lod_distance_ratio(1);
	}
	float _b_get_mesh_lod2_distance_ratio() const {
		return get_mesh_lod_distance_ratio(2);
	}
	float _b_get_mesh_lod3_distance_ratio() const {
		return get_mesh_lod_distance_ratio(3);
	}

	PackedFloat32Array _b_get_mesh_lod_distance_ratios() const;
	void _b_set_mesh_lod_distance_ratios(PackedFloat32Array ratios);

	// Settings manually set in the inspector, scripts, and saved to the resource file.
	Settings _manual_settings;
	// Settings gathered at runtime from the `_scene` property. They are not saved to the resource file. They take
	// precedence over manual settings.
	Settings _scene_settings;
	// If not null, will be converted and used at runtime instead of manual settings.
	// This alternative gives several benefits over manual settings:
	// - Better workflow to setup the model, using Godot's scene editor instead of a limited inspector
	// - Updates automatically if the scene changes
	// - Does not bloat the resource (unlike in-editor scene conversion) if the scene is embedding all its meshes and
	//   textures inside itself. This is often the case of imported scenes: conversion copies a reference to the mesh
	//   into manual settings, but when Godot saves the resource, it sees the mesh has no dedicated file, so it makes a
	//   copy of it and embeds it again in the resource.
	Ref<PackedScene> _scene;
	// This may be used if the terrain has no LOD or the item is on its last LOD
	bool _hide_beyond_max_lod = false;
	FixedArray<float, MAX_MESH_LODS> _mesh_lod_max_distance_ratios;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H
