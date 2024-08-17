#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include "../engine/gpu/compute_shader_resource.h"
#include "../engine/ids.h"
#include "../engine/priority_dependency.h"
#include "../util/containers/span.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/classes/resource.h"
#include "../util/math/box3i.h"
#include "../util/math/vector3f.h"
#include "../util/tasks/cancellation_token.h"
#include "../util/thread/mutex.h"

#include <memory>

namespace zylann {

class IThreadedTask;
class AsyncDependencyTracker;

namespace voxel {

class VoxelBuffer;
class ComputeShader;
struct ComputeShaderParameters;
struct StreamingDependency;
class VoxelData;

namespace godot {
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
		VoxelBuffer &voxel_buffer;
		Vector3i origin_in_voxels;
		uint32_t lod;
	};

	virtual Result generate_block(VoxelQueryData &input);

	struct BlockTaskParams {
		Vector3i block_position;
		VolumeID volume_id;
		uint8_t lod_index = 0;
		uint8_t block_size = 0;
		bool drop_beyond_max_distance = true;
		bool use_gpu = false; // This is a hint, if not supported it will just keep using CPU
		PriorityDependency priority_dependency;
		std::shared_ptr<StreamingDependency> stream_dependency; // For saving generator output
		std::shared_ptr<VoxelData> data; // Just for modifiers
		std::shared_ptr<AsyncDependencyTracker> tracker; // For async edits
		std::shared_ptr<VoxelBuffer> voxels; // Optionally re-use a voxel buffer for the result
		TaskCancellationToken cancellation_token; // For explicit cancellation
	};

	// Creates a threaded task that will use the generator asynchronously to generate a block that will be returned to
	// the requesting volume.
	virtual IThreadedTask *create_block_task(const BlockTaskParams &params) const;

	virtual bool supports_single_generation() const {
		return false;
	}

	virtual bool supports_series_generation() const {
		return false;
	}

	virtual bool supports_lod() const {
		return true;
	}

	// TODO Not sure if it's a good API regarding performance
	virtual VoxelSingleValue generate_single(Vector3i pos, unsigned int channel);

	virtual void generate_series(
			Span<const float> positions_x,
			Span<const float> positions_y,
			Span<const float> positions_z,
			unsigned int channel,
			Span<float> out_values,
			Vector3f min_pos,
			Vector3f max_pos
	);

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
		StdVector<ShaderParameter> parameters;

		// The generated source will contain a `generate` function starting with a `vec3 position` argument,
		// followed by outputs like `out float out_sd, ...`, which determines what will be returned by the
		// compute shader.
		StdVector<ShaderOutput> outputs;
	};

	struct ShaderOutputs {
		StdVector<ShaderOutput> outputs;
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

	// Caching API
	//
	// Some generators might use an internal cache to optimize performance. The following methods provide some info for
	// the generator to manage the lifetime of the cache.
	// VoxelTerrain only at the moment.

	// Must be called when a viewer gets paired, moved, or unpaired from the terrain.
	// Pairing should send an empty previous box.
	// Moving should send the the previous box and new box.
	// Unpairing should send an empty box as the current box.
	virtual void process_viewer_diff(ViewerID viewer_id, Box3i p_requested_box, Box3i p_prev_requested_box);

	virtual void clear_cache();

	// Editor

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const {}
#endif

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<godot::VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);

	std::shared_ptr<ComputeShader> _detail_rendering_shader;
	std::shared_ptr<ComputeShaderParameters> _detail_rendering_shader_parameters;
	std::shared_ptr<ComputeShader> _block_rendering_shader;
	std::shared_ptr<ComputeShaderParameters> _block_rendering_shader_parameters;
	std::shared_ptr<ShaderOutputs> _block_rendering_shader_outputs;
	Mutex _shader_mutex;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATOR_H
