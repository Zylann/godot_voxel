#include "godot_thread_helper.h"
#include <godot_cpp/core/class_db.hpp>

namespace zylann {

void ZN_GodotThreadHelper::run() {
	ZN_ASSERT_RETURN(_callback != nullptr);
	_callback(_callback_data);
}

void ZN_GodotThreadHelper::_bind_methods() {
	godot::ClassDB::bind_method(godot::D_METHOD("run"), &ZN_GodotThreadHelper::run);
}

} // namespace zylann
