#ifndef ZYLANN_EDITOR_PROPERTY_AABB_H
#define ZYLANN_EDITOR_PROPERTY_AABB_H

#include "../../util/fixed_array.h"
#include "../../util/godot/editor_property.h"
#include "../../util/godot/editor_spin_slider.h"
#include "../../util/macros.h"

namespace zylann {

// Alternative to the default AABB editor which presents it as a minimum and maximum point
class EditorPropertyAABBMinMax : public EditorProperty {
	GDCLASS(EditorPropertyAABBMinMax, EditorProperty);

public:
	EditorPropertyAABBMinMax();

	void setup(double p_min, double p_max, double p_step, bool p_no_slider, const String &p_suffix = String());

	virtual void ZN_GODOT_UNDERSCORE_PREFIX_IF_EXTENSION(update_property)() override;

protected:
// TODO GDX: `EditorProperty::_set_read_only` is not exposed!
#ifdef ZN_GODOT
	virtual void _set_read_only(bool p_read_only) override;
#endif

private:
	void _on_value_changed(double p_val);
	void _notification(int p_what);

	static void _bind_methods();

	FixedArray<EditorSpinSlider *, 6> _spinboxes;
	bool _ignore_value_change = false;
};

} // namespace zylann

#endif // ZYLANN_EDITOR_PROPERTY_AABB_H
