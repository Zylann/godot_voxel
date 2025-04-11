#include "test_options.h"
#include "../godot/core/dictionary.h"
#include "../godot/core/string.h"
#include "../godot/core/variant.h"
#include "../string/format.h"

namespace zylann::testing {

void parse_string_array(const Dictionary &dict, const char *key, StdVector<StdString> &dst) {
	const Variant includes_v = dict.get(key, Variant());
	if (includes_v.get_type() == Variant::ARRAY) {
		const Array includes_a = includes_v;
		for (int i = 0; i < includes_a.size(); ++i) {
			const Variant include_v = includes_a[i];
			if (include_v.get_type() == Variant::STRING) {
				dst.push_back(zylann::godot::to_std_string(String(include_v)));
			}
		}
	}
}

TestOptions::TestOptions(const Dictionary &options_dict) {
	parse_string_array(options_dict, "includes", _includes);
	parse_string_array(options_dict, "excludes", _excludes);
}

bool TestOptions::can_run_print(const char *test_name) const {
	if (_excludes.size() > 0) {
		for (const StdString &ex : _excludes) {
			if (ex == test_name) {
				ZN_PRINT_VERBOSE(format("Skipping excluded test {}", test_name));
				return false;
			}
		}
	}

	if (_includes.size() > 0) {
		bool found = false;
		for (const StdString &inc : _includes) {
			if (inc == test_name) {
				found = true;
				break;
			}
		}
		if (!found) {
			ZN_PRINT_VERBOSE(format("Skipping non-included test {}", test_name));
			return false;
		}
	}

	print_line(format("Running test `{}`", test_name));

	return true;
}

} // namespace zylann::testing
