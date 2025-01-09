#ifndef ZN_INSPECTOR_H
#define ZN_INSPECTOR_H

#include "../../containers/std_vector.h"
#include "../classes/scroll_container.h"
#include "../macros.h"
#include "inspector_property_listener.h"

ZN_GODOT_FORWARD_DECLARE(class GridContainer);
ZN_GODOT_FORWARD_DECLARE(class VBoxContainer);

namespace zylann {

class ZN_InspectorProperty;

// Custom implementation of a key/value object inspector, with only what I need, and custom tweaks.
// See also https://github.com/godotengine/godot-proposals/issues/7010
class ZN_Inspector : public ScrollContainer, public IInspectorPropertyListener {
public:
	ZN_Inspector();

	void clear();
	void set_target_object(const Object *obj);
	void set_target_index(const int i);
	void add_indexed_property(
			const String &name,
			const StringName &setter,
			const StringName &getter,
			const Variant &hint = Variant(),
			const bool editable = true
	);
	// void set_property_editable(const unsigned int i, const bool editable);

private:
	void on_property_value_changed(const unsigned int index, const Variant &value) override;

	static void _bind_methods();

	struct Property {
		Label *label = nullptr;
		ZN_InspectorProperty *control = nullptr;
		StringName getter;
		StringName setter;
		bool indexed = false;
	};

	VBoxContainer *_vb_container = nullptr;
	StdVector<Property> _properties;
	ObjectID _target_object;
	int _target_index = 0;
};

} // namespace zylann

#endif // ZN_INSPECTOR_H
