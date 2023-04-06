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

int VoxelBlockSerializer::_b_serialize_to_stream_peer(
		Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress) {
	ERR_FAIL_COND_V(voxel_buffer.is_null(), 0);
	ERR_FAIL_COND_V(peer.is_null(), 0);
	return serialize(**peer, voxel_buffer->get_buffer(), compress);
}

void VoxelBlockSerializer::_b_deserialize_from_stream_peer(
		Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress) {
	ERR_FAIL_COND(voxel_buffer.is_null());
	ERR_FAIL_COND(peer.is_null());
	ERR_FAIL_COND(size <= 0);
	deserialize(**peer, voxel_buffer->get_buffer(), size, decompress);
}

PackedByteArray VoxelBlockSerializer::_b_serialize_to_byte_array(Ref<VoxelBuffer> voxel_buffer, bool compress) {
	PackedByteArray bytes;
	ERR_FAIL_COND_V(voxel_buffer.is_null(), PackedByteArray());
	if (compress) {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(voxel_buffer->get_buffer());
		ERR_FAIL_COND_V(!res.success, PackedByteArray());
		copy_to(bytes, to_span(res.data));

	} else {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize(voxel_buffer->get_buffer());
		ERR_FAIL_COND_V(!res.success, PackedByteArray());
		copy_to(bytes, to_span(res.data));
	}
	return bytes;
}

void VoxelBlockSerializer::_b_deserialize_from_byte_array(
		PackedByteArray bytes, Ref<VoxelBuffer> voxel_buffer, bool decompress) {
	Span<const uint8_t> bytes_span = Span<const uint8_t>(bytes.ptr(), bytes.size());

	if (decompress) {
		const bool success = BlockSerializer::decompress_and_deserialize(bytes_span, voxel_buffer->get_buffer());
		ERR_FAIL_COND(!success);

	} else {
		BlockSerializer::deserialize(bytes_span, voxel_buffer->get_buffer());
	}
}

void VoxelBlockSerializer::_bind_methods() {
	auto cname = VoxelBlockSerializer::get_class_static();

	// Reasons for using methods with StreamPeer:
	// - Convenience, if you do write to a peer already
	// - Avoiding an allocation. When serializing to a PackedByteArray, the Godot API incurs allocating that
	// temporary array everytime.
	ClassDB::bind_static_method(cname, D_METHOD("serialize_to_stream_peer", "peer", "voxel_buffer", "compress"),
			&VoxelBlockSerializer::_b_serialize_to_stream_peer);
	ClassDB::bind_static_method(cname,
			D_METHOD("deserialize_from_stream_peer", "peer", "voxel_buffer", "size", "decompress"),
			&VoxelBlockSerializer::_b_deserialize_from_stream_peer);

	ClassDB::bind_static_method(cname, D_METHOD("serialize_to_byte_array", "voxel_buffer", "compress"),
			&VoxelBlockSerializer::_b_serialize_to_byte_array);
	ClassDB::bind_static_method(cname, D_METHOD("deserialize_from_byte_array", "bytes", "voxel_buffer", "decompress"),
			&VoxelBlockSerializer::_b_deserialize_from_byte_array);
}

} // namespace zylann::voxel::gd
