#ifndef VOXEL_MODIFIER_STACK_H
#define VOXEL_MODIFIER_STACK_H

#include "../util/containers/std_unordered_map.h"
#include "../util/containers/std_vector.h"
#include "../util/math/vector3f.h"
#include "../util/memory/memory.h"
#include "voxel_modifier.h"

namespace zylann::voxel {

class VoxelBuffer;

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
	void apply(VoxelBuffer &voxels, AABB aabb) const;
	void apply(float &sdf, Vector3 position) const;
	void apply(Span<const float> x_buffer, Span<const float> y_buffer, Span<const float> z_buffer,
			Span<float> sdf_buffer, Vector3f min_pos, Vector3f max_pos) const;
	void apply_for_gpu_rendering(
			StdVector<VoxelModifier::ShaderData> &out_data, AABB aabb, VoxelModifier::ShaderData::Type type) const;
	void clear();

	template <typename F>
	void for_each_modifier(F f) const {
		RWLockRead rlock(_stack_lock);
		for (const VoxelModifier *modifier : _stack) {
			f(*modifier);
		}
	}

private:
	void move_from_noclear(VoxelModifierStack &other);

	StdUnorderedMap<uint32_t, UniquePtr<VoxelModifier>> _modifiers;
	uint32_t _next_id = 1;
	// TODO Later, replace this with a spatial acceleration structure based on AABBs, like BVH
	StdVector<VoxelModifier *> _stack;
	RWLock _stack_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_STACK_H
