#ifndef ZN_INSPECTOR_PROPERTY_UNHANDLED_H
#define ZN_INSPECTOR_PROPERTY_UNHANDLED_H

#include "../macros.h"
#include "inspector_property.h"

ZN_GODOT_FORWARD_DECLARE(class Label);

namespace zylann {

class ZN_InspectorPropertyUnhandled : public ZN_InspectorProperty {
	GDCLASS(ZN_InspectorPropertyUnhandled, ZN_InspectorProperty)
public:
	ZN_InspectorPropertyUnhandled();

	void set_value(Variant value) override;
	Variant get_value() const override;

	void set_enabled(bool enabled) override;

private:
	static void _bind_methods();

	Label *_label = nullptr;
	Variant _value;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_UNHANDLED_H
