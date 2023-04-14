#ifndef ZN_GODOT_EDITOR_FILE_DIALOG_H
#define ZN_GODOT_EDITOR_FILE_DIALOG_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR == 0
#include <editor/editor_file_dialog.h>
#else
#include <editor/gui/editor_file_dialog.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_file_dialog.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_FILE_DIALOG_H
