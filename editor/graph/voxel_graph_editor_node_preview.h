#ifndef VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
#define VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H

#include "../../generators/graph/voxel_graph_runtime.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/image_texture.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/macros.h"
#include "../../util/math/vector2f.h"
#include "graph_preview_mode.h"

ZN_GODOT_FORWARD_DECLARE(class TextureRect)

namespace zylann::voxel {

struct GraphEditorAdapter;

// Shows a 2D slice of the 3D set of values coming from an output port
class VoxelGraphEditorNodePreview : public VBoxContainer {
	GDCLASS(VoxelGraphEditorNodePreview, VBoxContainer)
public:
	enum PixelMode { //
		PIXEL_MODE_GREYSCALE = 0,
		PIXEL_MODE_SDF = 1,
		PIXEL_MODE_COUNT
	};

	static void load_resources();
	static void unload_resources();

	VoxelGraphEditorNodePreview();

	void update_from_buffer(const pg::Runtime::Buffer &buffer);
	void update_display_settings(const pg::VoxelGraphFunction &graph, uint32_t node_id);

	struct PreviewInfo {
		VoxelGraphEditorNodePreview *control;
		uint32_t address;
		uint32_t node_id;
	};

	static void update_previews(
			GraphEditorAdapter &adapter,
			Span<const PreviewInfo> previews,
			const GraphEditorPreview::ViewMode view_mode,
			const float transform_scale,
			const Vector2f transform_offset
	);

private:
	static const int RESOLUTION = 128;

	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}

	TextureRect *_texture_rect = nullptr;
	Ref<ImageTexture> _texture;
	Ref<Image> _image;
	Ref<ShaderMaterial> _material;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
