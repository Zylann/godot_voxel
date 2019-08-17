#ifndef VOXEL_BLOCK_SERIALIZER_H
#define VOXEL_BLOCK_SERIALIZER_H

#include <core/io/file_access_memory.h>
#include <vector>

class VoxelBuffer;

class VoxelBlockSerializer {
public:
	const std::vector<uint8_t> &serialize(VoxelBuffer &voxel_buffer);
	bool deserialize(const std::vector<uint8_t> &p_data, VoxelBuffer &out_voxel_buffer);

	const std::vector<uint8_t> &serialize_and_compress(VoxelBuffer &voxel_buffer);
	bool decompress_and_deserialize(const std::vector<uint8_t> &p_data, VoxelBuffer &out_voxel_buffer);
	bool decompress_and_deserialize(FileAccess *f, unsigned int size_to_read, VoxelBuffer &out_voxel_buffer);

private:
	unsigned int get_size_in_bytes(const VoxelBuffer &buffer);

	std::vector<uint8_t> _data;
	std::vector<uint8_t> _compressed_data;
	FileAccessMemory _file_access_memory;
};

#endif // VOXEL_BLOCK_SERIALIZER_H
