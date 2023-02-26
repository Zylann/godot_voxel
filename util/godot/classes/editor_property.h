#ifndef ZN_GODOT_EDITOR_PROPERTY_H
#define ZN_GODOT_EDITOR_PROPERTY_H

#if defined(ZN_GODOT)
#include <editor/editor_inspector.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_property.hpp>
using namespace godot;
#endif

#include "../../span.h"

namespace zylann {

// This obscure method is actually used to get the XYZW tinting colors for controls that expose coordinates.
// In modules, this is `_get_property_colors`, but it is not exposed in GDExtension.
Span<const Color> editor_property_get_colors(EditorProperty &self);

class ZN_EditorProperty : public EditorProperty {
	GDCLASS(ZN_EditorProperty, EditorProperty)
public:
#if defined(ZN_GODOT)
	void update_property() override;
#elif defined(ZN_GODOT_EXTENSION)
	void _update_property() override;
#endif

#ifdef ZN_GODOT
protected:
#endif
	// This method is protected in core, but public in GDExtension...
	void _set_read_only(bool p_read_only) override;

protected:
	virtual void _zn_update_property();
	virtual void _zn_set_read_only(bool p_read_only);

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_GODOT_EDITOR_PROPERTY_H
