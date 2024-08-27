#include "voxel_instance_emitter.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/typed_array.h"
#include "../../util/string/format.h"
#include "array_utils.h"
#include "voxel_instancer.h"

namespace zylann::voxel {

void VoxelInstanceEmitter::set_lod_index(int index) {
	ZN_ASSERT_RETURN(index >= 0 && index < VoxelInstancer::MAX_LOD);
	const uint8_t new_index = index;
	if (new_index == _lod_index) {
		return;
	}
	const uint8_t prev = _lod_index;
	_lod_index = index;

	for (IInstanceEmitterListener *listener : _listeners) {
		listener->on_emitter_lod_index_changed(this, prev, new_index);
	}
}

int VoxelInstanceEmitter::get_lod_index() const {
	return _lod_index;
}

void VoxelInstanceEmitter::set_generator(Ref<VoxelInstanceGenerator> generator) {
	if (generator == _generator) {
		return;
	}

	if (_generator.is_valid()) {
		_generator->disconnect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelInstanceEmitter::on_generator_changed)
		);
	}

	_generator = generator;

	if (_generator.is_valid()) {
		_generator->connect(
				VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelInstanceEmitter::on_generator_changed)
		);
	}

	for (IInstanceEmitterListener *listener : _listeners) {
		listener->on_emitter_generator_changed(this);
	}
}

Ref<VoxelInstanceGenerator> VoxelInstanceEmitter::get_generator() const {
	return _generator;
}

void VoxelInstanceEmitter::on_generator_changed() {
	for (IInstanceEmitterListener *listener : _listeners) {
		listener->on_emitter_generator_changed(this);
	}
}

void VoxelInstanceEmitter::add_listener(IInstanceEmitterListener *listener) {
	ZN_ASSERT_RETURN(listener != nullptr);
	ZN_ASSERT_RETURN(!contains(_listeners, listener));
	_listeners.push_back(listener);
}

void VoxelInstanceEmitter::remove_listener(const IInstanceEmitterListener *listener) {
	size_t i;
	auto it = std::find(_listeners.begin(), _listeners.end(), listener);
	ZN_ASSERT_RETURN(it != _listeners.end());
	_listeners.erase(it);
}

Span<const Ref<VoxelInstanceLibraryItem>> VoxelInstanceEmitter::get_items() const {
	return to_span_const(_items);
}

TypedArray<VoxelInstanceLibraryItem> VoxelInstanceEmitter::_b_get_items() {
	TypedArray<VoxelInstanceLibraryItem> items;
	zylann::godot::copy_to(items, to_span_const(_items));
	return items;
}

void VoxelInstanceEmitter::_b_set_items(TypedArray<VoxelInstanceLibraryItem> array) {
	// TODO Don't diff if there are no listeners
	StdVector<Ref<VoxelInstanceLibraryItem>> added;
	StdVector<Ref<VoxelInstanceLibraryItem>> removed;

	diff_set_array(_items, array, added, removed);

	for (const Ref<VoxelInstanceLibraryItem> &item : added) {
		for (IInstanceEmitterListener *listener : _listeners) {
			listener->on_emitter_item_added(this, item);
		}
	}
	for (const Ref<VoxelInstanceLibraryItem> &item : removed) {
		for (IInstanceEmitterListener *listener : _listeners) {
			listener->on_emitter_item_removed(this, item);
		}
	}
}

void VoxelInstanceEmitter::_bind_methods() {
	using Self = VoxelInstanceEmitter;

	ClassDB::bind_method(D_METHOD("set_lod_index", "index"), &Self::set_lod_index);
	ClassDB::bind_method(D_METHOD("get_lod_index"), &Self::get_lod_index);

	ClassDB::bind_method(D_METHOD("set_generator", "generator"), &Self::set_generator);
	ClassDB::bind_method(D_METHOD("get_generator"), &Self::get_generator);

	ClassDB::bind_method(D_METHOD("set_items", "items"), &Self::_b_set_items);
	ClassDB::bind_method(D_METHOD("get_items"), &Self::_b_get_items);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_index"), "set_lod_index", "get_lod_index");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "generator"), "set_generator", "get_generator");

	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY,
					"items",
					PROPERTY_HINT_ARRAY_TYPE,
					MAKE_RESOURCE_TYPE_HINT(VoxelInstanceLibraryItem::get_class_static())
			),
			"set_items",
			"get_items"
	);
}

} // namespace zylann::voxel
