#include "test_stream_sqlite.h"
#include "../../streams/sqlite/block_location.h"
#include "../../streams/sqlite/voxel_stream_sqlite.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/random_pcg.h"
#include "../../util/math/conv.h"
#include "../../util/math/vector3i.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string/format.h"
#include "../testing.h"

namespace zylann::voxel::tests {

namespace {
void test_voxel_stream_sqlite_basic(
		bool with_key_cache,
		VoxelStreamSQLite::CoordinateFormat coordinate_format,
		Vector3i block_position
) {
	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());

	const String database_path = test_dir.get_path().path_join("database.sqlite");

	VoxelBuffer vb1(VoxelBuffer::ALLOCATOR_DEFAULT);
	vb1.create(Vector3i(16, 16, 16));
	vb1.fill_area(1, Vector3i(5, 5, 5), Vector3i(10, 11, 12), 0);
	const Vector3i vb1_pos = block_position;

	{
		Ref<VoxelStreamSQLite> stream;
		stream.instantiate();
		stream->set_key_cache_enabled(with_key_cache);
		stream->set_database_path(database_path);
		stream->set_preferred_coordinate_format(coordinate_format);
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
	test_voxel_stream_sqlite_basic(
			false, VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16, Vector3i(1, 2, -3)
	);
	test_voxel_stream_sqlite_basic(
			true, VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16, Vector3i(1, 2, -3)
	);
	test_voxel_stream_sqlite_basic(
			false, VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7, Vector3i(1, 2, -3)
	);
	test_voxel_stream_sqlite_basic(true, VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7, Vector3i(1, 2, -3));
	test_voxel_stream_sqlite_basic(false, VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD, Vector3i(1, 2, -3));
	test_voxel_stream_sqlite_basic(true, VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD, Vector3i(1, 2, -3));
	test_voxel_stream_sqlite_basic(
			false, VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5, Vector3i(1, 2, -3)
	);
	test_voxel_stream_sqlite_basic(
			true, VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5, Vector3i(1, 2, -3)
	);

	// Extras with large coordinates
	test_voxel_stream_sqlite_basic(
			false, VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7, Vector3i(100'000, 150'000, -200'000)
	);
	test_voxel_stream_sqlite_basic(
			false, VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD, Vector3i(10'000'000, -20'000'000, 30'000'000)
	);
	test_voxel_stream_sqlite_basic(
			false,
			VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5,
			Vector3i(10'000'000, -11'000'000, 12'000'000)
	);
}

void test_voxel_stream_sqlite_coordinate_format(const VoxelStreamSQLite::CoordinateFormat coordinate_format) {
	ZN_PROFILE_SCOPE();

	struct BlockInfo {
		Vector3i position;
		uint8_t lod_index;
		unsigned int id;

		static bool contains_location(const std::vector<BlockInfo> &blocks, Vector3i bpos, uint8_t lod_index) {
			for (const BlockInfo &bi : blocks) {
				if (bi.position == bpos && bi.lod_index == lod_index) {
					return true;
				}
			}
			return false;
		}
	};

	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());

	const String database_path = test_dir.get_path().path_join("database.sqlite");

	// Generate random locations
	RandomPCG rng;
	rng.seed(131183);
	std::vector<BlockInfo> blocks;
	blocks.resize(256);
	const int radius = 10000;
	// TODO Generate clusters/lines instead, to match what saves look like in practice?
	for (unsigned int i = 0; i < blocks.size(); ++i) {
		for (int attempt = 0; attempt < 10; ++attempt) {
			const uint8_t lod_index = rng.rand() % constants::MAX_LOD;
			const BlockInfo bi{ math::wrap(
										Vector3i(rng.rand(), rng.rand(), rng.rand()), Vector3iUtil::create(2 * radius)
								) - Vector3iUtil::create(radius),
								lod_index,
								i };
			if (!BlockInfo::contains_location(blocks, bi.position, bi.lod_index)) {
				blocks[i] = bi;
				break;
			}
		}
	}

	// Create a database populated with a lot of blocks
	{
		Ref<VoxelStreamSQLite> stream;
		stream.instantiate();
		stream->set_preferred_coordinate_format(coordinate_format);
		stream->set_database_path(database_path);

		ProfilingClock pclock;

		for (unsigned int i = 0; i < blocks.size(); ++i) {
			const BlockInfo block = blocks[i];
			VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);
			vb.create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));

			// The bottom layer will be filled by the index of the block
			vb.fill(i, 0);
			// A random amount of layers above will be random
			const int h = rng.rand() % (vb.get_size().y - 1) + 1;
			Vector3i rpos;
			for (rpos.z = 0; rpos.z < vb.get_size().z; ++rpos.z) {
				for (rpos.x = 0; rpos.x < vb.get_size().x; ++rpos.x) {
					for (rpos.y = 1; rpos.y < h; ++rpos.y) {
						const int rv = rng.rand() % 256;
						vb.set_voxel(rv, rpos, 0);
					}
				}
			}

			VoxelStreamSQLite::VoxelQueryData q{ vb, block.position, block.lod_index, VoxelStreamSQLite::RESULT_ERROR };
			stream->save_voxel_block(q);
		}

		stream->flush();

		const uint64_t elapsed_us = pclock.get_elapsed_microseconds();
		ZN_PRINT_VERBOSE(format("Writes time with coordinate format {}: {} us", coordinate_format, elapsed_us));
	}

	// Roughly shuffle locations
	for (unsigned int i = 0; i < blocks.size(); ++i) {
		const unsigned int j = rng.rand() % blocks.size();
		const BlockInfo &temp = blocks[i];
		blocks[i] = blocks[j];
		blocks[j] = temp;
	}

	// Reopen and read them all
	{
		Ref<VoxelStreamSQLite> stream;
		stream.instantiate();
		stream->set_key_cache_enabled(true);
		stream->set_database_path(database_path);

		VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);

		ZN_PROFILE_SCOPE();
		ProfilingClock pclock;

		for (unsigned int i = 0; i < blocks.size(); ++i) {
			const BlockInfo block = blocks[i];
			VoxelStreamSQLite::VoxelQueryData q{ vb, block.position, block.lod_index, VoxelStreamSQLite::RESULT_ERROR };
			{
				ZN_PROFILE_SCOPE();
				stream->load_voxel_block(q);
			}
			ZN_TEST_ASSERT(q.result == VoxelStreamSQLite::RESULT_BLOCK_FOUND);

			const unsigned int v = vb.get_voxel(Vector3i(vb.get_size().x / 2, 0, vb.get_size().z / 2), 0);
			ZN_TEST_ASSERT(v == block.id);
		}

		const uint64_t elapsed_us = pclock.get_elapsed_microseconds();
		ZN_PRINT_VERBOSE(format("Reads time with coordinate format {}: {} us", coordinate_format, elapsed_us));
	}
}

void test_voxel_stream_sqlite_coordinate_format() {
	test_voxel_stream_sqlite_coordinate_format(VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16);
	test_voxel_stream_sqlite_coordinate_format(VoxelStreamSQLite::COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7);
	test_voxel_stream_sqlite_coordinate_format(VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD);
	test_voxel_stream_sqlite_coordinate_format(VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5);
}

void test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i pos, uint8_t lod_index, std::string_view expected) {
	using namespace sqlite;

	FixedArray<uint8_t, STRING_LOCATION_MAX_LENGTH> buffer;
	const BlockLocation loc{ pos, lod_index };
	std::string_view sv = loc.encode_string_csd(buffer);
	ZN_ASSERT(sv == expected);
	BlockLocation loc2;
	ZN_ASSERT(loc.decode_string_csd(sv, loc2));
	ZN_ASSERT(loc == loc2);
}

void test_voxel_stream_sqlite_key_string_csd_encoding() {
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(0, 0, 0), 0, "0,0,0,0");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(1, 0, 0), 1, "1,0,0,1");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(-1, 4, -1), 2, "-1,4,-1,2");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(6, -9, 21), 5, "6,-9,21,5");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(123, -456, 789), 20, "123,-456,789,20");
}

void test_voxel_stream_sqlite_key_blob80_encoding(Vector3i position, uint8_t lod_index) {
	using namespace sqlite;

	FixedArray<uint8_t, BLOB80_LENGTH> buffer;
	const BlockLocation loc{ position, lod_index };
	loc.encode_blob80(to_span(buffer));
	const BlockLocation loc2 = BlockLocation::decode_blob80(to_span(buffer));
	ZN_ASSERT(loc == loc2);
}

void test_voxel_stream_sqlite_key_blob80_encoding() {
	using namespace sqlite;

	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(0, 0, 0), 0);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(1, 0, 0), 1);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(-1, 4, -1), 2);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(6, -9, 21), 5);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(123, -456, 789), 20);

	const BlockLocation::CoordinateFormat format = BlockLocation::FORMAT_BLOB80_X25_Y25_Z25_L5;
	const Box3i limits = BlockLocation::get_coordinate_range(format);
	const uint8_t max_lod_index = BlockLocation::get_lod_count(format) - 1;
	const Vector3i min_pos = limits.position;
	const Vector3i max_pos = limits.position + limits.size - Vector3i(1, 1, 1);
	test_voxel_stream_sqlite_key_blob80_encoding(min_pos, max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(max_pos, max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(min_pos.x, max_pos.y, min_pos.z), max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(max_pos.x, min_pos.y, max_pos.z), max_lod_index);
}

} // namespace zylann::voxel::tests
