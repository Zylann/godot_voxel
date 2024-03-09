#ifndef ZN_GODOT_NODE_BUCKETS_STRATEGY_H
#define ZN_GODOT_NODE_BUCKETS_STRATEGY_H

#include "../containers/std_vector.h"
#include "../errors.h"
#include "macros.h"

ZN_GODOT_FORWARD_DECLARE(class Node)

namespace zylann::godot {

// A workaround for the fact Godot is very slow at removing nodes from the scene tree if they have many siblings...
// See https://github.com/godotengine/godot/issues/61929
template <typename TBucket>
class NodeBucketsStrategy {
public:
	static const unsigned int BUCKET_SIZE = 20;

	NodeBucketsStrategy(Node &parent) : _parent(parent) {}

	void add_child(Node *node) {
		ZN_ASSERT(node != nullptr);
		if (_free_buckets.size() == 0) {
			TBucket *bucket = memnew(TBucket);
			_used_buckets.push_back(bucket);
			bucket->add_child(node);
			_parent.add_child(bucket);
		} else {
			TBucket *bucket = _free_buckets.back();
			_free_buckets.pop_back();
			_used_buckets.push_back(bucket);
			bucket->add_child(node);
		}
	}

	void remove_empty_buckets() {
		for (unsigned int i = 0; i < _used_buckets.size();) {
			TBucket *bucket = _used_buckets[i];
			if (bucket->get_child_count() == 0) {
				bucket->queue_free();
				_used_buckets[i] = _used_buckets.back();
				_used_buckets.pop_back();
			} else {
				++i;
			}
		}
	}

private:
	Node &_parent;
	StdVector<TBucket *> _free_buckets;
	StdVector<TBucket *> _used_buckets;
};

} // namespace zylann::godot

#endif // ZN_GODOT_NODE_BUCKETS_STRATEGY_H
