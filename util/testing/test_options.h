#ifndef ZN_TEST_OPTIONS_H
#define ZN_TEST_OPTIONS_H

#include "../containers/std_vector.h"
#include "../godot/macros.h"
#include "../string/std_string.h"

ZN_GODOT_FORWARD_DECLARE(class Dictionary);

namespace zylann::testing {

class TestOptions {
public:
	TestOptions() {}
	TestOptions(const Dictionary &options_dict);

	bool can_run_print(const char *test_name) const;

private:
	StdVector<StdString> _excludes;
	StdVector<StdString> _includes;
};

} // namespace zylann::testing

#endif // ZN_TEST_OPTIONS_H
