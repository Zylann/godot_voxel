#ifndef VOXEL_MODIFIER_STACK_H
#define VOXEL_MODIFIER_STACK_H

#include "../util/math/vector3f.h"
#include "../util/memory.h"
#include "voxel_modifier.h"

#include <unordered_map>

namespace zylann::voxel {

class VoxelBufferInternal;

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

#endif // VOXEL_MODIFIER_STACK_H
