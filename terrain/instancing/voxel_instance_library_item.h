#ifndef VOXEL_INSTANCE_LIBRARY_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_ITEM_H

#include <core/resource.h>
#include <scene/resources/mesh.h>
#include <scene/resources/shape.h>

#include "../../util/fixed_array.h"
#include "voxel_instance_generator.h"

// Settings for a model that can be used by VoxelInstancer
class VoxelInstanceLibraryItem : public Resource {
	GDCLASS(VoxelInstanceLibraryItem, Resource)
public:
	static const int MAX_MESH_LODS = 4;

	struct CollisionShapeInfo {
		Transform transform;
		Ref<Shape> shape;
	};

	enum ChangeType {
		CHANGE_LOD_INDEX,
		CHANGE_GENERATOR,
		CHANGE_VISUAL,
		CHANGE_ADDED,
		CHANGE_REMOVED
	};

	class IListener {
	public:
		virtual void on_library_item_changed(int id, ChangeType change) = 0;
	};

	void set_item_name(String name);
	String get_item_name() const;

	void set_lod_index(int lod);
	int get_lod_index() const;

	void set_generator(Ref<VoxelInstanceGenerator> generator);
	Ref<VoxelInstanceGenerator> get_generator() const;

	void set_persistent(bool persistent);
	bool is_persistent() const;

	void set_mesh(Ref<Mesh> mesh, int mesh_lod_index);
	Ref<Mesh> get_mesh(int mesh_lod_index) const;
	int get_mesh_lod_count() const;

	void set_material_override(Ref<Material> material);
	Ref<Material> get_material_override() const;

	void set_cast_shadows_setting(VisualServer::ShadowCastingSetting mode);
	VisualServer::ShadowCastingSetting get_cast_shadows_setting() const;

	void set_collision_layer(int collision_layer);
	int get_collision_layer() const;

	void set_collision_mask(int collision_mask);
	int get_collision_mask() const;

	void setup_from_template(Node *root);

	// Internal

	inline const Vector<CollisionShapeInfo> &get_collision_shapes() const {
		return _collision_shapes;
	}

	void add_listener(IListener *listener, int id);
	void remove_listener(IListener *listener, int id);

private:
	void notify_listeners(ChangeType change);
	void _on_generator_changed();

	static void _bind_methods();

	void _b_set_collision_shapes(Array shape_infos);
	Array _b_get_collision_shapes() const;

	Ref<Mesh> _b_get_mesh_lod0() const { return get_mesh(0); }
	Ref<Mesh> _b_get_mesh_lod1() const { return get_mesh(1); }
	Ref<Mesh> _b_get_mesh_lod2() const { return get_mesh(2); }
	Ref<Mesh> _b_get_mesh_lod3() const { return get_mesh(3); }

	void _b_set_mesh_lod0(Ref<Mesh> mesh) { set_mesh(mesh, 0); }
	void _b_set_mesh_lod1(Ref<Mesh> mesh) { set_mesh(mesh, 1); }
	void _b_set_mesh_lod2(Ref<Mesh> mesh) { set_mesh(mesh, 2); }
	void _b_set_mesh_lod3(Ref<Mesh> mesh) { set_mesh(mesh, 3); }

	// For the user, not used by the engine
	String _name;

	// If a layer is persistent, any change to its instances will be saved if the volume has a stream
	// supporting instances. It will also not generate on top of modified surfaces.
	// If a layer is not persistent, changes won't get saved, and it will keep generating on all compliant
	// surfaces.
	bool _persistent = false;

	// Which LOD of the octree this model will spawn into.
	// Higher means larger distances, but lower precision and density
	int _lod_index = 0;

	Ref<VoxelInstanceGenerator> _generator;

	FixedArray<Ref<Mesh>, MAX_MESH_LODS> _mesh_lods;
	unsigned int _mesh_lod_count = 1;

	// TODO Scenes?

	// It is preferred to have materials on the mesh already,
	// but this is in case OBJ meshes are used, which often dont have a material of their own
	Ref<Material> _material_override;

	VisualServer::ShadowCastingSetting _shadow_casting_setting = VisualServer::SHADOW_CASTING_SETTING_ON;

	int _collision_mask = 1;
	int _collision_layer = 1;
	Vector<CollisionShapeInfo> _collision_shapes;

	struct ListenerSlot {
		IListener *listener;
		int id;

		inline bool operator==(const ListenerSlot &other) const {
			return listener == other.listener && id == other.id;
		}
	};

	Vector<ListenerSlot> _listeners;
};

#endif // VOXEL_INSTANCE_LIBRARY_ITEM_H
