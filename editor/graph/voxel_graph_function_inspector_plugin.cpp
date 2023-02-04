#include "voxel_graph_function_inspector_plugin.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/h_separator.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/classes/v_separator.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "voxel_graph_editor_plugin.h"

namespace zylann::voxel {

using namespace pg;

bool VoxelGraphFunctionInspectorPlugin::_zn_can_handle(const Object *obj) const {
	return Object::cast_to<VoxelGraphFunction>(obj) != nullptr;
}

static VBoxContainer *create_ports_control(Span<const VoxelGraphFunction::Port> ports, String title) {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Label *label = memnew(Label);
	label->set_text(title);
	vb->add_child(label);

	vb->add_child(memnew(HSeparator));

	for (const VoxelGraphFunction::Port &port : ports) {
		label = memnew(Label);
		label->set_text(get_port_display_name(port));
		vb->add_child(label);
	}

	return vb;
}

bool VoxelGraphFunctionInspectorPlugin::_zn_parse_property(Object *p_object, const Variant::Type p_type,
		const String &p_path, const PropertyHint p_hint, const String &p_hint_text,
		const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "input_definitions") {
		VoxelGraphFunction *graph = Object::cast_to<VoxelGraphFunction>(p_object);

		Span<const VoxelGraphFunction::Port> inputs = graph->get_input_definitions();
		Span<const VoxelGraphFunction::Port> outputs = graph->get_output_definitions();

		HBoxContainer *hb = memnew(HBoxContainer);
		hb->add_child(create_ports_control(inputs, ZN_TTR("Inputs")));
		hb->add_child(memnew(VSeparator));
		hb->add_child(create_ports_control(outputs, ZN_TTR("Outputs")));

		add_custom_control(hb);

		Ref<VoxelGraphFunction> graph_ref(graph);

		Button *edit_io_button = memnew(Button);
		edit_io_button->set_text(ZN_TTR("Edit inputs/outputs..."));
#if defined(ZN_GODOT)
		edit_io_button->connect("pressed",
				ZN_GODOT_CALLABLE_MP(this, VoxelGraphFunctionInspectorPlugin, _on_edit_io_button_pressed)
						.bind(graph_ref));
#elif defined(ZN_GODOT_EXTENSION)
		// TODO GDX: `Callable::bind()` isn't implemented in GodotCpp
		// See https://github.com/godotengine/godot-cpp/issues/802
		ZN_PRINT_ERROR("`Callable::bind()` isn't implemented in GodotCpp! Can't edit function I/O.");
#endif

		add_custom_control(edit_io_button);

		return true;

	} else if (p_path == "output_definitions") {
		return true;
	}

	return false;
}

void VoxelGraphFunctionInspectorPlugin::_on_edit_io_button_pressed(Ref<VoxelGraphFunction> graph) {
	ERR_FAIL_COND(_listener == nullptr);
	_listener->edit_ios(graph);
}

void VoxelGraphFunctionInspectorPlugin::set_listener(VoxelGraphEditorPlugin *plugin) {
	_listener = plugin;
}

void VoxelGraphFunctionInspectorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_edit_io_button_pressed", "graph"),
			&VoxelGraphFunctionInspectorPlugin::_on_edit_io_button_pressed);
}

} // namespace zylann::voxel
