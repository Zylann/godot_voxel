#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include "../engine/gpu/compute_shader_resource.h"
#include "../util/godot/classes/resource.h"
#include "../util/math/vector3f.h"
#include "../util/math/vector3i.h"
#include "../util/span.h"
#include "../util/thread/mutex.h"
#include <memory>

namespace zylann::voxel {

class VoxelBufferInternal;
class ComputeShader;
struct ComputeShaderParameters;

namespace gd {
class VoxelBuffer;
}

// Non-encoded, generic voxel value.
// (Voxels stored inside VoxelBuffers are encoded to take less space)
union VoxelSingleValue {
	uint64_t i;
	float f;
};

// Provides access to read-only generated voxels.
// Must be implemented in a multi-thread-safe way.
class VoxelGenerator : public Resource {
	GDCLASS(VoxelGenerator, Resource)
public:
	VoxelGenerator();

	struct Result {
		// Used for block optimization when LOD is used.
		// If this is `false`, more precise data may be found if a lower LOD index is requested.
		// If `true`, any block below this LOD are considered to not bring more details or will be the same.
		// This allows to reduce the number of blocks to load when LOD is used.
		bool max_lod_hint = false;
	};

	struct VoxelQueryData {
		VoxelBufferInternal &voxel_buffer;
		Vector3i origin_in_voxels;
		uint32_t lod;
	};

	virtual Result generate_block(VoxelQueryData &input);

	virtual bool supports_single_generation() const {
		return false;
	}

	virtual bool supports_series_generation() const {
		return false;
	}

	// TODO Not sure if it's a good API regarding performance
	virtual VoxelSingleValue generate_single(Vector3i pos, unsigned int channel);

	virtual void generate_series(Span<const float> positions_x, Span<const float> positions_y,
			Span<const float> positions_z, unsigned int channel, Span<float> out_values, Vector3f min_pos,
			Vector3f max_pos);

	// Declares the channels this generator will use
	virtual int get_used_channels_mask() const;

	// GPU support
	// The way this support works is by providing a shader and parameters that can produce the same results as the CPU
	// version of the generator.

	virtual bool supports_shaders() const {
		return false;
	}

	struct ShaderParameter {
		String name;
		ComputeShaderResource resource;
	};

	struct ShaderOutput {
		enum Type { TYPE_SDF, TYPE_SINGLE_TEXTURE, TYPE_TYPE };
		Type type;
	};

	struct ShaderSourceData {
		// Source code relevant to the generator only. Does not contain interface blocks (uniforms), because they may be
		// generated depending on where the code is integrated.
		String glsl;
		// Associated resources
		std::vector<ShaderParameter> parameters;

		// The generated source will contain a `generate` function starting with a `vec3 position` argument,
		// followed by outputs like `out float out_sd, ...`, which determines what will be returned by the
		// compute shader.
		std::vector<ShaderOutput> outputs;
	};

	struct ShaderOutputs {
		std::vector<ShaderOutput> outputs;
	};

	virtual bool get_shader_source(ShaderSourceData &out_data) const;
	std::shared_ptr<ComputeShader> get_detail_rendering_shader();
	std::shared_ptr<ComputeShaderParameters> get_detail_rendering_shader_parameters();
	std::shared_ptr<ComputeShader> get_block_rendering_shader();
	// TODO Shouldn't these parameters be shared for each shader type?
	std::shared_ptr<ComputeShaderParameters> get_block_rendering_shader_parameters();
	std::shared_ptr<ShaderOutputs> get_block_rendering_shader_outputs();
	void compile_shaders();
	// Drops currently compiled shaders if any, so that they get recompiled when they are needed again
	void invalidate_shaders();

	// Requests to generate a broad result, which is supposed to be faster to obtain than full generation.
	// If it returns true, the returned block may be used as if it was a result from `generate_block`.
	// If it returns false, no block is returned and full generation should be used.
	// Usually, `generate_block` can do this anyways internally, but in some cases like GPU generation it may be used
	// to avoid sending work to the graphics card.
	virtual bool generate_broad_block(VoxelQueryData &input);

	// Editor

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const {}
#endif

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<gd::VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);

	std::shared_ptr<ComputeShader> _detail_rendering_shader;
	std::shared_ptr<ComputeShaderParameters> _detail_rendering_shader_parameters;
	std::shared_ptr<ComputeShader> _block_rendering_shader;
	std::shared_ptr<ComputeShaderParameters> _block_rendering_shader_parameters;
	std::shared_ptr<ShaderOutputs> _block_rendering_shader_outputs;
	Mutex _shader_mutex;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_H
