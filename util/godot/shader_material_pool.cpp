#include "shader_material_pool.h"
#include "../errors.h"
#include "../profiling.h"
#include "classes/rendering_server.h"

namespace zylann {

void ShaderMaterialPool::set_template(Ref<ShaderMaterial> tpl) {
	_template_material = tpl;
	_materials.clear();
	_shader_params_cache.clear();

	if (_template_material.is_valid()) {
		Ref<Shader> shader = _template_material->get_shader();

		if (shader.is_valid()) {
			std::vector<GodotShaderParameterInfo> params;
			get_shader_parameter_list(shader->get_rid(), params);

			for (const GodotShaderParameterInfo &pi : params) {
				_shader_params_cache.push_back(pi.name);
			}
		}
	}
}

Ref<ShaderMaterial> ShaderMaterialPool::get_template() const {
	return _template_material;
}

Ref<ShaderMaterial> ShaderMaterialPool::allocate() {
	if (_template_material.is_null() || _template_material->get_shader().is_null()) {
		return Ref<ShaderMaterial>();
	}
	if (!_materials.empty()) {
		Ref<ShaderMaterial> material = _materials.back();
		_materials.pop_back();
		return material;
	}
	ZN_PROFILE_SCOPE();
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(_template_material->get_shader());
	for (const StringName &name : _shader_params_cache) {
		// Note, I don't need to make copies of textures. They are shared (at least those coming from the template
		// material).
		material->set_shader_parameter(name, _template_material->get_shader_parameter(name));
	}
	return material;
}

void ShaderMaterialPool::recycle(Ref<ShaderMaterial> material) {
	ZN_ASSERT_RETURN(material.is_valid());
	ZN_ASSERT_RETURN(_template_material.is_valid());
	ZN_ASSERT_RETURN(material->get_shader() == _template_material->get_shader());
	_materials.push_back(material);
}

Span<const StringName> ShaderMaterialPool::get_cached_shader_uniforms() const {
	return to_span(_shader_params_cache);
}

void copy_shader_params(const ShaderMaterial &src, ShaderMaterial &dst, Span<const StringName> params) {
	// Ref<Shader> shader = src.get_shader();
	// ZN_ASSERT_RETURN(shader.is_valid());
	// Not using `Shader::get_param_list()` because it is not exposed to the script/extension API, and it prepends
	// `shader_params/` to every parameter name, which is slow and not usable for our case.
	// TBH List is slow too, I don't know why Godot uses that for lists of shader params.
	// List<PropertyInfo> properties;
	// RenderingServer::get_singleton()->shader_get_shader_uniform_list(shader->get_rid(), &properties);
	// for (const PropertyInfo &property : properties) {
	// 	dst.set_shader_uniform(property.name, src.get_shader_uniform(property.name));
	// }
	for (unsigned int i = 0; i < params.size(); ++i) {
		const StringName &name = params[i];
		dst.set_shader_parameter(name, src.get_shader_parameter(name));
	}
}

} // namespace zylann
