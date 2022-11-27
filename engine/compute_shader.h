#ifndef VOXEL_COMPUTE_SHADER_H
#define VOXEL_COMPUTE_SHADER_H

#include "../util/godot/rid.h"
#include "../util/godot/string.h"
#include "../util/memory.h"

namespace zylann::voxel {

// Thin RAII wrapper around compute shaders created with VoxelEngine RenderingDevice.
// May be passed around using shared pointers.
// A reference should be kept as long as a dispatch of this shader is running on the graphics card.
class ComputeShader {
public:
	static std::shared_ptr<ComputeShader> create_from_glsl(String source_text);
	static std::shared_ptr<ComputeShader> create_invalid();

	ComputeShader(RID p_rid);
	~ComputeShader();

	// An invalid instance means the shader failed to compile
	inline bool is_valid() const {
		return _rid.is_valid();
	}

	inline RID get_rid() const {
		return _rid;
	}

private:
	RID _rid;
};

} // namespace zylann::voxel

#endif // VOXEL_COMPUTE_SHADER_H
