#ifndef VOXEL_BLOCK_SERIALIZER_GD_H
#define VOXEL_BLOCK_SERIALIZER_GD_H

#include "../storage/voxel_buffer_gd.h"
#include "voxel_block_serializer.h"
#include <core/io/stream_peer.h>

namespace zylann::voxel::gd {

class VoxelBuffer;

// Godot-facing API for BlockSerializer
// TODO Could be a singleton? Or methods on VoxelBuffer? This object has no state.
class VoxelBlockSerializer : public RefCounted {
	GDCLASS(VoxelBlockSerializer, RefCounted)
public:
	int serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress) {
		ERR_FAIL_COND_V(voxel_buffer.is_null(), 0);
		ERR_FAIL_COND_V(peer.is_null(), 0);
		return BlockSerializer::serialize(**peer, voxel_buffer->get_buffer(), compress);
	}

	void deserialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress) {
		ERR_FAIL_COND(voxel_buffer.is_null());
		ERR_FAIL_COND(peer.is_null());
		ERR_FAIL_COND(size <= 0);
		BlockSerializer::deserialize(**peer, voxel_buffer->get_buffer(), size, decompress);
	}

private:
	static void _bind_methods() {
		ClassDB::bind_method(
				D_METHOD("serialize", "peer", "voxel_buffer", "compress"), &VoxelBlockSerializer::serialize);
		ClassDB::bind_method(D_METHOD("deserialize", "peer", "voxel_buffer", "size", "decompress"),
				&VoxelBlockSerializer::deserialize);
	}
};

} // namespace zylann::voxel::gd

#endif // VOXEL_BLOCK_SERIALIZER_GD_H
