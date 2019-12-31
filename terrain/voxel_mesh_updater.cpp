#include "voxel_mesh_updater.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../util/utility.h"
#include "voxel_lod_terrain.h"
#include <core/os/os.h>

static void scale_mesh_data(Vector<Array> &surfaces, float factor) {

	for (int i = 0; i < surfaces.size(); ++i) {
		Array &surface = surfaces.write[i]; // There is COW here too but should not happen, hopefully

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
			this->process_blocks_thread_func(inputs, outputs, blocky_mesher, smooth_mesher);
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
		Ref<VoxelMesher> smooth_mesher) {

	CRASH_COND(inputs.size() != outputs.size());

	for (unsigned int i = 0; i < inputs.size(); ++i) {

		const InputBlock &ib = inputs[i];
		const InputBlockData &block = ib.data;
		OutputBlockData &output = outputs[i].data;

		CRASH_COND(block.voxels.is_null());

		if (blocky_mesher.is_valid()) {
			blocky_mesher->build(output.blocky_surfaces, **block.voxels);
		}
		if (smooth_mesher.is_valid()) {
			smooth_mesher->build(output.smooth_surfaces, **block.voxels);
		}

		if (ib.lod > 0) {
			// TODO Make this optional if the mesher can factor in the upscale already
			float factor = 1 << ib.lod;
			scale_mesh_data(output.blocky_surfaces.surfaces, factor);
			scale_mesh_data(output.smooth_surfaces.surfaces, factor);
			for (int i = 0; i < output.smooth_surfaces.transition_surfaces.size(); ++i) {
				scale_mesh_data(output.smooth_surfaces.transition_surfaces[i], factor);
			}
		}
	}
}
