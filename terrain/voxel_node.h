#ifndef VOXEL_NODE_H
#define VOXEL_NODE_H

#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../server/streaming_dependency.h"
#include "../streams/voxel_stream.h"

#include <scene/3d/node_3d.h>

namespace zylann::voxel {

class VoxelTool;

// Base class for voxel volumes
class VoxelNode : public Node3D {
	GDCLASS(VoxelNode, Node3D)
public:
	virtual void set_mesher(Ref<VoxelMesher> mesher);
	virtual Ref<VoxelMesher> get_mesher() const;

	virtual void set_stream(Ref<VoxelStream> stream);
	virtual Ref<VoxelStream> get_stream() const;

	virtual void set_generator(Ref<VoxelGenerator> generator);
	virtual Ref<VoxelGenerator> get_generator() const;

	enum GIMode { //
		GI_MODE_DISABLED = 0,
		GI_MODE_BAKED,
		GI_MODE_DYNAMIC,
		_GI_MODE_COUNT
	};

	void set_gi_mode(GIMode mode);
	GIMode get_gi_mode() const;

	virtual void restart_stream();
	virtual void remesh_all_blocks();

	virtual uint32_t get_volume_id() const;
	virtual std::shared_ptr<StreamingDependency> get_streaming_dependency() const;

	virtual Ref<VoxelTool> get_voxel_tool();

#ifdef TOOLS_ENABLED
	virtual TypedArray<String> get_configuration_warnings() const override;
#endif

protected:
	int get_used_channels_mask() const;

	virtual void _on_gi_mode_changed() {}

private:
	Ref<VoxelMesher> _b_get_mesher() {
		return get_mesher();
	}
	void _b_set_mesher(Ref<VoxelMesher> mesher) {
		set_mesher(mesher);
	}

	Ref<VoxelStream> _b_get_stream() {
		return get_stream();
	}
	void _b_set_stream(Ref<VoxelStream> stream) {
		set_stream(stream);
	}

	Ref<VoxelGenerator> _b_get_generator() {
		return get_generator();
	}
	void _b_set_generator(Ref<VoxelGenerator> g) {
		set_generator(g);
	}

	static void _bind_methods();

	GIMode _gi_mode = GI_MODE_DISABLED;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelNode::GIMode);

#endif // VOXEL_NODE_H
