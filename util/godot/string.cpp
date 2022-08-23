#include "string.h"
#include <sstream>

namespace zylann {

#ifdef TOOLS_ENABLED

PackedStringArray to_godot(const std::vector<std::string_view> &svv) {
	PackedStringArray psa;
	psa.resize(svv.size());
	for (unsigned int i = 0; i < svv.size(); ++i) {
		psa.write[i] = to_godot(svv[i]);
	}
	return psa;
}

PackedStringArray to_godot(const std::vector<std::string> &sv) {
	PackedStringArray psa;
	psa.resize(sv.size());
	for (unsigned int i = 0; i < sv.size(); ++i) {
		psa.write[i] = to_godot(sv[i]);
	}
	return psa;
}

#endif

} // namespace zylann

std::stringstream &operator<<(std::stringstream &ss, GodotStringWrapper s) {
	const CharString cs = s.s.utf8();
	// String has non-explicit constructors from various types making this ambiguous
	ss.std::stringstream::operator<<(cs.get_data());
	return ss;
}
