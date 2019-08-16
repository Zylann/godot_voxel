#ifndef VOXEL_MESH_UPDATER_H
#define VOXEL_MESH_UPDATER_H

#include <core/os/semaphore.h>
#include <core/os/thread.h>
#include <core/vector.h>

#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../meshers/dmc/voxel_mesher_dmc.h"
#include "../voxel_buffer.h"

#include "block_thread_manager.h"

class VoxelMeshUpdater {
public:
	struct InputBlockData {
		Ref<VoxelBuffer> voxels;
	};

	struct OutputBlockData {
		VoxelMesher::Output blocky_surfaces;
		VoxelMesher::Output smooth_surfaces;
	};

	struct MeshingParams {
		Ref<VoxelLibrary> library;
		bool baked_ao = true;
		float baked_ao_darkness = 0.75;
		bool smooth_surface = false;
	};

	typedef VoxelBlockThreadManager<InputBlockData, OutputBlockData> Mgr;
	typedef Mgr::InputBlock InputBlock;
	typedef Mgr::OutputBlock OutputBlock;
	typedef Mgr::Input Input;
	typedef Mgr::Output Output;
	typedef Mgr::Stats Stats;

	VoxelMeshUpdater(unsigned int thread_count, MeshingParams params);
	~VoxelMeshUpdater();

	void push(const Input &input) { _mgr->push(input); }
	void pop(Output &output) { _mgr->pop(output); }

	int get_required_padding() const { return _required_padding; }

private:
	void process_blocks_thread_func(const ArraySlice<InputBlock> inputs,
			ArraySlice<OutputBlock> outputs,
			Ref<VoxelMesher> blocky_mesher,
			Ref<VoxelMesher> smooth_mesher,
			int padding);

	Mgr *_mgr = nullptr;
	int _required_padding = 0;
};

#endif // VOXEL_MESH_UPDATER_H
