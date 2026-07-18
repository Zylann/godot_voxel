#ifndef VOXEL_GRAPH_EDITOR_NODE_PREVIEW_INFO_H
#define VOXEL_GRAPH_EDITOR_NODE_PREVIEW_INFO_H

#include <cstdint>

namespace zylann::voxel {

class VoxelGraphEditorNodePreview;

struct VoxelGraphEditorNodePreviewInfo {
	VoxelGraphEditorNodePreview *control;
	uint32_t address;
	uint32_t node_id;
};

} // namespace zylann::voxel

#endif
