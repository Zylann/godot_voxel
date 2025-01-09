#ifndef ZN_INSPECTOR_PROPERTY_H
#define ZN_INSPECTOR_PROPERTY_H

#include "../classes/h_box_container.h"

namespace zylann {

class IInspectorPropertyListener;

class ZN_InspectorProperty : public HBoxContainer {
	GDCLASS(ZN_InspectorProperty, HBoxContainer)
public:
	// ZN_InspectorProperty();

	void set_listener(IInspectorPropertyListener *listener, const unsigned int index);

	virtual void set_value(Variant value);
	virtual Variant get_value() const;

	virtual void set_enabled(bool enabled);

	virtual bool is_wide();

protected:
	void emit_changed();

private:
	static void _bind_methods() {}

	IInspectorPropertyListener *_listener = nullptr;
	int _index = -1;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_H
