#include "editor_help.h"

namespace zylann::godot {
namespace EditorHelpUtility {

String get_method_description(const String &class_name, const String &method_name) {
#if defined(ZN_GODOT)
	const DocTools *doc = EditorHelp::get_doc_data();
	ERR_FAIL_COND_V(doc == nullptr, "<Error>");

	const DocData::ClassDoc *class_doc = doc->class_list.getptr(class_name);
	if (class_doc == nullptr) {
		return "<Class not found>";
	}
	for (const DocData::MethodDoc &method_doc : class_doc->methods) {
		if (method_doc.name == method_name) {
			return method_doc.description;
		}
	}
	return "<Method not found>";

#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: Need access to doc data in GDExtension
	return "<Description not available; Godot doesn't expose docs to GDExtension>";
#endif
}

} // namespace EditorHelpUtility
} // namespace zylann::godot
