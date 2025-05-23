#ifndef VOXEL_PROCEDURAL_CUBEMAP_H
#define VOXEL_PROCEDURAL_CUBEMAP_H

#include "../../util/godot/classes/noise.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/godot/cubemap.h"
#include "voxel_graph_function.h"

namespace zylann::voxel {

// This could almost be separate from Voxel, but isn't, because the graph backend is part of Voxel...
// I'd like to separate it one day, but doing that with GDExtension is quite the hassle.

// Not sure if I should make it inherit Cubemap. The initial use I make of it is CPU-based, and the GPU usage would come
// from editor inspection. If there is a reliable way to prevent the data from being uploaded to GPU/saved to disk based
// on an option, that could be possible?

class VoxelProceduralCubemap : public ZN_Cubemap {
	GDCLASS(VoxelProceduralCubemap, ZN_Cubemap)
public:
	enum Format {
		FORMAT_R8 = 0,
		FORMAT_L8,
		FORMAT_RF,
		FORMAT_COUNT,
	};

	void set_target_resolution(const unsigned int new_resolution);
	int get_target_resolution() const;

	void set_target_format(const Format format);
	Format get_target_format() const;

	void set_derivatives_enabled(const bool enabled);
	bool get_derivatives_enabled() const;

	void set_derivative_pixel_step(const float step);
	float get_derivative_pixel_step() const;

	void set_graph(Ref<pg::VoxelGraphFunction> graph);
	Ref<pg::VoxelGraphFunction> get_graph() const;

	void update();

	bool is_dirty() const {
		return _dirty;
	}

	Ref<ZN_Cubemap> zn_duplicate() const override;

private:
	void on_graph_changed();

	static void _bind_methods();

	unsigned int _target_resolution = 256;
	bool _dirty = false;
	bool _derivatives_enabled = false;
	float _derivative_pixel_step = 0.1f;
	Format _target_format = FORMAT_L8;
	Ref<pg::VoxelGraphFunction> _graph;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelProceduralCubemap::Format);

#endif // VOXEL_PROCEDURAL_CUBEMAP_H
