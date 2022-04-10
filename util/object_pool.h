#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "memory.h"
#include <vector>

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
	std::vector<T *> _objects;
};

} // namespace zylann

#endif // OBJECT_POOL_H
