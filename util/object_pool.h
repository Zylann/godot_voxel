#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "core/os/memory.h"
#include <vector>

namespace zylann {

template <class T>
class ObjectPool {
public:
	T *create() {
		if (_objects.empty()) {
			return memnew(T);
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
		for (auto it = _objects.begin(); it != _objects.end(); ++it) {
			memdelete(*it);
		}
	}

private:
	std::vector<T *> _objects;
};

} // namespace zylann

#endif // OBJECT_POOL_H
