#ifndef ZN_GODOT_EDITOR_SPIN_SLIDER_H
#define ZN_GODOT_EDITOR_SPIN_SLIDER_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR == 0
#include <editor/editor_spin_slider.h>
#else
#include <editor/gui/editor_spin_slider.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_spin_slider.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_SPIN_SLIDER_H
