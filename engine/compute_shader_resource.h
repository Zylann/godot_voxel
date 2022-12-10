#ifndef VOXEL_COMPUTE_SHADER_RESOURCE_H
#define VOXEL_COMPUTE_SHADER_RESOURCE_H

#include "../util/godot/rid.h"
#include "../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Image);
ZN_GODOT_FORWARD_DECLARE(class Curve);

namespace zylann::voxel {

// Because we are using a different RenderingDevice for compute shaders (as we can't use the regular one) we have to
// create our own kinds of resources, because Godot's regular resources are all assuming the use of the main rendering
// device...

// Thin wrapper around a resource held by VoxelEngine's RenderingDevice
class ComputeShaderResource {
public:
	enum Type { TYPE_TEXTURE };

	ComputeShaderResource();
	ComputeShaderResource(ComputeShaderResource &&other);
	~ComputeShaderResource();

	void clear();
	bool is_valid() const;
	Type get_type() const;
	void create_texture(Image &image);
	void create_texture(Curve &curve);
	RID get_rid() const;

	void operator=(ComputeShaderResource &&src);

private:
	Type _type = TYPE_TEXTURE;
	RID _rid;
};

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_RESOURCE_H
