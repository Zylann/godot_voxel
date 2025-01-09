#ifndef ZN_INSPECTOR_PROPERTY_STRING_H
#define ZN_INSPECTOR_PROPERTY_STRING_H

#include "../macros.h"
#include "inspector_property.h"

ZN_GODOT_FORWARD_DECLARE(class LineEdit);

namespace zylann {

class ZN_InspectorPropertyString : public ZN_InspectorProperty {
	GDCLASS(ZN_InspectorPropertyString, ZN_InspectorProperty)
public:
	ZN_InspectorPropertyString();

	void set_value(Variant value) override;
	Variant get_value() const override;

	void set_enabled(bool enabled) override;

private:
	void on_line_edit_focus_entered();
	void on_line_edit_text_changed(String new_text);
	void on_line_edit_text_submitted(String text);
	void on_line_edit_focus_exited();

	static void _bind_methods();

	LineEdit *_line_edit = nullptr;
	String _text;
	bool _ignore_changes = false;
	bool _changed = false;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_STRING_H
