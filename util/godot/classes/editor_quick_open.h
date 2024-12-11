#ifndef ZN_GODOT_EDITOR_QUICK_OPEN_H
#define ZN_GODOT_EDITOR_QUICK_OPEN_H

#if defined(ZN_GODOT)

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 3
#include <editor/editor_quick_open.h>
#else
#include <editor/gui/editor_quick_open_dialog.h>
#endif

// TODO GDX: EditorQuickOpen is not exposed!
// #elif defined(ZN_GODOT_EXTENSION)
// #include <godot_cpp/classes/editor_quick_open.hpp>
// using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_QUICK_OPEN_H
