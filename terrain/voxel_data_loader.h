#ifndef VOXEL_DATA_LOADER_H
#define VOXEL_DATA_LOADER_H

#include "block_thread_manager.h"

class VoxelProvider;
class VoxelBuffer;

class VoxelDataLoader {
public:
	struct InputBlockData {
		Ref<VoxelBuffer> voxels_to_save;
	};

	struct OutputBlockData {
		Ref<VoxelBuffer> voxels_loaded;
	};

	struct Processor {
		void process_block(const InputBlockData &input, OutputBlockData &output, Vector3i block_position, unsigned int lod);

		Ref<VoxelProvider> provider;
		int block_size_pow2 = 0;
	};

	typedef VoxelBlockThreadManager<InputBlockData, OutputBlockData, Processor> Mgr;
	typedef Mgr::InputBlock InputBlock;
	typedef Mgr::OutputBlock OutputBlock;
	typedef Mgr::Input Input;
	typedef Mgr::Output Output;
	typedef Mgr::Stats Stats;

	VoxelDataLoader(int thread_count, Ref<VoxelProvider> provider, int block_size_pow2);
	~VoxelDataLoader();

	void push(const Input &input) { _mgr->push(input); }
	void pop(Output &output) { _mgr->pop(output); }

private:
	Mgr *_mgr = nullptr;
};

#endif // VOXEL_DATA_LOADER_H
