#ifndef VOXEL_VOLUME_ID_H
#define VOXEL_VOLUME_ID_H

#include "../util/slot_map.h"
#include <iosfwd>

namespace zylann::voxel {

typedef SlotMapKey<uint16_t, uint16_t> VolumeID;
typedef SlotMapKey<uint16_t, uint16_t> ViewerID;

} // namespace zylann::voxel

namespace zylann {
std::stringstream &operator<<(std::stringstream &ss, const SlotMapKey<uint16_t, uint16_t> &v);
} // namespace zylann

#endif // VOXEL_VOLUME_ID_H
