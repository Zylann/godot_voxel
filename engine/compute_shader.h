#ifndef VOXEL_COMPUTE_SHADER_H
#define VOXEL_COMPUTE_SHADER_H

#include "../util/godot/core/rid.h"
#include "../util/godot/core/string.h"
#include "../util/memory.h"

namespace zylann::voxel {

// Thin RAII wrapper around compute shaders created with the `RenderingDevice` held inside `VoxelEngine`.
// If the source can change at runtime, it may be passed around using shared pointers and a new instance may be created,
// rather than clearing the old shader anytime, for thread-safety. A reference should be kept as long as a dispatch of
// this shader is running on the graphics card.
class ComputeShader {
public:
	static std::shared_ptr<ComputeShader> create_from_glsl(String source_text, String name);
	static std::shared_ptr<ComputeShader> create_invalid();

	ComputeShader();
	ComputeShader(RID p_rid);

	~ComputeShader();

	void clear();

	void load_from_glsl(String source_text, String name);

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
