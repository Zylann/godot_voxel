#ifndef ZN_GODOT_CALLABLE_H
#define ZN_GODOT_CALLABLE_H

#if defined(ZN_GODOT)
#include "version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 6
#include <core/object/callable_method_pointer.h>
#else
#include <core/object/callable_mp.h>
#endif

#endif

#endif // ZN_GODOT_CALLABLE_H
