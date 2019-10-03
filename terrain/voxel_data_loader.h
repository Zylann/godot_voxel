#ifndef VOXEL_DATA_LOADER_H
#define VOXEL_DATA_LOADER_H

#include "block_thread_manager.h"

class VoxelStream;
class VoxelBuffer;

class VoxelDataLoader {
public:
	struct InputBlockData {
		Ref<VoxelBuffer> voxels_to_save;
	};

	enum RequestType {
		TYPE_SAVE = 0,
		TYPE_LOAD
	};

	struct OutputBlockData {
		RequestType type;
		Ref<VoxelBuffer> voxels_loaded;
	};

	typedef VoxelBlockThreadManager<InputBlockData, OutputBlockData> Mgr;
	typedef Mgr::InputBlock InputBlock;
	typedef Mgr::OutputBlock OutputBlock;
	typedef Mgr::Input Input;
	typedef Mgr::Output Output;
	typedef Mgr::Stats Stats;

	VoxelDataLoader(unsigned int thread_count, Ref<VoxelStream> stream, unsigned int block_size_pow2);
	~VoxelDataLoader();

	void push(const Input &input) { _mgr->push(input); }
	void pop(Output &output) { _mgr->pop(output); }

private:
	void process_blocks_thread_func(const ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Ref<VoxelStream> stream, Mgr::ProcessorStats &stats);

	Mgr *_mgr = nullptr;
	int _block_size_pow2 = 0;
};

#endif // VOXEL_DATA_LOADER_H
