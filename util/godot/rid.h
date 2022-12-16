#ifndef ZN_GODOT_RID_H
#define ZN_GODOT_RID_H

#if defined(ZN_GODOT)
#include <core/templates/rid.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/rid.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_RID_H
