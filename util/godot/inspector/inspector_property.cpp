#include "inspector_property.h"
#include "../../errors.h"
#include "inspector_property_listener.h"

namespace zylann {

// ZN_InspectorProperty::ZN_InspectorProperty() {}

void ZN_InspectorProperty::set_listener(IInspectorPropertyListener *listener, const unsigned int index) {
	_listener = listener;
	_index = index;
}

void ZN_InspectorProperty::set_value(Variant value) {
	ZN_PRINT_ERROR("Not implemented");
}

Variant ZN_InspectorProperty::get_value() const {
	ZN_PRINT_ERROR("Not implemented");
	return Variant();
}

void ZN_InspectorProperty::set_enabled(bool enabled) {
	ZN_PRINT_ERROR("Not implemented");
}

bool ZN_InspectorProperty::is_wide() {
	return false;
}

void ZN_InspectorProperty::emit_changed() {
	// emit_signal(voxel::VoxelStringNames::get_singleton().changed);
	if (_listener != nullptr) {
		_listener->on_property_value_changed(_index, get_value());
	}
}

} // namespace zylann
