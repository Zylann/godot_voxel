#ifndef VOXEL_MODIFIER_SDF_H
#define VOXEL_MODIFIER_SDF_H

#include "voxel_modifier.h"

namespace zylann::voxel {

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

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_SDF_H
