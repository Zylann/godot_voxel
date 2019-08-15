#include "voxel_data_loader.h"
#include "../streams/voxel_stream.h"
#include "../util/utility.h"

VoxelDataLoader::VoxelDataLoader(int thread_count, Ref<VoxelStream> stream, int block_size_pow2) {

	CRASH_COND(stream.is_null());

	Processor processors[Mgr::MAX_JOBS];

	for (unsigned int i = 0; i < Mgr::MAX_JOBS; ++i) {
		Processor &p = processors[i];
		p.block_size_pow2 = block_size_pow2;
	}

	if (stream->is_thread_safe()) {

		for (unsigned int i = 0; i < thread_count; ++i) {
			Processor &p = processors[i];
			p.stream = stream;
		}

	} else if (stream->is_cloneable()) {

		// Note: more than one thread can make sense for generators,
		// but won't be as useful for file and network streams
		for (int i = 0; i < thread_count; ++i) {
			Processor &p = processors[i];
			if (i == 0) {
				p.stream = stream;
			} else {
				p.stream = stream->duplicate();
			}
		}

	} else {

		if (thread_count > 1) {
			ERR_PRINT("Thread count set to higher than 1, but the stream is neither thread-safe nor cloneable. Capping back to 1 thread.");
			thread_count = 1;
		}
		processors[0].stream = stream;
	}

	_mgr = memnew(Mgr(thread_count, 500, processors, true));
}

VoxelDataLoader::~VoxelDataLoader() {
	print_line("Destroying VoxelDataLoader");
	if (_mgr) {
		memdelete(_mgr);
	}
}

void VoxelDataLoader::Processor::process_block(const InputBlockData &input, OutputBlockData &output, Vector3i block_position, unsigned int lod) {

	int bs = 1 << block_size_pow2;
	Vector3i block_origin_in_voxels = block_position * (bs << lod);

	if (input.voxels_to_save.is_null()) {

		Ref<VoxelBuffer> buffer;
		buffer.instance();
		buffer->create(bs, bs, bs);

		stream->emerge_block(buffer, block_origin_in_voxels, lod);
		output.type = TYPE_LOAD;
		output.voxels_loaded = buffer;

	} else {

		stream->immerge_block(input.voxels_to_save, block_origin_in_voxels, lod);
		output.type = TYPE_SAVE;
	}
}
