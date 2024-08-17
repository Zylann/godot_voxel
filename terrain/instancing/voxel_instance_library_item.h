#ifndef VOXEL_INSTANCE_LIBRARY_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_ITEM_H

#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_vector.h"
#include "instance_library_item_listener.h"
#include "voxel_instance_generator.h"

namespace zylann::voxel {

class VoxelInstanceLibraryItem : public Resource {
	GDCLASS(VoxelInstanceLibraryItem, Resource)
public:
	void set_item_name(String p_name);
	String get_item_name() const;

	void set_lod_index(int lod);
	int get_lod_index() const;

	void set_generator(Ref<VoxelInstanceGenerator> generator);
	Ref<VoxelInstanceGenerator> get_generator() const;

	void set_persistent(bool persistent);
	bool is_persistent() const;

	// Internal

	void add_listener(IInstanceLibraryItemListener *listener, int id);
	void remove_listener(IInstanceLibraryItemListener *listener, int id);

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

protected:
	void notify_listeners(IInstanceLibraryItemListener::ChangeType change);

private:
	void _on_generator_changed();

	static void _bind_methods();

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

	struct ListenerSlot {
		IInstanceLibraryItemListener *listener;
		int id;

		inline bool operator==(const ListenerSlot &other) const {
			return listener == other.listener && id == other.id;
		}
	};

	StdVector<ListenerSlot> _listeners;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_ITEM_H
