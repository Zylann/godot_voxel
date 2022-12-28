#ifndef VOXEL_COLOR_PALETTE_H
#define VOXEL_COLOR_PALETTE_H

#include "../../util/fixed_array.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/math/color8.h"

namespace zylann::voxel {

// Associates small numbers to colors, so colored voxels can be specified using less memory.
class VoxelColorPalette : public Resource {
	GDCLASS(VoxelColorPalette, Resource)
public:
	static const unsigned int MAX_COLORS = 256;

	VoxelColorPalette();

	void set_color(int index, Color color);
	Color get_color(int index) const;

	PackedColorArray get_colors() const;
	void set_colors(PackedColorArray colors);

	void clear();

	// Internal

	inline void set_color8(uint8_t i, Color8 c) {
		_colors[i] = c;
	}

	inline Color8 get_color8(uint8_t i) const {
		return _colors[i];
	}

private:
	PackedInt32Array _b_get_data() const;
	void _b_set_data(PackedInt32Array colors);

	static void _bind_methods();

	FixedArray<Color8, MAX_COLORS> _colors;
};

} // namespace zylann::voxel

#endif // VOXEL_COLOR_PALETTE_H
