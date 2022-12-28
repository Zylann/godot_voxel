#include "rendering_server.h"

namespace zylann {

void get_shader_parameter_list(const RID &shader_rid, std::vector<GodotShaderParameterInfo> &out_parameters) {
#if defined(ZN_GODOT)
	List<PropertyInfo> params;
	RenderingServer::get_singleton()->get_shader_parameter_list(shader_rid, &params);
	// I'd like to use ConstIterator since I only read that list but that isn't possible :shrug:
	for (List<PropertyInfo>::Iterator it = params.begin(); it != params.end(); ++it) {
		const PropertyInfo property = *it;
		GodotShaderParameterInfo pi;
		pi.type = property.type;
		pi.name = property.name;
		out_parameters.push_back(pi);
	}

#elif defined(ZN_GODOT_EXTENSION)
	const Array properties = RenderingServer::get_singleton()->get_shader_parameter_list(shader_rid);
	const String type_key = "type";
	const String name_key = "name";
	for (int i = 0; i < properties.size(); ++i) {
		Dictionary d = properties[i];
		GodotShaderParameterInfo pi;
		pi.type = Variant::Type(int(d[type_key]));
		pi.name = d[name_key];
		out_parameters.push_back(pi);
	}
#endif
}

} // namespace zylann
