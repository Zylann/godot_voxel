#include "voxel_block_serializer_gd.h"
#include "../util/godot/classes/stream_peer.h"
#include "voxel_block_serializer.h"

namespace zylann::voxel::gd {

int VoxelBlockSerializer::serialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, bool compress) {
	if (compress) {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(voxel_buffer);
		ERR_FAIL_COND_V(!res.success, -1);
		stream_peer_put_data(peer, to_span(res.data));
		return res.data.size();

	} else {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize(voxel_buffer);
		ERR_FAIL_COND_V(!res.success, -1);
		stream_peer_put_data(peer, to_span(res.data));
		return res.data.size();
	}
}

void VoxelBlockSerializer::deserialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, int size, bool decompress) {
	if (decompress) {
		std::vector<uint8_t> &compressed_data = BlockSerializer::get_tls_compressed_data();
		compressed_data.resize(size);
		const Error err = stream_peer_get_data(peer, to_span(compressed_data));
		ERR_FAIL_COND(err != OK);
		bool success = BlockSerializer::decompress_and_deserialize(to_span(compressed_data), voxel_buffer);
		ERR_FAIL_COND(!success);

	} else {
		std::vector<uint8_t> &data = BlockSerializer::get_tls_data();
		data.resize(size);
		const Error err = stream_peer_get_data(peer, to_span(data));
		ERR_FAIL_COND(err != OK);
		BlockSerializer::deserialize(to_span(data), voxel_buffer);
	}
}

int VoxelBlockSerializer::serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress) {
	ERR_FAIL_COND_V(voxel_buffer.is_null(), 0);
	ERR_FAIL_COND_V(peer.is_null(), 0);
	return serialize(**peer, voxel_buffer->get_buffer(), compress);
}

void VoxelBlockSerializer::deserialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress) {
	ERR_FAIL_COND(voxel_buffer.is_null());
	ERR_FAIL_COND(peer.is_null());
	ERR_FAIL_COND(size <= 0);
	deserialize(**peer, voxel_buffer->get_buffer(), size, decompress);
}

int VoxelBlockSerializer::_b_serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress) {
	return serialize(peer, voxel_buffer, compress);
}

void VoxelBlockSerializer::_b_deserialize(
		Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress) {
	deserialize(peer, voxel_buffer, size, decompress);
}

void VoxelBlockSerializer::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("serialize", "peer", "voxel_buffer", "compress"), &VoxelBlockSerializer::_b_serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "peer", "voxel_buffer", "size", "decompress"),
			&VoxelBlockSerializer::_b_deserialize);
}

} // namespace zylann::voxel::gd
