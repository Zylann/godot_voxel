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

class VoxelProceduralCubemap : public Resource {
	GDCLASS(VoxelProceduralCubemap, Resource)
public:
	enum Format {
		FORMAT_R8 = 0,
		FORMAT_L8,
		FORMAT_RF,
		FORMAT_COUNT,
	};

	void set_resolution(const unsigned int new_resolution);
	int get_resolution() const;

	void set_format(const Format format);
	Format get_format() const;

	void set_graph(Ref<pg::VoxelGraphFunction> graph);
	Ref<pg::VoxelGraphFunction> get_graph() const;

	void update();

	Ref<GodotCubemap> create_texture() const;

private:
	static void _bind_methods();

	unsigned int _resolution = 256;
	Format _format = FORMAT_L8;
	zylann::Cubemap _cubemap;
	Ref<pg::VoxelGraphFunction> _graph;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelProceduralCubemap::Format);

#endif // VOXEL_PROCEDURAL_CUBEMAP_H
