#include "voxel_mesh_updater.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../util/utility.h"
#include "voxel_lod_terrain.h"
#include <core/os/os.h>
#include <scene/resources/concave_polygon_shape.h>

// Faster version of Mesh::create_trimesh_shape()
// See https://github.com/Zylann/godot_voxel/issues/54
//
static Ref<ConcavePolygonShape> create_concave_polygon_shape(Array surface_arrays) {

	PoolVector<Vector3> positions = surface_arrays[Mesh::ARRAY_VERTEX];
	PoolVector<int> indices = surface_arrays[Mesh::ARRAY_INDEX];

	ERR_FAIL_COND_V(positions.size() < 3, Ref<ConcavePolygonShape>());
	ERR_FAIL_COND_V(indices.size() < 3, Ref<ConcavePolygonShape>());
	ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ConcavePolygonShape>());

	int face_points_count = indices.size();

	PoolVector<Vector3> face_points;
	face_points.resize(face_points_count);

	{
		PoolVector<Vector3>::Write w = face_points.write();
		PoolVector<int>::Read index_r = indices.read();
		PoolVector<Vector3>::Read position_r = positions.read();

		for (int i = 0; i < face_points_count; ++i) {
			w[i] = position_r[index_r[i]];
		}
	}

	Ref<ConcavePolygonShape> shape = memnew(ConcavePolygonShape);
	shape->set_faces(face_points);
	return shape;
}

VoxelMeshUpdater::VoxelMeshUpdater(unsigned int thread_count, MeshingParams params) {

	print_line("Constructing VoxelMeshUpdater");

	Ref<VoxelMesherBlocky> blocky_mesher;
	Ref<VoxelMesherTransvoxel> smooth_mesher;

	_minimum_padding = 0;
	_maximum_padding = 0;

	_collision_lod_count = params.collision_lod_count;

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
	const int collision_lod_count = _collision_lod_count;

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

		processors[i] = [blocky_mesher, smooth_mesher, collision_lod_count](const ArraySlice<InputBlock> inputs, ArraySlice<OutputBlock> outputs, Mgr::ProcessorStats &_) {
			process_blocks_thread_func(inputs, outputs, blocky_mesher, smooth_mesher, collision_lod_count);
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
		int collision_lod_count) {

	CRASH_COND(inputs.size() != outputs.size());

	struct L {
		static void build(Ref<VoxelMesher> mesher, VoxelMesher::Output &output, Ref<Shape> *shape, const VoxelMesher::Input &input) {
			if (mesher.is_valid()) {
				mesher->build(output, input);
				// TODO Decide which surfaces should be collidable, currently defaults to 0
				// TODO Support multiple shapes
				if (shape != nullptr && output.surfaces.size() > 0 && shape->is_null()) {
					Array surface = output.surfaces[0];
					if (is_surface_triangulated(surface)) {
						*shape = create_concave_polygon_shape(surface);
					}
				}
			}
		}
	};

	for (unsigned int i = 0; i < inputs.size(); ++i) {

		const InputBlock &ib = inputs[i];
		const InputBlockData &block = ib.data;
		OutputBlockData &output = outputs[i].data;

		CRASH_COND(block.voxels.is_null());

		VoxelMesher::Input input = { **block.voxels, ib.lod };

		Ref<Shape> *shape = nullptr;
		if (ib.lod < collision_lod_count) {
			shape = &output.collision_shape;
		}

		L::build(blocky_mesher, output.blocky_surfaces, shape, input);
		L::build(smooth_mesher, output.smooth_surfaces, shape, input);
	}
}
