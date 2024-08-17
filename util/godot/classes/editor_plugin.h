#ifndef ZN_GODOT_EDITOR_PLUGIN_H
#define ZN_GODOT_EDITOR_PLUGIN_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 2
#include <editor/editor_plugin.h>
#else
#include <editor/plugins/editor_plugin.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
// Header includes required due to implementation being required inside the `GDCLASS` macro for virtual methods...
#include "camera_3d.h"
#include "input_event.h"

#include <godot_cpp/classes/editor_plugin.hpp>
using namespace godot;
#endif

namespace zylann::godot {

class ZN_EditorPlugin : public EditorPlugin {
	GDCLASS(ZN_EditorPlugin, EditorPlugin)
public:
#if defined(ZN_GODOT)
	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;
	EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;
	void save_external_data() override;

#elif defined(ZN_GODOT_EXTENSION)
	bool _handles(Object *p_object) const override;
	void _edit(Object *p_object) override;
	void _make_visible(bool visible) override;
	int32_t _forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;
	void _save_external_data() override;
#endif

protected:
	virtual bool _zn_handles(const Object *p_object) const;
	virtual void _zn_edit(Object *p_object);
	virtual void _zn_make_visible(bool visible);
	virtual EditorPlugin::AfterGUIInput _zn_forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event);
	virtual void _zn_save_external_data();

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}
};

} // namespace zylann::godot

#endif // ZN_GODOT_EDITOR_PLUGIN_H
