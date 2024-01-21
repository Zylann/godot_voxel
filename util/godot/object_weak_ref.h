#ifndef ZN_GODOT_OBJECT_WEAK_REF_H
#define ZN_GODOT_OBJECT_WEAK_REF_H

#include "classes/object.h"

namespace zylann {

// Holds a weak reference to a Godot object.
// Mainly useful to reference scene tree nodes more safely, because their ownership model is harder to handle with
// pointers, compared to RefCounted objects.
// It is not intented for use with RefCounted objects.
// Warning: if the object can be destroyed by a different thread, this won't be safe to use.
template <typename T>
class ObjectWeakRef {
public:
	void set(T *obj) {
		_id = obj != nullptr ? obj->get_instance_id() : ObjectID();
	}

	T *get() const {
		if (!_id.is_valid()) {
			return nullptr;
		}
		Object *obj = ObjectDB::get_instance(_id);
		if (obj == nullptr) {
			// Could have been destroyed.
			// _node_object_id = ObjectID();
			return nullptr;
		}
		T *tobj = Object::cast_to<T>(obj);
		// We don't expect Godot to re-use the same ObjectID for different objects
		ERR_FAIL_COND_V(tobj == nullptr, nullptr);
		return tobj;
	}

private:
	ObjectID _id;
};

} // namespace zylann

#endif // ZN_GODOT_OBJECT_WEAK_REF_H
