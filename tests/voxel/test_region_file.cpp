#include "test_region_file.h"
#include "../../streams/region/region_file.h"
#include "../../streams/region/voxel_stream_region_files.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/godot/core/random_pcg.h"
#include "../../util/string/format.h"
#include "../../util/testing/test_directory.h"
#include "../../util/testing/test_macros.h"
#include "test_util.h"

namespace zylann::voxel::tests {

void test_region_file() {
	const int block_size_po2 = 4;
	const int block_size = 1 << block_size_po2;
	const char *region_file_name = "test_region_file.vxr";
	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());
	String region_file_path = test_dir.get_path().path_join(region_file_name);

	struct RandomBlockGenerator {
		RandomPCG rng;

		RandomBlockGenerator() {
			rng.seed(131183);
		}

		void generate(VoxelBuffer &buffer) {
			buffer.create(Vector3iUtil::create(block_size));
			const unsigned int channel_index = 0;
			buffer.set_channel_depth(channel_index, VoxelBuffer::DEPTH_16_BIT);

			const float r = rng.randf();

			if (r < 0.2f) {
				// Every so often, make a uniform block
				buffer.clear_channel(channel_index, rng.rand() % 256);

			} else if (r < 0.4f) {
				// Every so often, make a semi-uniform block
				buffer.clear_channel(channel_index, rng.rand() % 256);
				const int ymax = rng.rand() % buffer.get_size().y;
				for (int z = 0; z < buffer.get_size().z; ++z) {
					for (int x = 0; x < buffer.get_size().x; ++x) {
						for (int y = 0; y < ymax; ++y) {
							buffer.set_voxel(rng.rand() % 256, x, y, z, channel_index);
						}
					}
				}

			} else {
				// Make a block with enough data to take some significant space even if compressed
				for (int z = 0; z < buffer.get_size().z; ++z) {
					for (int x = 0; x < buffer.get_size().x; ++x) {
						for (int y = 0; y < buffer.get_size().y; ++y) {
							buffer.set_voxel(rng.rand() % 256, x, y, z, channel_index);
						}
					}
				}
			}
		}
	};

	RandomBlockGenerator generator;

	// Create a block of voxels
	VoxelBuffer voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	generator.generate(voxel_buffer);

	{
		RegionFile region_file;

		// Configure region format
		RegionFormat region_format = region_file.get_format();
		region_format.block_size_po2 = block_size_po2;
		for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {
			region_format.channel_depths[channel_index] = voxel_buffer.get_channel_depth(channel_index);
		}
		ZN_TEST_ASSERT(region_file.set_format(region_format));

		// Open file
		const Error open_error = region_file.open(region_file_path, true);
		ZN_TEST_ASSERT(open_error == OK);

		// Save block
		const Error save_error =
				region_file.save_block(Vector3i(1, 2, 3), voxel_buffer, CompressedData::COMPRESSION_LZ4);
		ZN_TEST_ASSERT(save_error == OK);

		// Read back
		VoxelBuffer loaded_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
		const Error load_error = region_file.load_block(Vector3i(1, 2, 3), loaded_voxel_buffer);
		ZN_TEST_ASSERT(load_error == OK);

		// Must be equal
		ZN_TEST_ASSERT(voxel_buffer.equals(loaded_voxel_buffer));
	}
	// Load again but using a new region file object
	{
		RegionFile region_file;

		// Open file
		const Error open_error = region_file.open(region_file_path, false);
		ZN_TEST_ASSERT(open_error == OK);

		// Read back
		VoxelBuffer loaded_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
		const Error load_error = region_file.load_block(Vector3i(1, 2, 3), loaded_voxel_buffer);
		ZN_TEST_ASSERT(load_error == OK);

		// Must be equal
		ZN_TEST_ASSERT(voxel_buffer.equals(loaded_voxel_buffer));
	}
	// Save many blocks
	{
		RegionFile region_file;

		// Open file
		const Error open_error = region_file.open(region_file_path, false);
		ZN_TEST_ASSERT(open_error == OK);

		RandomPCG rng;

		struct Chunk {
			VoxelBuffer voxels;
			Chunk() : voxels(VoxelBuffer::ALLOCATOR_DEFAULT) {}
		};

		StdUnorderedMap<Vector3i, Chunk> buffers;
		const Vector3i region_size = region_file.get_format().region_size;

		for (int i = 0; i < 1000; ++i) {
			const Vector3i pos = Vector3i( //
					rng.rand() % uint32_t(region_size.x), //
					rng.rand() % uint32_t(region_size.y), //
					rng.rand() % uint32_t(region_size.z) //
			);
			generator.generate(voxel_buffer);

			// Save block
			const Error save_error = region_file.save_block(pos, voxel_buffer, CompressedData::COMPRESSION_LZ4);
			ZN_TEST_ASSERT(save_error == OK);

			// Note, the same position can occur twice, we just overwrite
			buffers[pos].voxels = std::move(voxel_buffer);
		}

		// Read back
		for (auto it = buffers.begin(); it != buffers.end(); ++it) {
			VoxelBuffer loaded_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
			const Error load_error = region_file.load_block(it->first, loaded_voxel_buffer);
			ZN_TEST_ASSERT(load_error == OK);
			ZN_TEST_ASSERT(it->second.voxels.equals(loaded_voxel_buffer));
		}

		const Error close_error = region_file.close();
		ZN_TEST_ASSERT(close_error == OK);

		// Open file
		const Error open_error2 = region_file.open(region_file_path, false);
		ZN_TEST_ASSERT(open_error2 == OK);

		// Read back again
		for (auto it = buffers.begin(); it != buffers.end(); ++it) {
			VoxelBuffer loaded_voxel_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
			const Error load_error = region_file.load_block(it->first, loaded_voxel_buffer);
			ZN_TEST_ASSERT(load_error == OK);
			ZN_TEST_ASSERT(it->second.voxels.equals(loaded_voxel_buffer));
		}
	}
}

// Test based on an issue from `I am the Carl` on Discord. It should only not crash or cause errors.
void test_voxel_stream_region_files() {
	const int block_size_po2 = 4;
	const int block_size = 1 << block_size_po2;

	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());

	Ref<VoxelStreamRegionFiles> stream;
	stream.instantiate();
	stream->set_block_size_po2(block_size_po2);
	stream->set_directory(test_dir.get_path());

	RandomPCG rng;

	for (int cycle = 0; cycle < 1000; ++cycle) {
		VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
		buffer.create(block_size, block_size, block_size);

		// Make a block with enough data to take some significant space even if compressed
		for (int z = 0; z < buffer.get_size().z; ++z) {
			for (int x = 0; x < buffer.get_size().x; ++x) {
				for (int y = 0; y < buffer.get_size().y; ++y) {
					buffer.set_voxel(rng.rand() % 256, x, y, z, 0);
				}
			}
		}

		// Dividing coordinate so it saves multiple times the same block. That should not crash.
		VoxelStream::VoxelQueryData q{ buffer, Vector3(cycle / 16, 0, 0), 0, VoxelStream::RESULT_ERROR };
		stream->save_voxel_block(q);
	}
}

void test_voxel_stream_region_files_lods() {
	const uint32_t block_size_po2 = 4;
	const uint32_t block_size = 1 << block_size_po2;
	const uint8_t lod_count = 3;
	const uint32_t cycles = 1000;
	const Vector3i center_blocks = Vector3i();
	const uint32_t radius_blocks = 8;
	const uint32_t seed = 131183;

	zylann::testing::TestDirectory test_dir;
	ZN_TEST_ASSERT(test_dir.is_valid());

	Ref<VoxelStreamRegionFiles> stream;
	stream.instantiate();
	stream->set_block_size_po2(block_size_po2);
	stream->set_directory(test_dir.get_path());

	struct L {
		static int randi(RandomPCG &rng, const int min, const int max) {
			return min + static_cast<int>(rng.rand() % (max - min));
		}
		static Vector3i get_random_position(RandomPCG &rng, const Vector3i center, const uint32_t radius) {
			return Vector3i(
					randi(rng, center.x - radius, center.x + radius),
					randi(rng, center.y - radius, center.y + radius),
					randi(rng, center.z - radius, center.z + radius)
			);
		}
		static void generate_block(RandomPCG &rng, VoxelBuffer &buffer, uint32_t index) {
			// Make a block with enough data to take some significant space even if compressed
			for (int z = 0; z < buffer.get_size().z; ++z) {
				for (int x = 0; x < buffer.get_size().x; ++x) {
					for (int y = 0; y < buffer.get_size().y; ++y) {
						buffer.set_voxel(rng.rand() % 10000, x, y, z, 0);
						// buffer.set_voxel((x + y + z + index) % 10000, x, y, z, 0);
					}
				}
			}
		}
	};

	struct SavedBlock {
		VoxelBuffer voxels;
		Vector3i pos;
		uint8_t lod;
		bool overwritten = false;
	};

	StdVector<SavedBlock> saved_blocks;

	{
		RandomPCG rng;
		rng.seed(seed);

		uint32_t index = 0;
		for (uint8_t lod_index = 0; lod_index < lod_count; ++lod_index) {
			for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
				VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
				buffer.create(block_size, block_size, block_size);
				L::generate_block(rng, buffer, index);

				const Vector3i block_pos = L::get_random_position(rng, center_blocks, radius_blocks);

				VoxelStream::VoxelQueryData q{ buffer, block_pos, lod_index, VoxelStream::RESULT_ERROR };
				stream->save_voxel_block(q);

				for (SavedBlock &b : saved_blocks) {
					if (b.overwritten) {
						continue;
					}
					if (b.lod == lod_index && b.pos == block_pos) {
						b.overwritten = true;
						break;
					}
				}

				saved_blocks.push_back({ std::move(buffer), block_pos, lod_index, false });
				index += 1;
			}
		}
	}
	{
		for (uint32_t index = 0; index < saved_blocks.size(); ++index) {
			const SavedBlock &expected_block = saved_blocks[index];
			if (expected_block.overwritten) {
				continue;
			}

			VoxelBuffer found_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
			found_buffer.create(block_size, block_size, block_size);

			VoxelStream::VoxelQueryData q{
				found_buffer, expected_block.pos, expected_block.lod, VoxelStream::RESULT_ERROR
			};
			stream->load_voxel_block(q);

			ZN_TEST_ASSERT(q.result == VoxelStream::RESULT_BLOCK_FOUND);
			if (!q.voxel_buffer.equals(expected_block.voxels)) {
				for (unsigned int i = 0; i < saved_blocks.size(); ++i) {
					if (i == index) {
						continue;
					}
					const SavedBlock &b = saved_blocks[i];
					if (b.voxels.equals(q.voxel_buffer)) {
						ZN_PRINT_VERBOSE(
								format("Loaded block #{} at {} lod {} found unequal, but is equal to #{} at {} lod {} "
									   "overwritten: {}",
									   index,
									   expected_block.pos,
									   static_cast<int>(expected_block.lod),
									   i,
									   b.pos,
									   static_cast<int>(b.lod),
									   b.overwritten)
						);
					}
				}
				print_channel_as_ascii(q.voxel_buffer, 0, 4);
			}

			ZN_TEST_ASSERT(q.voxel_buffer.equals(expected_block.voxels));
		}
	}
}

} // namespace zylann::voxel::tests
