#ifndef VOXEL_UP_MODE_H
#define VOXEL_UP_MODE_H

namespace zylann::voxel {

// Tells how to interpret where "upwards" is in the current volume
enum UpMode : uint8_t {
	// The world is a plane, so altitude is obtained from the Y coordinate and upwards is always toward +Y.
	UP_MODE_POSITIVE_Y,
	// The world is a sphere (planet), so altitude is obtained from distance to the origin (0,0,0),
	// and upwards is the normalized vector from origin to current position.
	UP_MODE_SPHERE,
	// How many up modes there are
	UP_MODE_COUNT
};

} // namespace zylann::voxel

#endif // VOXEL_UP_MODE_H
