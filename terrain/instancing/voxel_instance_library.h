#ifndef VOXEL_INSTANCE_MODEL_LIBRARY_H
#define VOXEL_INSTANCE_MODEL_LIBRARY_H

#include "../../constants/voxel_constants.h"
#include "../../util/containers/std_map.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/godot/core/string.h"
#include "../../util/hash_funcs.h"
#include "../../util/thread/mutex.h"
#include "instance_emitter_listener.h"
#include "instance_library_item_listener.h"
// Can't forward-declare because ~Ref<T> requires `unref` to be known from the destructor of StdVector...
#include "voxel_instance_generator.h"
// Can't forward-declare because ~Ref<T> requires `unref` to be known from the destructor of StdVector...
#include "voxel_instance_emitter.h"

namespace zylann::voxel {

class VoxelInstanceLibraryItem;
// class VoxelInstanceGenerator;
// class VoxelInstanceEmitter;
class IInstanceLibraryListener;

// Contains a list of emitters that can be used by VoxelInstancer.
// Every item referenced by those emitters is associated with a unique ID.
class VoxelInstanceLibrary : public Resource, public IInstanceEmitterListener, public IInstanceLibraryItemListener {
	GDCLASS(VoxelInstanceLibrary, Resource)

public:
	// This is a small limit to allow a few minor optimizations, and also because I can't think of a reason to have so
	// many emitters
	static const uint32_t MAX_EMITTERS = 64;

	static const int MAX_ITEM_ID = 0xffff;

	~VoxelInstanceLibrary();

	// Editor

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

	// Internal

	const VoxelInstanceLibraryItem *get_item_by_layer_id(const int id) const;
	int get_layer_id_from_emitter_and_item(const VoxelInstanceLibraryItem *item, const VoxelInstanceEmitter *emitter)
			const;

	template <typename F>
	void for_each_layer(F f) const {
		for (auto it = _layer_id_to_item.begin(); it != _layer_id_to_item.end(); ++it) {
			const int id = it->first;
			VoxelInstanceLibraryItem *item = it->second.ptr();
			ZN_ASSERT_CONTINUE(item != nullptr);
			f(id, item);
		}
	}

	template <typename F>
	void for_each_emitter(F f) const {
		for (const Ref<VoxelInstanceEmitter> &emitter : _emitters) {
			if (emitter.is_valid()) {
				f(**emitter);
			}
		}
	}

	void add_listener(IInstanceLibraryListener *listener);
	void remove_listener(IInstanceLibraryListener *listener);

	Span<const Ref<VoxelInstanceEmitter>> get_emitters() const;

	unsigned int get_registered_item_count() const;

	// Precomputed data accessible by threads
	struct PackedData {
		struct Lod {
			// No nulls
			StdVector<Ref<VoxelInstanceEmitter>> emitters;
			// Number of item slots per emitter
			StdVector<uint8_t> item_counts;
			// Concatenated layer IDs per item. Can have -1
			StdVector<int> layer_ids;

			void clear() {
				emitters.clear();
				item_counts.clear();
				layer_ids.clear();
			}
		};
		FixedArray<Lod, 8> lods;
		Mutex mutex;
	};

	// May only be used by the main thread
	void update_packed_data();
	bool is_packed_data_needing_update() const;

	const PackedData &get_packed_data() const;

private:
	int get_next_available_layer_id() const;
	void register_item(Ref<VoxelInstanceLibraryItem> item, const VoxelInstanceEmitter *emitter);
	void unregister_item(Ref<VoxelInstanceLibraryItem> item, const VoxelInstanceEmitter *emitter);

	void on_emitter_item_added(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) override;
	void on_emitter_item_removed(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) override;

	void on_emitter_lod_index_changed(
			VoxelInstanceEmitter *emitter,
			const uint8_t previous_lod_index,
			const uint8_t new_lod_index
	) override;

	void on_emitter_generator_changed(VoxelInstanceEmitter *emitter) override;
	void on_emitter_array_assigned() override;

	void on_library_item_changed(VoxelInstanceLibraryItem *item, ItemChangeType change) override;

	TypedArray<VoxelInstanceEmitter> _b_get_emitters() const;
	void _b_set_emitters(TypedArray<VoxelInstanceEmitter> emitter);

	static void _bind_methods();

	// Can have nulls.
	// Cannot have duplicates.
	StdVector<Ref<VoxelInstanceEmitter>> _emitters;

	// The instancer stores instances into "layers" for each LOD. Each layer contains instances of only one item. To
	// identify which layer corresponds to which item, and which emitters have to run, we register them with IDs.
	// Note 1: the same item used in different LODs will have different IDs.
	// Note 2: the same item can be shared by two emitters in the same LOD. As an optimization, we could use the same
	// layer for both emitters, But for simplicity, we create a different one. So each layer corresponds to one item,
	// and one emitter.

	StdMap<int, Ref<VoxelInstanceLibraryItem>> _layer_id_to_item;

	struct LayerKey {
		const VoxelInstanceLibraryItem *item;
		const VoxelInstanceEmitter *emitter;

		inline bool operator==(const LayerKey &other) const {
			return item == other.item && emitter == other.emitter;
		}
	};

	struct LayerKeyHasher {
		size_t operator()(const LayerKey &key) const {
			const uint64_t h1 = std::hash<const VoxelInstanceLibraryItem *>{}(key.item);
			const uint64_t h2 = std::hash<const VoxelInstanceEmitter *>{}(key.emitter);
			return hash_djb2_one_64(h1, h2);
		}
	};

	StdUnorderedMap<LayerKey, int, LayerKeyHasher> _item_to_layer_id;

	StdVector<IInstanceLibraryListener *> _listeners;

	PackedData _packed_data;
	bool _packed_data_need_update = false;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_LIBRARY_H
