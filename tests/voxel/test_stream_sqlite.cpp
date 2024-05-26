#include "test_stream_sqlite.h"
#include "../../streams/sqlite/voxel_stream_sqlite.h"
#include "../testing.h"

namespace zylann::voxel::tests {

namespace {
void test_voxel_stream_sqlite_basic(bool with_key_cache) {
	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());

	const String database_path = test_dir.get_path().path_join("database.sqlite");

	VoxelBuffer vb1(VoxelBuffer::ALLOCATOR_DEFAULT);
	vb1.create(Vector3i(16, 16, 16));
	vb1.fill_area(1, Vector3i(5, 5, 5), Vector3i(10, 11, 12), 0);
	const Vector3i vb1_pos(1, 2, -3);

	{
		Ref<VoxelStreamSQLite> stream;
		stream.instantiate();
		stream->set_key_cache_enabled(with_key_cache);
		stream->set_database_path(database_path);
		// Save block
		{
			VoxelStreamSQLite::VoxelQueryData q{ vb1, vb1_pos, 0, VoxelStream::RESULT_ERROR };
			stream->save_voxel_block(q);
			// Result is not set currently for saves...
			// ZN_TEST_ASSERT(q.result == VoxelStream::RESULT_BLOCK_FOUND);
		}
		// Load it back (caching might take effect)
		{
			VoxelBuffer loaded_vb1(VoxelBuffer::ALLOCATOR_DEFAULT);
			VoxelStreamSQLite::VoxelQueryData q{ loaded_vb1, vb1_pos, 0, VoxelStream::RESULT_ERROR };
			stream->load_voxel_block(q);
			ZN_TEST_ASSERT(q.result == VoxelStream::RESULT_BLOCK_FOUND);
			ZN_TEST_ASSERT(loaded_vb1.equals(vb1));
		}
		// Flush before the stream object is destroyed
		stream->flush();
	}
	{
		// Create new stream object and reopen the database (avoids caching effects).
		Ref<VoxelStreamSQLite> stream;
		stream.instantiate();
		stream->set_key_cache_enabled(with_key_cache);
		stream->set_database_path(database_path);
		{
			VoxelBuffer loaded_vb1(VoxelBuffer::ALLOCATOR_DEFAULT);
			VoxelStreamSQLite::VoxelQueryData q{ loaded_vb1, vb1_pos, 0, VoxelStream::RESULT_ERROR };
			stream->load_voxel_block(q);
			ZN_TEST_ASSERT(q.result == VoxelStream::RESULT_BLOCK_FOUND);
			ZN_TEST_ASSERT(loaded_vb1.equals(vb1));
		}
	}
}
} // namespace

void test_voxel_stream_sqlite_basic() {
	test_voxel_stream_sqlite_basic(false);
	test_voxel_stream_sqlite_basic(true);
}

} // namespace zylann::voxel::tests