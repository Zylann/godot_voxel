#include "multimesh.h"

namespace zylann {

int get_visible_instance_count(const MultiMesh &mm) {
	int visible_count = mm.get_visible_instance_count();
	if (visible_count == -1) {
		visible_count = mm.get_instance_count();
	}
	return visible_count;
}

} // namespace zylann
