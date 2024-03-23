#include "test_block_serializer.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../streams/voxel_block_serializer.h"
#include "../../streams/voxel_block_serializer_gd.h"
#include "../../util/godot/classes/stream_peer_buffer.h"
#include "../testing.h"

namespace zylann::voxel::tests {

void test_block_serializer() {
	// Create an example buffer
	const Vector3i block_size(8, 9, 10);
	VoxelBuffer voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	voxel_buffer.create(block_size);
	voxel_buffer.fill_area(42, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 0);
	voxel_buffer.fill_area(43, Vector3i(2, 3, 4), Vector3i(6, 6, 6), 0);
	voxel_buffer.fill_area(44, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 1);

	{
		// Serialize without compression wrapper
		BlockSerializer::SerializeResult result = BlockSerializer::serialize(voxel_buffer);
		ZN_TEST_ASSERT(result.success);
		StdVector<uint8_t> data = result.data;

		ZN_TEST_ASSERT(data.size() > 0);
		ZN_TEST_ASSERT(data[0] == BlockSerializer::BLOCK_FORMAT_VERSION);

		// Deserialize
		VoxelBuffer deserialized_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
		ZN_TEST_ASSERT(BlockSerializer::deserialize(to_span_const(data), deserialized_voxel_buffer));

		// Must be equal
		ZN_TEST_ASSERT(voxel_buffer.equals(deserialized_voxel_buffer));
	}
	{
		// Serialize
		BlockSerializer::SerializeResult result = BlockSerializer::serialize_and_compress(voxel_buffer);
		ZN_TEST_ASSERT(result.success);
		StdVector<uint8_t> data = result.data;

		ZN_TEST_ASSERT(data.size() > 0);

		// Deserialize
		VoxelBuffer deserialized_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
		ZN_TEST_ASSERT(BlockSerializer::decompress_and_deserialize(to_span_const(data), deserialized_voxel_buffer));

		// Must be equal
		ZN_TEST_ASSERT(voxel_buffer.equals(deserialized_voxel_buffer));
	}
}

void test_block_serializer_stream_peer() {
	// Create an example buffer
	const Vector3i block_size(8, 9, 10);
	Ref<godot::VoxelBuffer> voxel_buffer;
	voxel_buffer.instantiate();
	voxel_buffer->create(block_size.x, block_size.y, block_size.z);
	voxel_buffer->fill_area(42, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 0);
	voxel_buffer->fill_area(43, Vector3i(2, 3, 4), Vector3i(6, 6, 6), 0);
	voxel_buffer->fill_area(44, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 1);

	Ref<StreamPeerBuffer> peer;
	peer.instantiate();
	// peer->clear();

	const int size = godot::VoxelBlockSerializer::serialize_to_stream_peer(peer, voxel_buffer, true);

	PackedByteArray data_array = peer->get_data_array();

	// Client

	Ref<godot::VoxelBuffer> voxel_buffer2;
	voxel_buffer2.instantiate();

	Ref<StreamPeerBuffer> peer2;
	peer2.instantiate();
	peer2->set_data_array(data_array);

	godot::VoxelBlockSerializer::deserialize_from_stream_peer(peer2, voxel_buffer2, size, true);

	ZN_TEST_ASSERT(voxel_buffer2->get_buffer().equals(voxel_buffer->get_buffer()));
}

} // namespace zylann::voxel::tests
