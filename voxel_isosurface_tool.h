#ifndef VOXEL_ISOSURFACE_TOOL_H
#define VOXEL_ISOSURFACE_TOOL_H

#include "voxel_buffer.h"
#include <core/image.h>

// TODO Will be obsoleted by VoxelTool
class VoxelIsoSurfaceTool : public Reference {
	GDCLASS(VoxelIsoSurfaceTool, Reference)
public:
	enum Operation {
		OP_ADD = 0,
		OP_SUBTRACT,
		OP_SET
	};

	VoxelIsoSurfaceTool();

	void set_buffer(Ref<VoxelBuffer> buffer);
	Ref<VoxelBuffer> get_buffer() const;

	void set_iso_scale(float iso_scale);
	float get_iso_scale() const;

	void set_offset(Vector3 offset);
	Vector3 get_offset() const;

	void do_sphere(Vector3 center, real_t radius, Operation op);
	void do_plane(Plane plane, Operation op);
	void do_cube(Transform transform, Vector3 extents, Operation op);
	void do_heightmap(Ref<Image> heightmap, Vector3 offset, real_t vertical_scale, Operation op);

protected:
	static void _bind_methods();

private:
	Ref<VoxelBuffer> _buffer;
	float _iso_scale;
	Vector3 _offset;
};

VARIANT_ENUM_CAST(VoxelIsoSurfaceTool::Operation)

#endif // VOXEL_ISOSURFACE_TOOL_H
