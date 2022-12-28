#ifndef VOXEL_COMPUTE_SHADER_RESOURCE_H
#define VOXEL_COMPUTE_SHADER_RESOURCE_H

#include "../util/godot/core/packed_byte_array.h"
#include "../util/godot/core/rid.h"
#include "../util/macros.h"
#include "../util/math/transform_3d.h"
#include "../util/math/vector3i.h"
#include "../util/span.h"

ZN_GODOT_FORWARD_DECLARE(class Image);
ZN_GODOT_FORWARD_DECLARE(class Curve);

namespace zylann::voxel {

// Because we are using a different RenderingDevice for compute shaders (as we can't use the regular one) we have to
// create our own kinds of resources, because Godot's regular resources are all assuming the use of the main rendering
// device...

// Thin wrapper around a resource held by VoxelEngine's RenderingDevice
class ComputeShaderResource {
public:
	enum Type { TYPE_TEXTURE_2D, TYPE_TEXTURE_3D, TYPE_STORAGE_BUFFER };

	ComputeShaderResource();
	ComputeShaderResource(ComputeShaderResource &&other);
	~ComputeShaderResource();

	void clear();
	bool is_valid() const;
	Type get_type() const;
	void create_texture_2d(const Image &image);
	void create_texture_2d(const Curve &curve);
	void create_texture_3d_zxy(Span<const float> fdata_zxy, Vector3i size);
	void create_storage_buffer(const PackedByteArray &data);
	void update_storage_buffer(const PackedByteArray &data);
	RID get_rid() const;

	void operator=(ComputeShaderResource &&src);

private:
	Type _type = TYPE_TEXTURE_2D;
	RID _rid;
};

// Converts a 3D transform into a 4x4 matrix with a layout usable in GLSL.
void transform3d_to_mat4(const Transform3D &t, Span<float> dst);

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_RESOURCE_H
