#include "voxel_library.h"

VoxelLibrary::VoxelLibrary() :
		Resource(),
		_atlas_size(1) {
}

VoxelLibrary::~VoxelLibrary() {
	// Handled with a WeakRef
	//	for (unsigned int i = 0; i < MAX_VOXEL_TYPES; ++i) {
	//		if (_voxel_types[i].is_valid()) {
	//			_voxel_types[i]->set_library(NULL);
	//		}
	//	}
}

unsigned int VoxelLibrary::get_voxel_count() const {
	return _voxel_types.size();
}

void VoxelLibrary::set_voxel_count(unsigned int type_count) {
	ERR_FAIL_COND(type_count > MAX_VOXEL_TYPES);
	if (type_count == _voxel_types.size()) {
		return;
	}
	// Note: a smaller size may cause a loss of data
	_voxel_types.resize(type_count);
	_change_notify();
}

void VoxelLibrary::load_default() {
	set_voxel_count(2);
	create_voxel(0, "air")->set_transparent(true);
	create_voxel(1, "solid")
			->set_transparent(false)
			->set_geometry_type(Voxel::GEOMETRY_CUBE);
}

// TODO Add a way to add voxels

bool VoxelLibrary::_set(const StringName &p_name, const Variant &p_value) {

	//	if(p_name == "voxels/max") {

	//		int v = p_value;
	//		_max_count = CLAMP(v, 0, MAX_VOXEL_TYPES);
	//		for(int i = _max_count; i < MAX_VOXEL_TYPES; ++i) {
	//			_voxel_types[i] = Ref<Voxel>();
	//			return true;
	//		}

	//	} else
	if (p_name.operator String().begins_with("voxels/")) {

		unsigned int idx = p_name.operator String().get_slicec('/', 1).to_int();

		ERR_FAIL_INDEX_V(idx, MAX_VOXEL_TYPES, false);

		if (idx >= _voxel_types.size()) {
			_voxel_types.resize(idx + 1);
		}

		Ref<Voxel> voxel = p_value;
		_voxel_types[idx] = voxel;
		if (voxel.is_valid()) {
			voxel->set_library(Ref<VoxelLibrary>(this));
			voxel->set_id(idx);
		}

		// Note: if the voxel is set to null, we could set the previous one's library reference to null.
		// however Voxels use a weak reference, so it's not really needed
		return true;
	}

	return false;
}

bool VoxelLibrary::_get(const StringName &p_name, Variant &r_ret) const {

	//	if(p_name == "voxels/max") {

	//		r_ret = _max_count;
	//		return true;

	//	} else
	if (p_name.operator String().begins_with("voxels/")) {

		unsigned int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx < _voxel_types.size()) {
			r_ret = _voxel_types[idx];
			return true;
		}
	}

	return false;
}

void VoxelLibrary::_get_property_list(List<PropertyInfo> *p_list) const {
	for (unsigned int i = 0; i < _voxel_types.size(); ++i) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, "voxels/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "Voxel"));
	}
}

void VoxelLibrary::set_atlas_size(int s) {
	ERR_FAIL_COND(s <= 0);
	_atlas_size = s;
}

Ref<Voxel> VoxelLibrary::create_voxel(unsigned int id, String name) {
	ERR_FAIL_COND_V(id >= _voxel_types.size(), Ref<Voxel>());
	Ref<Voxel> voxel(memnew(Voxel));
	voxel->set_library(Ref<VoxelLibrary>(this));
	voxel->set_id(id);
	voxel->set_voxel_name(name);
	_voxel_types[id] = voxel;
	return voxel;
}

void VoxelLibrary::_bind_methods() {

	ClassDB::bind_method(D_METHOD("create_voxel", "id", "name"), &VoxelLibrary::create_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel", "id"), &VoxelLibrary::_b_get_voxel);

	ClassDB::bind_method(D_METHOD("set_atlas_size", "square_size"), &VoxelLibrary::set_atlas_size);
	ClassDB::bind_method(D_METHOD("get_atlas_size"), &VoxelLibrary::get_atlas_size);

	ClassDB::bind_method(D_METHOD("set_voxel_count", "count"), &VoxelLibrary::set_voxel_count);
	ClassDB::bind_method(D_METHOD("get_voxel_count"), &VoxelLibrary::get_voxel_count);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "atlas_size"), "set_atlas_size", "get_atlas_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "voxel_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_voxel_count", "get_voxel_count");

	BIND_CONSTANT(MAX_VOXEL_TYPES);
}

Ref<Voxel> VoxelLibrary::_b_get_voxel(unsigned int id) {
	ERR_FAIL_COND_V(id >= _voxel_types.size(), Ref<Voxel>());
	return _voxel_types[id];
}
