#ifndef ZN_SHADER_MATERIAL_POOL_H
#define ZN_SHADER_MATERIAL_POOL_H

#include "../span.h"
#include "classes/shader_material.h"
#include <vector>

namespace zylann {

// Reasons to pool numerous copies of the same ShaderMaterial:
// - In the editor, the Shader `changed` signal is connected even if they aren't editable, which makes the shader manage
//   a huge list of connections to "listening" materials, making insertion/removal super slow.
// - The generic `Resource.duplicate()` behavior is super slow. 95% of the time is spent NOT setting shader params
//   (getting property list in a LINKED LIST, many of which have to reach the fallback for "generated" ones, allocation,
//   resolution of assignments using variant `set` function...).
// - Allocating the object alone takes a bit of time
// TODO Next step could be to make a thin wrapper and use RenderingServer directly?
class ShaderMaterialPool {
public:
	void set_template(Ref<ShaderMaterial> tpl);
	Ref<ShaderMaterial> get_template() const;

	Ref<ShaderMaterial> allocate();
	void recycle(Ref<ShaderMaterial> material);

	// Materials have a cache too, but this one is even more direct
	Span<const StringName> get_cached_shader_uniforms() const;

private:
	Ref<ShaderMaterial> _template_material;
	std::vector<StringName> _shader_params_cache;
	std::vector<Ref<ShaderMaterial>> _materials;
};

void copy_shader_params(const ShaderMaterial &src, ShaderMaterial &dst, Span<const StringName> params);

} // namespace zylann

#endif // ZN_SHADER_MATERIAL_POOL_H
