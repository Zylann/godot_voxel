#include "voxel_data_loader.h"
#include "../streams/voxel_stream.h"
#include "../util/utility.h"

VoxelDataLoader::VoxelDataLoader(unsigned int thread_count, Ref<VoxelStream> stream, unsigned int block_size_pow2) {

	print_line("Constructing VoxelDataLoader");
	CRASH_COND(stream.is_null());

	// TODO I'm not sure it's worth to configure more than one thread for voxel streams

	Mgr::BlockProcessingFunc processors[Mgr::MAX_JOBS];

	processors[0] = [this, stream](ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Mgr::ProcessorStats &stats) {
		this->process_blocks_thread_func(inputs, outputs, stream, stats);
	};

	if (thread_count > 1) {
		if (stream->is_thread_safe()) {

			for (unsigned int i = 1; i < thread_count; ++i) {
				processors[i] = processors[0];
			}

		} else if (stream->is_cloneable()) {

			// Note: more than one thread can make sense for generators,
			// but won't be as useful for file and network streams
			for (unsigned int i = 1; i < thread_count; ++i) {
				stream = stream->duplicate();
				processors[i] = [this, stream](ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Mgr::ProcessorStats &stats) {
					this->process_blocks_thread_func(inputs, outputs, stream, stats);
				};
			}

		} else {
			ERR_PRINT("Thread count set to higher than 1, but the stream is neither thread-safe nor cloneable. Capping back to 1 thread.");
			thread_count = 1;
		}
	}

	int batch_count = 128;
	int sync_interval_ms = 500;

	_block_size_pow2 = block_size_pow2;
	_mgr = memnew(Mgr(thread_count, sync_interval_ms, processors, true, batch_count));
}

VoxelDataLoader::~VoxelDataLoader() {
	print_line("Destroying VoxelDataLoader");
	if (_mgr) {
		memdelete(_mgr);
	}
}

// Can run in multiple threads
void VoxelDataLoader::process_blocks_thread_func(const ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Ref<VoxelStream> stream, Mgr::ProcessorStats &stats) {

	CRASH_COND(inputs.size() != outputs.size());

	Vector<VoxelStream::BlockRequest> emerge_requests;
	Vector<VoxelStream::BlockRequest> immerge_requests;

	for (size_t i = 0; i < inputs.size(); ++i) {

		const InputBlock &ib = inputs[i];

		int bs = 1 << _block_size_pow2;
		Vector3i block_origin_in_voxels = ib.position * (bs << ib.lod);

		if (ib.data.voxels_to_save.is_null()) {

			VoxelStream::BlockRequest r;
			r.voxel_buffer.instance();
			r.voxel_buffer->create(bs, bs, bs);
			r.origin_in_voxels = block_origin_in_voxels;
			r.lod = ib.lod;
			emerge_requests.push_back(r);

		} else {

			VoxelStream::BlockRequest r;
			r.voxel_buffer = ib.data.voxels_to_save;
			r.origin_in_voxels = block_origin_in_voxels;
			r.lod = ib.lod;
			immerge_requests.push_back(r);
		}
	}

	stream->emerge_blocks(emerge_requests);
	stream->immerge_blocks(immerge_requests);

	VoxelStream::Stats stream_stats = stream->get_statistics();
	stats.file_openings = stream_stats.file_openings;
	stats.time_spent_opening_files = stream_stats.time_spent_opening_files;

	// Assumes the stream won't change output order
	int iload = 0;
	for (size_t i = 0; i < outputs.size(); ++i) {

		const InputBlock &ib = inputs[i];
		OutputBlockData &output = outputs[i].data;

		if (ib.data.voxels_to_save.is_null()) {
			output.type = TYPE_LOAD;
			output.voxels_loaded = emerge_requests.write[iload].voxel_buffer;
			++iload;

		} else {
			output.type = TYPE_SAVE;
		}
	}

	// If unordered responses were allowed
	//
	//	size_t j = 0;
	//	for (size_t i = 0; i < emerge_requests.size(); ++i) {
	//		VoxelStream::BlockRequest &r = emerge_requests.write[i];
	//		OutputBlock &ob = outputs[j];
	//		ob.position = r.origin_in_voxels >> (_block_size_pow2 + r.lod);
	//		ob.lod = r.lod;
	//		ob.data.type = TYPE_LOAD;
	//		ob.data.voxels_loaded = r.voxel_buffer;
	//		++j;
	//	}
	//	for (size_t i = 0; i < immerge_requests.size(); ++i) {
	//		VoxelStream::BlockRequest &r = immerge_requests.write[i];
	//		OutputBlock &ob = outputs[j];
	//		ob.position = r.origin_in_voxels >> (_block_size_pow2 + r.lod);
	//		ob.lod = r.lod;
	//		ob.data.type = TYPE_SAVE;
	//		++j;
	//	}
}
