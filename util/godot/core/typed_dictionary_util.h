#ifndef ZN_GODOT_TYPED_DICTIONARY_UTIL_H
#define ZN_GODOT_TYPED_DICTIONARY_UTIL_H

#include "../../containers/std_unordered_map.h"
#include "typed_dictionary.h"

namespace zylann::godot {

template <typename TSrcKey, typename TSrcValue, typename TDstKey, typename TDstValue>
ZN_GODOT_TYPED_DICTIONARY_T(TDstKey, TDstValue)
to_typed_dictionary_template(const StdUnorderedMap<TSrcKey, TSrcValue> &src_map) {
	ZN_GODOT_TYPED_DICTIONARY_T(int64_t, int64_t) dict;
	for (auto it = src_map.begin(); it != src_map.end(); ++it) {
		dict[it->first] = it->second;
	}
	return dict;
}

inline zylann::godot::DictionaryIntInt to_typed_dictionary(const StdUnorderedMap<uint32_t, uint32_t> &src_map) {
	return to_typed_dictionary_template<uint32_t, uint32_t, int64_t, int64_t>(src_map);
}

} // namespace zylann::godot

#endif // ZN_GODOT_TYPED_DICTIONARY_UTIL_H
