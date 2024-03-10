#ifndef VOXEL_INSTANCE_MODEL_LIBRARY_H
#define VOXEL_INSTANCE_MODEL_LIBRARY_H

#include "../../constants/voxel_constants.h"
#include "../../util/containers/std_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/core/string_name.h"
#include "../../util/thread/mutex.h"
#include "voxel_instance_library_item.h"

namespace zylann::voxel {

// Contains a list of items that can be used by VoxelInstancer, associated with a unique ID
class VoxelInstanceLibrary : public Resource, public VoxelInstanceLibraryItem::IListener {
	GDCLASS(VoxelInstanceLibrary, Resource)

public:
	static const int MAX_ID = 0xffff;

	class IListener {
	public:
		virtual ~IListener() {}
		virtual void on_library_item_changed(int id, VoxelInstanceLibraryItem::ChangeType change) = 0;
	};

	~VoxelInstanceLibrary();

	int get_next_available_id();
	void add_item(int p_id, Ref<VoxelInstanceLibraryItem> item);
	void remove_item(int p_id);
	void clear();
	int find_item_by_name(String p_name) const;
	int get_item_count() const;

	// Internal

	const VoxelInstanceLibraryItem *get_item_const(int id) const;
	VoxelInstanceLibraryItem *get_item(int id);

	// f(int item_id, VoxelInstanceLibraryItem &item)
	template <typename F>
	void for_each_item(F f) {
		for (auto it = _items.begin(); it != _items.end(); ++it) {
			ZN_ASSERT(it->second.is_valid());
			f(it->first, **it->second);
		}
	}

	void add_listener(IListener *listener);
	void remove_listener(IListener *listener);

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

	struct PackedItem {
		Ref<VoxelInstanceGenerator> generator;
		unsigned int id;
	};

	void get_packed_items_at_lod(StdVector<PackedItem> &out_items, unsigned int lod_index) const;

protected:
	void set_item(int id, Ref<VoxelInstanceLibraryItem> item);
	void update_packed_items();

	Ref<VoxelInstanceLibraryItem> _b_get_item(int id) const;
	PackedInt32Array _b_get_all_item_ids() const;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

private:
	void on_library_item_changed(int id, VoxelInstanceLibraryItem::ChangeType change) override;
	void notify_listeners(int item_id, VoxelInstanceLibraryItem::ChangeType change);

	static void _bind_methods();

	// ID => Item
	// Using a map keeps items ordered, so the last item has highest ID
	StdMap<int, Ref<VoxelInstanceLibraryItem>> _items;

	StdVector<IListener *> _listeners;

	struct PackedItems {
		struct Lod {
			StdVector<PackedItem> items;
		};
		FixedArray<Lod, constants::MAX_LOD> lods;
		mutable Mutex mutex;
		std::atomic_bool needs_update = false;
	};

	// Packed representation of items for use in procedural generation tasks
	PackedItems _packed_items;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_LIBRARY_H
