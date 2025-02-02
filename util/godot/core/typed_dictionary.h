#ifndef ZN_GODOT_TYPED_DICTIONARY_H
#define ZN_GODOT_TYPED_DICTIONARY_H

#if defined(ZN_GODOT)
#include <core/variant/typed_dictionary.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/typed_dictionary.hpp>
using namespace godot;
#endif

#include "version.h"

#if GODOT_VERSION == 4 && GODOT_VERSION < 4
#define ZN_GODOT_TYPED_DICTIONARY_T(key_type, value_type) Dictionary
#else
#define ZN_GODOT_TYPED_DICTIONARY_T(key_type, value_type) TypedDictionary<key_type, value_type>
#endif

namespace zylann::godot {
using DictionaryIntInt = ZN_GODOT_TYPED_DICTIONARY_T(int64_t, int64_t);
}

#endif // ZN_GODOT_TYPED_DICTIONARY_H
