#ifndef VOXEL_MESHER_SCRIPT_H
#define VOXEL_MESHER_SCRIPT_H

#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/gdvirtual.h"
#include "../../util/thread/mutex.h"
#include "../voxel_mesher.h"
// GDVIRTUAL requires complete definition of the class
#include "voxel_mesher_context.h"

namespace zylann::voxel {

// Allows to implement an entirely custom mesher using the scripting API.
// With GDScript it can be used for prototyping, and faster languages like C# could be used for viable performance.
// If you are creating a custom mesher with C++ using direct access to the module, prefer using VoxelMesher.
class VoxelMesherScript : public VoxelMesher {
	GDCLASS(VoxelMesherScript, VoxelMesher)
public:
	VoxelMesherScript();
	~VoxelMesherScript();

	void build(VoxelMesherOutput &output, const VoxelMesherInput &input) override;
	Ref<Material> get_material_by_index(unsigned int i) const override;
	unsigned int get_material_index_count() const override;

protected:
	GDVIRTUAL1C(_build, VoxelMesherContext *)
	GDVIRTUAL1RC(Ref<Material>, _get_material_by_index, int)
	GDVIRTUAL0RC(int, _get_material_index_count)

private:
	VoxelMesherContext *get_or_create_context();
	void return_context(VoxelMesherContext *context);

	void _b_set_padding(const int p_negative_padding, const int p_positive_padding);

	static void _bind_methods();

	StdVector<VoxelMesherContext *> _context_pool;
	Mutex _context_pool_mutex;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_SCRIPT_H
