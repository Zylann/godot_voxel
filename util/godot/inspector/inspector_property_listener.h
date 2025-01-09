#ifndef ZN_INSPECTOR_PROPERTY_LISTENER_H
#define ZN_INSPECTOR_PROPERTY_LISTENER_H

#include "../macros.h"

ZN_GODOT_FORWARD_DECLARE(class Variant);

namespace zylann {

class IInspectorPropertyListener {
public:
	virtual ~IInspectorPropertyListener() {}
	virtual void on_property_value_changed(const unsigned int index, const Variant &value) = 0;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_LISTENER_H
