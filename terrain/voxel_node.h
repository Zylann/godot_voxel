#ifndef VOXEL_NODE_H
#define VOXEL_NODE_H

#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../streams/voxel_stream.h"

#include <scene/3d/spatial.h>

// Base class for voxel volumes
class VoxelNode : public Spatial {
	GDCLASS(VoxelNode, Spatial)
public:
	virtual void set_mesher(Ref<VoxelMesher> mesher);
	virtual Ref<VoxelMesher> get_mesher() const;

	virtual void set_stream(Ref<VoxelStream> stream);
	virtual Ref<VoxelStream> get_stream() const;

	virtual void set_generator(Ref<VoxelGenerator> generator);
	virtual Ref<VoxelGenerator> get_generator() const;

	virtual void restart_stream();
	virtual void remesh_all_blocks();

	virtual String get_configuration_warning() const override;

protected:
	int get_used_channels_mask() const;

private:
	Ref<VoxelMesher> _b_get_mesher() { return get_mesher(); }
	void _b_set_mesher(Ref<VoxelMesher> mesher) { set_mesher(mesher); }

	Ref<VoxelStream> _b_get_stream() { return get_stream(); }
	void _b_set_stream(Ref<VoxelStream> stream) { set_stream(stream); }

	Ref<VoxelGenerator> _b_get_generator() { return get_generator(); }
	void _b_set_generator(Ref<VoxelGenerator> g) { set_generator(g); }

	static void _bind_methods();
};

#endif // VOXEL_NODE_H
