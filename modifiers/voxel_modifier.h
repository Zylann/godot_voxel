#ifndef VOXEL_MODIFIER_H
#define VOXEL_MODIFIER_H

#include "../engine/compute_shader_parameters.h"
#include "../util/math/transform_3d.h"
#include "../util/thread/rw_lock.h"

namespace zylann::voxel {

struct VoxelModifierContext {
	Span<float> sdf;
	Span<const Vector3> positions;
};

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

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_H
