#ifndef VOXEL_INSTANCE_EMITTER_LISTENER_H
#define VOXEL_INSTANCE_EMITTER_LISTENER_H

#include "../../util/godot/classes/ref_counted.h"
#include <cstdint>

namespace zylann::voxel {

class VoxelInstanceEmitter;
class VoxelInstanceLibraryItem;

class IInstanceEmitterListener {
public:
	virtual ~IInstanceEmitterListener() {}

	virtual void on_emitter_item_added(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) = 0;
	virtual void on_emitter_item_removed(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) = 0;

	virtual void on_emitter_lod_index_changed(
			VoxelInstanceEmitter *emitter,
			const uint8_t previous_lod_index,
			const uint8_t new_lod_index
	) = 0;

	virtual void on_emitter_generator_changed(VoxelInstanceEmitter *emitter) = 0;

	virtual void on_emitter_array_assigned() = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_EMITTER_LISTENER_H
