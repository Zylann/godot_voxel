#include "voxel_terrain_editor_task_indicator.h"
#include "../../engine/voxel_engine.h"
#include "../../storage/voxel_memory_pool.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/font.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/os.h"
#include "../../util/godot/classes/v_separator.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

VoxelTerrainEditorTaskIndicator::VoxelTerrainEditorTaskIndicator() {
	// We use a scroll container so it doesn't prevent Godot from shrinking horizontally on small screens

	// We only want horizontal scrolling
	set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
	set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);

	_box_container = memnew(HBoxContainer);
	add_child(_box_container);

	create_stat(STAT_STREAM_TASKS, ZN_TTR("I/O"), ZN_TTR("I/O tasks"));
	create_stat(STAT_GENERATE_TASKS, ZN_TTR("Gen"), ZN_TTR("Generation tasks"));
	create_stat(STAT_MESH_TASKS, ZN_TTR("Mesh"), ZN_TTR("Meshing tasks"));
	create_stat(STAT_TOTAL_TASKS, ZN_TTR("Total"), ZN_TTR("Total remaining threaded tasks"));
	create_stat(STAT_MAIN_THREAD_TASKS, ZN_TTR("Main"), ZN_TTR("Main thread tasks"));
	create_stat(STAT_VOXEL_MEMORY, ZN_TTR("Voxel memory"), ZN_TTR("Memory for voxels (in use / pooled)"));
	create_stat(STAT_TOTAL_MEMORY, ZN_TTR("Total"), ZN_TTR("Memory usage (whole editor, not just voxel)"));
}

void VoxelTerrainEditorTaskIndicator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
			// Set a monospace font.
			// Can't do this in constructor, fonts are not available then. Also the theme can change.
			for (unsigned int i = 0; i < _stats.size(); ++i) {
				_stats[i].label->add_theme_font_override("font", get_theme_font("source", "EditorFonts"));
			}
			break;
	}
}

void VoxelTerrainEditorTaskIndicator::update_stats() {
	const VoxelEngine::Stats stats = VoxelEngine::get_singleton().get_stats();
	set_stat(STAT_STREAM_TASKS, stats.streaming_tasks);
	set_stat(STAT_GENERATE_TASKS, stats.generation_tasks);
	set_stat(STAT_MESH_TASKS, stats.meshing_tasks);
	set_stat(STAT_TOTAL_TASKS, stats.general.tasks);
	set_stat(STAT_MAIN_THREAD_TASKS, stats.main_thread_tasks);
	const VoxelMemoryPool &mp = VoxelMemoryPool::get_singleton();
	set_stat(STAT_VOXEL_MEMORY, int64_t(mp.debug_get_used_memory()), int64_t(mp.debug_get_total_memory()), "b");
	set_stat(STAT_TOTAL_MEMORY, int64_t(OS::get_singleton()->get_static_memory_usage()), "b");
}

void VoxelTerrainEditorTaskIndicator::create_stat(StatID id, String short_name, String long_name) {
	_box_container->add_child(memnew(VSeparator));
	Stat &stat = _stats[id];
	CRASH_COND(stat.label != nullptr);
	Label *name_label = memnew(Label);
	name_label->set_text(short_name);
	name_label->set_tooltip_text(long_name);
	name_label->set_mouse_filter(Control::MOUSE_FILTER_PASS); // Necessary for tooltip to work
	_box_container->add_child(name_label);
	stat.label = memnew(Label);
	stat.label->set_custom_minimum_size(Vector2(45 * EDSCALE, 0));
	stat.label->set_text("---");
	_box_container->add_child(stat.label);
}

// TODO Optimize: these String formatting functions are extremely slow in GDExtension, due to indirection costs,
// extra calls and unnecessary allocations caused by workarounds for missing APIs. It would just be more portable to
// format a local `std::string` and convert to `String` at the end.

static String with_commas(int64_t n) {
	String res = "";
	if (n < 0) {
		res += "-";
		n = -n;
	}
	String s = String::num_int64(n);
	const int mod = s.length() % 3;
	for (int i = 0; i < s.length(); ++i) {
		if (i != 0 && i % 3 == mod) {
			res += ",";
		}
		res += s[i];
	}
	return res;
}

// There is `String::humanize_size` but:
// 1) it is specifically for bytes, 2) it works on a 1024 base
// This function allows any unit, and works on a base of 1000
static String with_unit(int64_t n, const char *unit) {
	String s = "";
	if (n < 0) {
		s += "-";
		n = -n;
	}
	// TODO Perhaps use a for loop
	if (n >= 1000'000'000'000) {
		s += with_commas(n / 1000'000'000'000);
		s += ".";
		s += String::num_int64((n % 1000'000'000'000) / 1000'000'000).pad_zeros(3);
		s += " T";
		s += unit;
		return s;
	}
	if (n >= 1000'000'000) {
		s += String::num_int64(n / 1000'000'000);
		s += ".";
		s += String::num_int64((n % 1000'000'000) / 1000'000).pad_zeros(3);
		s += " G";
		s += unit;
		return s;
	}
	if (n >= 1000'000) {
		s += String::num_int64(n / 1000'000);
		s += ".";
		s += String::num_int64((n % 1000'000) / 1000).pad_zeros(3);
		s += " M";
		s += unit;
		return s;
	}
	if (n >= 1000) {
		s += String::num_int64(n / 1000);
		s += ".";
		s += String::num_int64(n % 1000).pad_zeros(3);
		s += " K";
		s += unit;
		return s;
	}
	s += String::num_int64(n);
	s += " ";
	s += unit;
	return s;
}

void VoxelTerrainEditorTaskIndicator::set_stat(StatID id, int64_t value) {
	Stat &stat = _stats[id];
	if (stat.value != value) {
		stat.value = value;
		stat.label->set_text(with_commas(stat.value));
	}
}

void VoxelTerrainEditorTaskIndicator::set_stat(StatID id, int64_t value, const char *unit) {
	Stat &stat = _stats[id];
	if (stat.value != value) {
		stat.value = value;
		stat.label->set_text(with_unit(stat.value, unit));
	}
}

void VoxelTerrainEditorTaskIndicator::set_stat(StatID id, int64_t value, int64_t value2, const char *unit) {
	Stat &stat = _stats[id];
	if (stat.value != value || stat.value2 != value2) {
		stat.value = value;
		stat.value2 = value2;
		stat.label->set_text(
				String("{0} / {1}").format(varray(with_unit(stat.value, unit), with_unit(stat.value2, unit))));
	}
}

} // namespace zylann::voxel
