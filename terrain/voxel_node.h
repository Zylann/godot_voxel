#ifndef VOXEL_NODE_H
#define VOXEL_NODE_H

#include <scene/3d/spatial.h>

class VoxelMesher;
class VoxelStream;

// Base class for voxel volumes
class VoxelNode : public Spatial {
	GDCLASS(VoxelNode, Spatial)
public:
	virtual void set_mesher(Ref<VoxelMesher> mesher);
	virtual Ref<VoxelMesher> get_mesher() const;

	virtual void set_stream(Ref<VoxelStream> stream);
	virtual Ref<VoxelStream> get_stream() const;

	virtual void restart_stream();
	virtual void remesh_all_blocks();

	String get_configuration_warning() const override;

private:
	Ref<VoxelMesher> _b_get_mesher() { return get_mesher(); }
	void _b_set_mesher(Ref<VoxelMesher> mesher) { set_mesher(mesher); }

	Ref<VoxelStream> _b_get_stream() { return get_stream(); }
	void _b_set_stream(Ref<VoxelStream> stream) { set_stream(stream); }

	static void _bind_methods();
};

#endif // VOXEL_NODE_H
