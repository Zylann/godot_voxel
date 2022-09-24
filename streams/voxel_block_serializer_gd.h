#ifndef VOXEL_BLOCK_SERIALIZER_GD_H
#define VOXEL_BLOCK_SERIALIZER_GD_H

#include "../storage/voxel_buffer_gd.h"

ZN_GODOT_FORWARD_DECLARE(class StreamPeer);

namespace zylann::voxel::gd {

class VoxelBuffer;

// Godot-facing API for BlockSerializer
// TODO Could be a singleton? Or methods on VoxelBuffer? This object has no state.
class VoxelBlockSerializer : public RefCounted {
	GDCLASS(VoxelBlockSerializer, RefCounted)
public:
	static int serialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, bool compress);
	static void deserialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, int size, bool decompress);

	int serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress);
	void deserialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress);

private:
	int _b_serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress);
	void _b_deserialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress);

	static void _bind_methods();
};

} // namespace zylann::voxel::gd

#endif // VOXEL_BLOCK_SERIALIZER_GD_H
