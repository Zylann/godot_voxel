#ifndef VOXEL_MODIFIERS_H
#define VOXEL_MODIFIERS_H

#include "../edition/voxel_mesh_sdf_gd.h"
#include "../engine/compute_shader_parameters.h"
#include "../util/math/sdf.h"
#include "../util/math/transform_3d.h"
#include "../util/math/vector3.h"
#include "../util/math/vector3f.h"
#include "../util/thread/mutex.h"
#include "../util/thread/rw_lock.h"
#include "voxel_buffer_internal.h"

#include <unordered_map>

namespace zylann::voxel {

struct VoxelModifierContext {
	Span<float> sdf;
	Span<const Vector3> positions;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifier {
public:
	enum Type { TYPE_SPHERE, TYPE_MESH };

	virtual ~VoxelModifier() {}

	virtual void apply(VoxelModifierContext ctx) const = 0;

	const Transform3D &get_transform() const {
		return _transform;
	}

	void set_transform(Transform3D t);

	const AABB &get_aabb() const {
		return _aabb;
	}

	virtual Type get_type() const = 0;
	virtual bool is_sdf() const = 0;

	struct ShaderData {
		RID detail_rendering_shader_rid;
		std::shared_ptr<ComputeShaderParameters> params;
	};

	virtual void get_shader_data(ShaderData &out_shader_data) = 0;

protected:
	virtual void update_aabb() = 0;

	RWLock _rwlock;
	AABB _aabb;

	std::shared_ptr<ComputeShaderParameters> _shader_data;
	bool _shader_data_need_update = false;

private:
	Transform3D _transform;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifierSdf : public VoxelModifier {
public:
	enum Operation { //
		OP_ADD = 0,
		OP_SUBTRACT = 1
	};

	inline Operation get_operation() const {
		return _operation;
	}

	void set_operation(Operation op);

	inline float get_smoothness() const {
		return _smoothness;
	}

	void set_smoothness(float p_smoothness);

	bool is_sdf() const override {
		return true;
	}

protected:
	void update_base_shader_data_no_lock();

private:
	Operation _operation = OP_ADD;
	float _smoothness = 0.f;
	// float _margin = 0.f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifierSphere : public VoxelModifierSdf {
public:
	Type get_type() const override {
		return TYPE_SPHERE;
	};
	void set_radius(real_t radius);
	real_t get_radius() const;
	void apply(VoxelModifierContext ctx) const override;
	void get_shader_data(ShaderData &out_shader_data) override;

protected:
	void update_aabb() override;

private:
	real_t _radius = 10.f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifierMesh : public VoxelModifierSdf {
public:
	Type get_type() const override {
		return TYPE_MESH;
	};

	void set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf);
	void set_isolevel(float isolevel);
	void apply(VoxelModifierContext ctx) const override;
	void get_shader_data(ShaderData &out_shader_data) override;
	void request_shader_data_update();

protected:
	void update_aabb() override;

private:
	// Originally I wanted to keep the core of modifiers separate from Godot stuff, but in order to also support
	// GPU resources, putting this here was easier.
	Ref<VoxelMeshSDF> _mesh_sdf;
	float _isolevel;
};

// TODO Capsule
// TODO Box
// TODO Heightmap
// TODO Graph (using generator graph as a brush?)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifierStack {
public:
	uint32_t allocate_id();

	VoxelModifierStack();
	VoxelModifierStack(VoxelModifierStack &&other);

	VoxelModifierStack &operator=(VoxelModifierStack &&other);

	template <typename T>
	T *add_modifier(uint32_t id) {
		ZN_ASSERT(!has_modifier(id));
		UniquePtr<VoxelModifier> &uptr = _modifiers[id];
		uptr = make_unique_instance<T>();
		VoxelModifier *ptr = uptr.get();
		RWLockWrite lock(_stack_lock);
		_stack.push_back(ptr);
		return static_cast<T *>(ptr);
	}

	void remove_modifier(uint32_t id);
	bool has_modifier(uint32_t id) const;
	VoxelModifier *get_modifier(uint32_t id) const;
	void apply(VoxelBufferInternal &voxels, AABB aabb) const;
	void apply(float &sdf, Vector3 position) const;
	void apply(Span<const float> x_buffer, Span<const float> y_buffer, Span<const float> z_buffer,
			Span<float> sdf_buffer, Vector3f min_pos, Vector3f max_pos) const;
	void apply_for_detail_gpu_rendering(std::vector<VoxelModifier::ShaderData> &out_data, AABB aabb) const;
	void clear();

private:
	void move_from_noclear(VoxelModifierStack &other);

	std::unordered_map<uint32_t, UniquePtr<VoxelModifier>> _modifiers;
	uint32_t _next_id = 1;
	// TODO Later, replace this with a spatial acceleration structure based on AABBs, like BVH
	std::vector<VoxelModifier *> _stack;
	RWLock _stack_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_MODIFIERS_H
