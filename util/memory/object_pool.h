#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "../containers/std_vector.h"
#include "memory.h"

namespace zylann {

template <class T>
class ObjectPool {
public:
	T *create() {
		if (_objects.empty()) {
			return ZN_NEW(T);
		} else {
			T *obj = _objects.back();
			_objects.pop_back();
			return obj;
		}
	}

	void recycle(T *obj) {
		obj->init();
		_objects.push_back(obj);
	}

	~ObjectPool() {
		for (T *obj : _objects) {
			ZN_DELETE(obj);
		}
	}

private:
	StdVector<T *> _objects;
};

} // namespace zylann

#endif // OBJECT_POOL_H
