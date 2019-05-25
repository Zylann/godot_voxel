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

	Ref<VoxelMesherBlocky> blocky_mesher;
	Ref<VoxelMesherDMC> smooth_mesher;

	if (params.library.is_valid()) {
		blocky_mesher.instance();
		blocky_mesher->set_library(params.library);
		blocky_mesher->set_occlusion_enabled(params.baked_ao);
		blocky_mesher->set_occlusion_darkness(params.baked_ao_darkness);
	}

	if (params.smooth_surface) {
		smooth_mesher.instance();
		smooth_mesher->set_geometric_error(0.05);
		smooth_mesher->set_simplify_mode(VoxelMesherDMC::SIMPLIFY_NONE);
		smooth_mesher->set_seam_mode(VoxelMesherDMC::SEAM_MARCHING_SQUARE_SKIRTS);
	}

	Processor processors[Mgr::MAX_LOD];

	for (int i = 0; i < thread_count; ++i) {
		Processor &p = processors[i];
		if (i == 0) {
			p.blocky_mesher = blocky_mesher;
			p.smooth_mesher = smooth_mesher;
			_required_padding = p.get_required_padding();
		} else {
			// Need to clone them because they are not thread-safe.
			// Also thanks to the wonders of ref_pointer() being private we trigger extra refs/unrefs for no reason
			if (blocky_mesher.is_valid()) {
				p.blocky_mesher = Ref<VoxelMesher>(blocky_mesher->clone());
			}
			if (smooth_mesher.is_valid()) {
				p.smooth_mesher = Ref<VoxelMesher>(smooth_mesher->clone());
			}
		}
	}

	_mgr = memnew(Mgr(thread_count, 50, processors));
}

VoxelMeshUpdater::~VoxelMeshUpdater() {
	if (_mgr) {
		memdelete(_mgr);
	}
}

int VoxelMeshUpdater::Processor::get_required_padding() {
	int padding = 0;
	if (blocky_mesher.is_valid()) {
		padding = max(padding, blocky_mesher->get_minimum_padding());
	}
	if (smooth_mesher.is_valid()) {
		padding = max(padding, smooth_mesher->get_minimum_padding());
	}
	return padding;
}

void VoxelMeshUpdater::Processor::process_block(const InputBlockData &input, OutputBlockData &output, Vector3i block_position, unsigned int lod) {

	const InputBlockData &block = input;
	CRASH_COND(block.voxels.is_null());

	int padding = get_required_padding();

	if (blocky_mesher.is_valid()) {
		blocky_mesher->build(output.blocky_surfaces, **block.voxels, padding);
	}
	if (smooth_mesher.is_valid()) {
		smooth_mesher->build(output.smooth_surfaces, **block.voxels, padding);
	}

	if (lod > 0) {
		float factor = 1 << lod;
		scale_mesh_data(output.blocky_surfaces, factor);
		scale_mesh_data(output.smooth_surfaces, factor);
	}
}
