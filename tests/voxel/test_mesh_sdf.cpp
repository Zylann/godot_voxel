#include "test_mesh_sdf.h"
#include "../../edition/voxel_mesh_sdf_gd.h"

namespace zylann::voxel::tests {

void test_voxel_mesh_sdf_issue463() {
	Ref<VoxelMeshSDF> msdf;
	msdf.instantiate();

	Dictionary d;
	d["roman"] = 22;
	d[22] = 25;
	// TODO The original report was creating a BoxShape3D, but for reasons beyond my understanding, Godot's
	// PhysicsServer3D is still not created after `MODULE_INITIALIZATION_LEVEL_SERVERS`. And not even SCENE or EDITOR
	// levels. It's impossible to use a level to do anything with physics.... Go figure.
	//
	// Ref<BoxShape3D> shape1;
	// shape1.instantiate();
	// Ref<BoxShape3D> shape2;
	// shape2.instantiate();
	// d[shape1] = shape2;
	Ref<Resource> res1;
	res1.instantiate();
	Ref<Resource> res2;
	res2.instantiate();
	d[res1] = res2;

	ZN_ASSERT(msdf->has_method("_set_data"));
	// Setting invalid data should cause an error but not crash or leak
	msdf->call("_set_data", d);
}

} // namespace zylann::voxel::tests
