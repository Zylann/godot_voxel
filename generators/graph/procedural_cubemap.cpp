#include "procedural_cubemap.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/cubemap.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/conv.h"
#include "../../util/math/vector2f.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"

namespace zylann::voxel {

void VoxelProceduralCubemap::set_target_resolution(const unsigned int new_resolution) {
	const unsigned int res = math::clamp<unsigned int>(new_resolution, 4, 4096);
	if (res == _target_resolution) {
		return;
	}
	_target_resolution = res;
	_dirty = true;
}

int VoxelProceduralCubemap::get_target_resolution() const {
	return _target_resolution;
}

void VoxelProceduralCubemap::set_target_format(const Format format) {
	if (format == _target_format) {
		return;
	}
	_target_format = format;
	_dirty = true;
}

VoxelProceduralCubemap::Format VoxelProceduralCubemap::get_target_format() const {
	return _target_format;
}

void VoxelProceduralCubemap::set_derivatives_enabled(const bool enabled) {
	if (enabled == _derivatives_enabled) {
		return;
	}
	_derivatives_enabled = true;
	_dirty = true;
}

bool VoxelProceduralCubemap::get_derivatives_enabled() const {
	return _derivatives_enabled;
}

void VoxelProceduralCubemap::set_graph(Ref<pg::VoxelGraphFunction> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelProceduralCubemap::on_graph_changed)
		);
#ifdef TOOLS_ENABLED
		_graph->editor_remove_user(get_instance_id());
#endif
	}

	_graph = graph;

	if (_graph.is_valid()) {
		// Can't compile and update right here... because because Godot loads the resource by setting properties in an
		// unreliable order, so we can only update only when we are sure they were all set. Using call_deferred for this
		// is also not possible because it could be too late.

		_graph->connect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelProceduralCubemap::on_graph_changed)
		);
#ifdef TOOLS_ENABLED
		_graph->editor_add_user(get_instance_id());
#endif
	}

	_dirty = true;
}

Ref<pg::VoxelGraphFunction> VoxelProceduralCubemap::get_graph() const {
	return _graph;
}

void VoxelProceduralCubemap::on_graph_changed() {
	_dirty = true;
}

static Image::Format get_image_format(const VoxelProceduralCubemap::Format src, const bool with_derivatives) {
	switch (src) {
		case VoxelProceduralCubemap::FORMAT_L8:
			return with_derivatives ? Image::FORMAT_RGB8 : Image::FORMAT_L8;
		case VoxelProceduralCubemap::FORMAT_R8:
			return with_derivatives ? Image::FORMAT_RGB8 : Image::FORMAT_R8;
		case VoxelProceduralCubemap::FORMAT_RF:
			return with_derivatives ? Image::FORMAT_RGBF : Image::FORMAT_RF;
		default:
			ZN_PRINT_ERROR("Unhandled format");
			return Image::FORMAT_MAX;
	}
}

void VoxelProceduralCubemap::update() {
	ZN_PROFILE_SCOPE();

	_dirty = false;

	if (_graph.is_null()) {
		ZN_PRINT_VERBOSE("Procedural cubemap didn't update because the graph is null");
		return;
	}
	if (!_graph->is_compiled()) {
		const pg::CompilationResult result = _graph->compile(false);
		if (!result.success) {
			ZN_PRINT_VERBOSE("Procedural cubemap didn't update because the graph doesn't compile");
			return;
		}
	}

	{
		std::shared_ptr<pg::VoxelGraphFunction::CompiledGraph> compiled_graph = _graph->get_compiled_graph();
		if (compiled_graph != nullptr) {
			const int input_count = compiled_graph->runtime.get_input_count();
			const int output_count = compiled_graph->runtime.get_output_count();

			if (input_count != 3) {
				ZN_PRINT_VERBOSE(format(
						"Procedural cubemap didn't update because input count is not 3, found {} instead", input_count
				));
				return;
			}
			if (output_count != 1) {
				ZN_PRINT_VERBOSE(format(
						"Procedural cubemap didn't update because output count is not 1, found {} instead", output_count
				));
				return;
			}
		}
	}

	const Image::Format image_format = get_image_format(_target_format, _derivatives_enabled);
	if (image_format == Image::FORMAT_MAX) {
		return;
	}

	// TODO Handle CPU filtering preservation. Re-pad afterwards? Generate with padding directly?

	if (!is_valid() || get_format() != image_format || get_resolution() != _target_resolution) {
		create(_target_resolution, image_format);
	}

	const int max_chunk_length = 256;

	ZN_ASSERT_RETURN(_target_resolution > 0);
	const float inv_res = 1.f / static_cast<float>(_target_resolution);
	const float derivative_step_pixels = 0.1f;
	const float derivative_step_uv = derivative_step_pixels * inv_res;
	const float inv_derivative_step_pixels = 1.f / derivative_step_pixels;
	const float derivative_quantization_a = _target_format == FORMAT_RF ? 1.f : 0.5f;
	const float derivative_quantization_b = _target_format == FORMAT_RF ? 0.f : 0.5f;

	StdVector<float> x_array;
	StdVector<float> y_array;
	StdVector<float> z_array;

	StdVector<float> r_array;

	x_array.reserve(max_chunk_length);
	y_array.reserve(max_chunk_length);
	z_array.reserve(max_chunk_length);

	r_array.reserve(max_chunk_length);

	for (unsigned int side = 0; side < SIDE_COUNT; ++side) {
		Image &image = get_image(side);

		const Vector2i im_size = image.get_size();
		ZN_ASSERT_RETURN(!Vector2iUtil::is_empty_size(im_size));

		// const Image::Format format = image.get_format();

		const unsigned int im_area = im_size.x * im_size.y;
		const unsigned int last_pixel_index = im_area - 1;

		const Vector2f inv_im_size = Vector2f(1.f) / to_vec2f(im_size);
		unsigned int pixel_start_index = 0;
		unsigned int pixel_index = 0;

		int x0 = 0;
		int y0 = 0;

		for (int y = 0; y < im_size.y; ++y) {
			for (int x = 0; x < im_size.x; ++x, ++pixel_index) {
				// We add 0.5 so we calculate pixel centers
				const Vector2f uv = (Vector2f(x, y) + Vector2f(0.5f)) * inv_im_size;
				{
					const Vector3f cube_pos = get_xyz_from_uv(uv, static_cast<SideIndex>(side));
					const Vector3f sphere_pos = math::normalized(cube_pos);
					x_array.push_back(sphere_pos.x);
					y_array.push_back(sphere_pos.y);
					z_array.push_back(sphere_pos.z);
				}

				if (_derivatives_enabled) {
					{
						const Vector2f uv2(uv.x + derivative_step_uv, uv.y);
						const Vector3f cube_pos2 = get_xyz_from_uv(uv2, static_cast<SideIndex>(side));
						const Vector3f sphere_pos2 = math::normalized(cube_pos2);
						x_array.push_back(sphere_pos2.x);
						y_array.push_back(sphere_pos2.y);
						z_array.push_back(sphere_pos2.z);
					}
					{
						const Vector2f uv3(uv.x, uv.y + derivative_step_uv);
						const Vector3f cube_pos3 = get_xyz_from_uv(uv3, static_cast<SideIndex>(side));
						const Vector3f sphere_pos3 = math::normalized(cube_pos3);
						x_array.push_back(sphere_pos3.x);
						y_array.push_back(sphere_pos3.y);
						z_array.push_back(sphere_pos3.z);
					}
				}

				if (x_array.size() < max_chunk_length && pixel_index != last_pixel_index) {
					continue;
				}

				r_array.resize(x_array.size());

				std::array<Span<const float>, 3> inputs;
				inputs[0] = to_span(x_array);
				inputs[1] = to_span(y_array);
				inputs[2] = to_span(z_array);

				std::array<Span<float>, 1> outputs;
				outputs[0] = to_span(r_array);

				_graph->execute(to_span(inputs), to_span(outputs));

				// TODO Fast-paths. A bit annoying with Godot's CoW mechanism
				// switch (format) {
				// 	default:
				// Slow generic
				unsigned int i = 0;
				while (y0 <= y) {
					const int xend = y0 < y ? _target_resolution : x + 1;
					for (; x0 < xend; ++x0, ++i) {
						if (_derivatives_enabled) {
							const float v = r_array[i++];
							const float v2 = r_array[i++];
							const float v3 = r_array[i];
							const float dx = (v2 - v) * inv_derivative_step_pixels;
							const float dy = (v3 - v) * inv_derivative_step_pixels;
							const float dx_norm = dx * derivative_quantization_a + derivative_quantization_b;
							const float dy_norm = dy * derivative_quantization_a + derivative_quantization_b;
							const Color color(v, dx_norm, dy_norm);
							image.set_pixel(x0, y0, color);
						} else {
							const float v = r_array[i];
							const Color color(v, v, v);
							image.set_pixel(x0, y0, color);
						}
					}
					if (x0 == static_cast<int>(_target_resolution)) {
						++y0;
						x0 = 0;
					} else {
						break;
					}
				}
				// }

				pixel_start_index = pixel_index + 1;

				x_array.clear();
				y_array.clear();
				z_array.clear();
			}
		}

		// image.save_png(String("test_side_{0}.png").format(varray(side)));
	}

	emit_signal(VoxelStringNames::get_singleton().updated);
}

Ref<ZN_Cubemap> VoxelProceduralCubemap::zn_duplicate() const {
	Ref<VoxelProceduralCubemap> d;
	d.instantiate();
	ZN_Cubemap::duplicate_to(**d);
	d->_graph = _graph;
	d->_target_format = _target_format;
	d->_target_resolution = _target_resolution;
	d->_dirty = _dirty;
	d->_derivatives_enabled = _derivatives_enabled;
	return d;
}

void VoxelProceduralCubemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_graph", "graph"), &VoxelProceduralCubemap::set_graph);
	ClassDB::bind_method(D_METHOD("get_graph"), &VoxelProceduralCubemap::get_graph);

	ClassDB::bind_method(
			D_METHOD("set_target_resolution", "resolution"), &VoxelProceduralCubemap::set_target_resolution
	);
	ClassDB::bind_method(
			D_METHOD("set_derivatives_enabled", "enabled"), &VoxelProceduralCubemap::set_derivatives_enabled
	);
	ClassDB::bind_method(D_METHOD("get_derivatives_enabled"), &VoxelProceduralCubemap::get_derivatives_enabled);
	ClassDB::bind_method(D_METHOD("get_target_resolution"), &VoxelProceduralCubemap::get_target_resolution);

	ClassDB::bind_method(D_METHOD("set_target_format", "format"), &VoxelProceduralCubemap::set_target_format);
	ClassDB::bind_method(D_METHOD("get_target_format"), &VoxelProceduralCubemap::get_target_format);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT, "graph", PROPERTY_HINT_RESOURCE_TYPE, pg::VoxelGraphFunction::get_class_static()
			),
			"set_graph",
			"get_graph"
	);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "target_resolution"), "set_target_resolution", "get_target_resolution");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "target_format", PROPERTY_HINT_ENUM, "R8,L8,RF"),
			"set_target_format",
			"get_target_format"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "derivatives_enabled"), "set_derivatives_enabled", "get_derivatives_enabled"
	);

	ADD_SIGNAL(MethodInfo("updated"));
}

} // namespace zylann::voxel
