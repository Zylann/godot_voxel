#include "voxel_mesh_updater.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../util/macros.h"
#include "../util/utility.h"
#include "voxel_lod_terrain.h"

VoxelMeshUpdater::VoxelMeshUpdater(unsigned int thread_count, MeshingParams params) {
	PRINT_VERBOSE("Constructing VoxelMeshUpdater");

	Ref<VoxelMesherBlocky> blocky_mesher;
	Ref<VoxelMesherTransvoxel> smooth_mesher;

	_minimum_padding = 0;
	_maximum_padding = 0;

	if (params.library.is_valid()) {
		blocky_mesher.instance();
		blocky_mesher->set_library(params.library);
		blocky_mesher->set_occlusion_enabled(params.baked_ao);
		blocky_mesher->set_occlusion_darkness(params.baked_ao_darkness);
		_minimum_padding = max(_minimum_padding, blocky_mesher->get_minimum_padding());
		_maximum_padding = max(_maximum_padding, blocky_mesher->get_maximum_padding());
	}

	if (params.smooth_surface) {
		smooth_mesher.instance();
		_minimum_padding = max(_minimum_padding, smooth_mesher->get_minimum_padding());
		_maximum_padding = max(_maximum_padding, smooth_mesher->get_maximum_padding());
	}

	FixedArray<Mgr::BlockProcessingFunc, VoxelConstants::MAX_LOD> processors;

	for (unsigned int i = 0; i < thread_count; ++i) {

		if (i > 0) {
			// Need to clone them because they are not thread-safe due to memory pooling.
			// Also thanks to the wonders of ref_pointer() being private we trigger extra refs/unrefs for no reason
			if (blocky_mesher.is_valid()) {
				blocky_mesher = Ref<VoxelMesher>(blocky_mesher->clone());
			}
			if (smooth_mesher.is_valid()) {
				smooth_mesher = Ref<VoxelMesher>(smooth_mesher->clone());
			}
		}

		processors[i] = [this, blocky_mesher, smooth_mesher](const ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Mgr::ProcessorStats &_) {
			this->process_blocks_thread_func(inputs, outputs, blocky_mesher, smooth_mesher);
		};
	}

	_mgr = memnew(Mgr(thread_count, 50, processors));
}

VoxelMeshUpdater::~VoxelMeshUpdater() {
	PRINT_VERBOSE("Destroying VoxelMeshUpdater");
	if (_mgr) {
		memdelete(_mgr);
	}
}

void VoxelMeshUpdater::process_blocks_thread_func(
		const ArraySlice<InputBlock> inputs,
		ArraySlice<OutputBlock> outputs,
		Ref<VoxelMesher> blocky_mesher,
		Ref<VoxelMesher> smooth_mesher) {

	CRASH_COND(inputs.size() != outputs.size());

	for (unsigned int i = 0; i < inputs.size(); ++i) {

		const InputBlock &ib = inputs[i];
		const InputBlockData &block = ib.data;
		OutputBlockData &output = outputs[i].data;

		CRASH_COND(block.voxels.is_null());

		VoxelMesher::Input input = { **block.voxels, ib.lod };

		if (blocky_mesher.is_valid()) {
			blocky_mesher->build(output.blocky_surfaces, input);
		}
		if (smooth_mesher.is_valid()) {
			smooth_mesher->build(output.smooth_surfaces, input);
		}
	}
}
