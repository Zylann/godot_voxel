#ifndef VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H
#define VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H

#include "../../util/fixed_array.h"
#include "../../util/godot/classes/scroll_container.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Label)
ZN_GODOT_FORWARD_DECLARE(class HBoxContainer)

namespace zylann::voxel {

class VoxelTerrainEditorTaskIndicator : public ScrollContainer {
	GDCLASS(VoxelTerrainEditorTaskIndicator, ScrollContainer)
public:
	VoxelTerrainEditorTaskIndicator();

	void _notification(int p_what);
	void update_stats();

private:
	enum StatID {
		STAT_STREAM_TASKS,
		STAT_GENERATE_TASKS,
		STAT_MESH_TASKS,
		STAT_TOTAL_TASKS,
		STAT_MAIN_THREAD_TASKS,
		STAT_TOTAL_MEMORY,
		STAT_VOXEL_MEMORY,
		STAT_COUNT
	};

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	void create_stat(StatID id, String short_name, String long_name);
	void set_stat(StatID id, int64_t value);
	void set_stat(StatID id, int64_t value, const char *unit);
	void set_stat(StatID id, int64_t value, int64_t value2, const char *unit);

	struct Stat {
		int64_t value = 0;
		int64_t value2 = 0;
		Label *label = nullptr;
	};

	FixedArray<Stat, STAT_COUNT> _stats;
	HBoxContainer *_box_container = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_EDITOR_TASK_INDICATOR_H
