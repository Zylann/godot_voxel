#include "string.h"
#include <sstream>

namespace zylann::godot {

#ifdef TOOLS_ENABLED

PackedStringArray to_godot(const StdVector<std::string_view> &svv) {
	PackedStringArray psa;
	// Not resizing up-front, because in Godot core writing elements uses different code than GDExtension.
	for (unsigned int i = 0; i < svv.size(); ++i) {
		psa.append(to_godot(svv[i]));
	}
	return psa;
}

PackedStringArray to_godot(const StdVector<StdString> &sv) {
	PackedStringArray psa;
	// Not resizing up-front, because in Godot core writing elements uses different code than GDExtension.
	for (unsigned int i = 0; i < sv.size(); ++i) {
		psa.append(to_godot(sv[i]));
	}
	return psa;
}

#endif

} // namespace zylann::godot

ZN_GODOT_NAMESPACE_BEGIN

zylann::StdStringStream &operator<<(zylann::StdStringStream &ss, GodotStringWrapper s) {
	const CharString cs = s.s.utf8();
	// String has non-explicit constructors from various types making this ambiguous
	const char *ca = cs.get_data();
	ss << ca;
	return ss;
}

ZN_GODOT_NAMESPACE_END
