#ifndef ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H
#define ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/macros.h"
#include "fast_noise_lite_viewer.h"

namespace zylann {

class ZN_FastNoiseLiteEditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(ZN_FastNoiseLiteEditorInspectorPlugin, EditorInspectorPlugin)
public:
	// clang-format wasn't happy at all when defintions were header-only, due to the #ifs. So I moved it in a cpp.

#if defined(ZN_GODOT)
	bool can_handle(Object *p_object) override;
#elif defined(ZN_GODOT_EXTENSION)
	bool _can_handle(const Variant &p_object_v) const override;
#endif

	void ZN_GODOT_UNDERSCORE_PREFIX_IF_EXTENSION(parse_begin)(Object *p_object) override;

private:
	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H
