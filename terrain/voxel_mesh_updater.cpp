#include "voxel_mesh_updater.h"
#include "../util/utility.h"
#include "voxel_lod_terrain.h"
#include <core/os/os.h>

static void scale_mesh_data(VoxelMesher::Output &data, float factor) {

	for (int i = 0; i < data.surfaces.size(); ++i) {
		Array &surface = data.surfaces.write[i]; // There is COW here too but should not happen, hopefully

		if (surface.empty()) {
			continue;
		}

		PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX]; // Array of Variants here, implicit cast going on

		// Now dear COW, let's make sure there is only ONE ref to that PoolVector,
		// so you won't trash performance with pointless allocations
		surface[Mesh::ARRAY_VERTEX] = Variant();

		{
			PoolVector3Array::Write w = positions.write();
			int len = positions.size();
			for (int j = 0; j < len; ++j) {
				w[j] = w[j] * factor;
			}
		}

		// Thank you
		surface[Mesh::ARRAY_VERTEX] = positions;
	}
}

VoxelMeshUpdater::VoxelMeshUpdater(unsigned int thread_count, MeshingParams params) {

	print_line("Constructing VoxelMeshUpdater");

	Ref<VoxelMesherBlocky> blocky_mesher;
	Ref<VoxelMesherDMC> smooth_mesher;

	_required_padding = 0;

	if (params.library.is_valid()) {
		blocky_mesher.instance();
		blocky_mesher->set_library(params.library);
		blocky_mesher->set_occlusion_enabled(params.baked_ao);
		blocky_mesher->set_occlusion_darkness(params.baked_ao_darkness);
		_required_padding = max(_required_padding, blocky_mesher->get_minimum_padding());
	}

	if (params.smooth_surface) {
		smooth_mesher.instance();
		smooth_mesher->set_geometric_error(0.05);
		smooth_mesher->set_simplify_mode(VoxelMesherDMC::SIMPLIFY_NONE);
		smooth_mesher->set_seam_mode(VoxelMesherDMC::SEAM_MARCHING_SQUARE_SKIRTS);
		_required_padding = max(_required_padding, smooth_mesher->get_minimum_padding());
	}

	Mgr::BlockProcessingFunc processors[Mgr::MAX_LOD];

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
			this->process_blocks_thread_func(inputs, outputs, blocky_mesher, smooth_mesher, this->_required_padding);
		};
	}

	_mgr = memnew(Mgr(thread_count, 50, processors));
}

VoxelMeshUpdater::~VoxelMeshUpdater() {
	print_line("Destroying VoxelMeshUpdater");
	if (_mgr) {
		memdelete(_mgr);
	}
}

void VoxelMeshUpdater::process_blocks_thread_func(
		const ArraySlice<InputBlock> inputs,
		ArraySlice<OutputBlock> outputs,
		Ref<VoxelMesher> blocky_mesher,
		Ref<VoxelMesher> smooth_mesher,
		int padding) {

	CRASH_COND(inputs.size() != outputs.size());

	for (unsigned int i = 0; i < inputs.size(); ++i) {

		const InputBlock &ib = inputs[i];
		const InputBlockData &block = ib.data;
		OutputBlockData &output = outputs[i].data;

		CRASH_COND(block.voxels.is_null());

		if (blocky_mesher.is_valid()) {
			blocky_mesher->build(output.blocky_surfaces, **block.voxels, padding);
		}
		if (smooth_mesher.is_valid()) {
			smooth_mesher->build(output.smooth_surfaces, **block.voxels, padding);
		}

		if (ib.lod > 0) {
			float factor = 1 << ib.lod;
			scale_mesh_data(output.blocky_surfaces, factor);
			scale_mesh_data(output.smooth_surfaces, factor);
		}
	}
}
