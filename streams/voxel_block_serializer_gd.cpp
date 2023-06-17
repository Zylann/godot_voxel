#include "voxel_block_serializer_gd.h"
#include "../util/godot/classes/stream_peer.h"
#include "voxel_block_serializer.h"

namespace zylann::voxel::gd {

int VoxelBlockSerializer::serialize_to_stream_peer(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress) {
	ERR_FAIL_COND_V(voxel_buffer.is_null(), 0);
	ERR_FAIL_COND_V(peer.is_null(), 0);

	if (compress) {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(voxel_buffer->get_buffer());
		ERR_FAIL_COND_V(!res.success, -1);
		stream_peer_put_data(**peer, to_span(res.data));
		return res.data.size();

	} else {
		BlockSerializer::SerializeResult res = BlockSerializer::serialize(voxel_buffer->get_buffer());
		ERR_FAIL_COND_V(!res.success, -1);
		stream_peer_put_data(**peer, to_span(res.data));
		return res.data.size();
	}
}

void VoxelBlockSerializer::deserialize_from_stream_peer(
		Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress) {
	ERR_FAIL_COND(voxel_buffer.is_null());
	ERR_FAIL_COND(peer.is_null());
	ERR_FAIL_COND(size <= 0);

	if (decompress) {
		std::vector<uint8_t> &compressed_data = BlockSerializer::get_tls_compressed_data();
		compressed_data.resize(size);
		const Error err = stream_peer_get_data(**peer, to_span(compressed_data));
		ERR_FAIL_COND(err != OK);
		const bool success =
				BlockSerializer::decompress_and_deserialize(to_span(compressed_data), voxel_buffer->get_buffer());
		ERR_FAIL_COND(!success);

	} else {
		std::vector<uint8_t> &data = BlockSerializer::get_tls_data();
		data.resize(size);
		const Error err = stream_peer_get_data(**peer, to_span(data));
		ERR_FAIL_COND(err != OK);
		BlockSerializer::deserialize(to_span(data), voxel_buffer->get_buffer());
	}
}

PackedByteArray VoxelBlockSerializer::serialize_to_byte_array(Ref<VoxelBuffer> voxel_buffer, bool compress) {
	ERR_FAIL_COND_V(voxel_buffer.is_null(), PackedByteArray());

	PackedByteArray bytes;
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

void VoxelBlockSerializer::deserialize_from_byte_array(
		PackedByteArray bytes, Ref<VoxelBuffer> voxel_buffer, bool decompress) {
	ERR_FAIL_COND(voxel_buffer.is_null());
	ERR_FAIL_COND(bytes.size() == 0);

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
	// temporary array every time.
	ClassDB::bind_static_method(cname, D_METHOD("serialize_to_stream_peer", "peer", "voxel_buffer", "compress"),
			&VoxelBlockSerializer::serialize_to_stream_peer);
	ClassDB::bind_static_method(cname,
			D_METHOD("deserialize_from_stream_peer", "peer", "voxel_buffer", "size", "decompress"),
			&VoxelBlockSerializer::deserialize_from_stream_peer);

	ClassDB::bind_static_method(cname, D_METHOD("serialize_to_byte_array", "voxel_buffer", "compress"),
			&VoxelBlockSerializer::serialize_to_byte_array);
	ClassDB::bind_static_method(cname, D_METHOD("deserialize_from_byte_array", "bytes", "voxel_buffer", "decompress"),
			&VoxelBlockSerializer::deserialize_from_byte_array);
}

} // namespace zylann::voxel::gd
