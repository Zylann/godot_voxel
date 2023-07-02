#include "object.h"
#include "../../hash_funcs.h"
#include "../../profiling.h"
#ifdef ZN_GODOT_EXTENSION
#include "../core/callable.h"
#include "undo_redo.h"
#endif

namespace zylann {

#ifdef TOOLS_ENABLED

void get_property_list(const Object &obj, std::vector<GodotPropertyInfo> &out_properties) {
#if defined(ZN_GODOT)
	List<PropertyInfo> properties;
	obj.get_property_list(&properties, false);
	// I'd like to use ConstIterator since I only read that list but that isn't possible :shrug:
	for (List<PropertyInfo>::Iterator it = properties.begin(); it != properties.end(); ++it) {
		const PropertyInfo property = *it;
		GodotPropertyInfo pi;
		pi.type = property.type;
		pi.name = property.name;
		pi.usage = property.usage;
		out_properties.push_back(pi);
	}
#elif defined(ZN_GODOT_EXTENSION)
	const Array properties = obj.get_property_list();
	const String type_key = "type";
	const String name_key = "name";
	const String usage_key = "usage";
	for (int i = 0; i < properties.size(); ++i) {
		Dictionary d = properties[i];
		GodotPropertyInfo pi;
		pi.type = Variant::Type(int(d[type_key]));
		pi.name = d[name_key];
		pi.usage = d[usage_key];
		out_properties.push_back(pi);
	}
#endif
}

uint64_t get_deep_hash(const Object &obj, uint32_t property_usage, uint64_t hash) {
	ZN_PROFILE_SCOPE();

	hash = hash_djb2_one_64(obj.get_class().hash(), hash);

	std::vector<GodotPropertyInfo> properties;
	get_property_list(obj, properties);

	// I'd like to use ConstIterator since I only read that list but that isn't possible :shrug:
	for (const GodotPropertyInfo &property : properties) {
		if ((property.usage & property_usage) != 0) {
			const Variant value = obj.get(property.name);
			uint64_t value_hash = 0;

			if (value.get_type() == Variant::OBJECT) {
				const Object *obj_value = value.operator Object *();
				if (obj_value != nullptr) {
					value_hash = get_deep_hash(*obj_value, property_usage, hash);
				}

			} else {
				value_hash = value.hash();
			}

			hash = hash_djb2_one_64(value_hash, hash);
		}
	}

	return hash;
}

void set_object_edited(Object &obj) {
#if defined(ZN_GODOT)
	obj.set_edited(true);

#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: Object::set_edited is not exposed, and nested resource saving is an unexplained problem
	// See https://github.com/godotengine/godot-proposals/discussions/7168

	// WARN_PRINT(String("Can't mark {0} as edited for saving, Object.set_edited() is not exposed to GDExtension. You "
	// 				  "will have to manually save using the floppy icon in the inspector.")
	// 				   .format(obj.get_class()));

	// A dirty workaround is to call a method without side-effects with a temporary UndoRedo instance, which should
	// internally call `set_edited` in the editor, if the object is a resource...

	UndoRedo *ur = memnew(UndoRedo);
	ur->create_action("Dummy Action");
	Callable callable = ZN_GODOT_CALLABLE_MP(&obj, Object, is_blocking_signals);
	ur->add_do_method(callable);
	ur->add_undo_method(callable);
	ur->commit_action();
	memdelete(ur);
#endif
}

#endif

} // namespace zylann
