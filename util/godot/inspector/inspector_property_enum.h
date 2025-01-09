#ifndef ZN_INSPECTOR_PROPERTY_ENUM_H
#define ZN_INSPECTOR_PROPERTY_ENUM_H

#include "../../containers/span.h"
#include "../macros.h"
#include "inspector_property.h"

ZN_GODOT_FORWARD_DECLARE(class OptionButton);

namespace zylann {

class ZN_InspectorPropertyEnum : public ZN_InspectorProperty {
	GDCLASS(ZN_InspectorPropertyEnum, ZN_InspectorProperty)
public:
	ZN_InspectorPropertyEnum();

	void set_names(const PackedStringArray &names);

	void set_value(Variant value) override;
	Variant get_value() const override;

	void set_enabled(bool enabled) override;

private:
	void on_option_button_item_selected(int index);

	static void _bind_methods();

	OptionButton *_option_button = nullptr;
	bool _ignore_changes = false;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_ENUM_H
