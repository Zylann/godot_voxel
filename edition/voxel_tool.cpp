#include "voxel_tool.h"
#include "../terrain/voxel_lod_terrain.h"
#include "../util/macros.h"
#include "../voxel_buffer.h"

Vector3 VoxelRaycastResult::_b_get_position() const {
	return position.to_vec3();
}

Vector3 VoxelRaycastResult::_b_get_previous_position() const {
	return previous_position.to_vec3();
}

void VoxelRaycastResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &VoxelRaycastResult::_b_get_position);
	ClassDB::bind_method(D_METHOD("get_previous_position"), &VoxelRaycastResult::_b_get_previous_position);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "previous_position"), "", "get_previous_position");
}

//----------------------------------------

void VoxelTool::set_value(uint64_t val) {
	_value = val;
}

uint64_t VoxelTool::get_value() const {
	return _value;
}

void VoxelTool::set_eraser_value(uint64_t value) {
	_eraser_value = value;
}

uint64_t VoxelTool::get_eraser_value() const {
	return _eraser_value;
}

void VoxelTool::set_channel(int channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
}

int VoxelTool::get_channel() const {
	return _channel;
}

void VoxelTool::set_mode(Mode mode) {
	_mode = mode;
}

VoxelTool::Mode VoxelTool::get_mode() const {
	return _mode;
}

Ref<VoxelRaycastResult> VoxelTool::raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) {
	ERR_PRINT("Not implemented");
	return Ref<VoxelRaycastResult>();
}

uint64_t VoxelTool::get_voxel(Vector3i pos) {
	return _get_voxel(pos);
}

float VoxelTool::get_voxel_f(Vector3i pos) {
	return _get_voxel_f(pos);
}

void VoxelTool::set_voxel(Vector3i pos, uint64_t v) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel(pos, v);
	_post_edit(box);
}

void VoxelTool::set_voxel_f(Vector3i pos, float v) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel_f(pos, v);
	_post_edit(box);
}

void VoxelTool::do_point(Vector3i pos) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		return;
	}
	if (_channel == VoxelBuffer::CHANNEL_SDF) {
		_set_voxel_f(pos, _mode == MODE_REMOVE ? 1.0 : -1.0);
	} else {
		_set_voxel(pos, _mode == MODE_REMOVE ? _eraser_value : _value);
	}
	_post_edit(box);
}

void VoxelTool::do_line(Vector3i begin, Vector3i end) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::do_circle(Vector3i pos, int radius, Vector3i direction) {
	ERR_PRINT("Not implemented");
}

uint64_t VoxelTool::_get_voxel(Vector3i pos) {
	ERR_PRINT("Not implemented");
	return 0;
}

float VoxelTool::_get_voxel_f(Vector3i pos) {
	ERR_PRINT("Not implemented");
	return 0;
}

void VoxelTool::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::_set_voxel_f(Vector3i pos, float v) {
	ERR_PRINT("Not implemented");
}

// TODO May be worth using VoxelBuffer::read_write_action() in the future with a lambda,
// so we avoid the burden of going through get/set, validation and rehash access to blocks.
// Would work well by avoiding virtual as well using a specialized implementation.

namespace {
inline float sdf_blend(float src_value, float dst_value, VoxelTool::Mode mode) {
	float res;
	switch (mode) {
		case VoxelTool::MODE_ADD:
			// Union
			res = min(src_value, dst_value);
			break;

		case VoxelTool::MODE_REMOVE:
			// Relative complement (or difference)
			res = max(1.f - src_value, dst_value);
			break;

		case VoxelTool::MODE_SET:
			res = src_value;
			break;

		default:
			res = 0;
			break;
	}
	return res;
}
} // namespace

void VoxelTool::do_sphere(Vector3 center, float radius) {
	Rect3i box(Vector3i(center) - Vector3i(Math::floor(radius)), Vector3i(Math::ceil(radius) * 2));

	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBuffer::CHANNEL_SDF) {
		box.for_each_cell([this, center, radius](Vector3i pos) {
			float d = pos.to_vec3().distance_to(center) - radius;
			_set_voxel_f(pos, sdf_blend(d, get_voxel_f(pos), _mode));
		});

	} else {
		int value = _mode == MODE_REMOVE ? _eraser_value : _value;

		box.for_each_cell([this, center, radius, value](Vector3i pos) {
			float d = pos.to_vec3().distance_to(center);
			if (d <= radius) {
				_set_voxel(pos, value);
			}
		});
	}

	_post_edit(box);
}

//Note that the name is a minomer: If its not convex it still works
class ConvexChecker {
	public:
		ConvexChecker(PoolVector3Array convex_points) {
			if (convex_points.size() % 3!=0) {
				ERR_PRINT("Input array for convex checker need to be a multiple of 3!");
				return;
			}
			int num_planes = convex_points.size() / 3;
			//Resize arrays
			_normals.resize(num_planes);
			_normals_dot_r0.resize(num_planes);

			//Determine side of plane of centerpoint
			Vector3 sum_of_coords = Vector3(0,0,0);
			for (int i = 0; i < convex_points.size(); i++) {
				sum_of_coords += convex_points[i];
			}
			Vector3 center_of_shape = sum_of_coords/float(convex_points.size());
			

			for (int i = 0; i <num_planes; i++) {
				//Get normals
				int offset = i * 3;
				Vector3 p1 = convex_points[offset+0];
				Vector3 p2 = convex_points[offset+1];
				Vector3 p3 = convex_points[offset+2];
				_normals.set(i,(p1-p2).cross(p1-p3));
				
				//n.r = n.r0 for point on plane
				//Get value of n.r0
				_normals_dot_r0.set(i,_normals[i].dot(convex_points[i]));
				//Make sure all the normals will give a positive value when testing a point inside the convex shape
				if (_normals[i].dot(center_of_shape) - _normals_dot_r0[i] < 0) {
					// print_line("Wrong positivity index:");
					// print_line(itos(i));
					_normals.set(i,-_normals[i]);
				}
				//Test forcefully set normals
				// _normals.set(0,Vector3(-10,0,1));
				// _normals.set(1,Vector3(10,0,1));
				// print_line("n1");
				// print_line(rtos(_normals[0].x));
				// print_line(rtos(_normals[0].y));
				// print_line(rtos(_normals[0].z));
				// print_line("n2");
				// print_line(rtos(_normals[1].x));
				// print_line(rtos(_normals[1].y));
				// print_line(rtos(_normals[1].z));
				// print_line("dot1");
				// print_line(rtos(_normals_dot_r0[0]));
				// print_line("dot2");
				// print_line(rtos(_normals_dot_r0[1]));
			};

		};

		bool check_point_in_shape(Vector3 point) {
			for (int i = 0; i < _normals.size(); ++i) {
				if (_normals[i].dot(point) - _normals_dot_r0[i] < 0) {
					return false;
				}
			}
			return true;
		};

	protected:
		PoolVector3Array _normals = PoolVector3Array();
		PoolRealArray _normals_dot_r0 = PoolRealArray();
};

//Please supply angle in radians
Vector3 rotate_point(Vector3 center, Vector3 point, float angle) {
	float rotatedX = Math::cos(angle) * (point.x - center.x) - Math::sin(angle) * (point.z-center.z) + center.x;
	float rotatedZ = Math::sin(angle) * (point.x - center.x) + Math::cos(angle) * (point.z-center.z) + center.z;
	return Vector3(rotatedX,point.y, rotatedZ);
}

//WARNING: I am not sure if my angle is wrong here or wrong in the game itself
void VoxelTool::do_ravine(Vector3i center, float angle) {

	Rect3i ravine_box(Vector3i(center.x,center.y,center.z)- Vector3i(10,10,10),Vector3i(20));

	if (!is_area_editable(ravine_box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	PoolVector3Array ravine_points = PoolVector3Array();
	//right side
	ravine_points.push_back(Vector3(1,0,10));
	ravine_points.push_back(Vector3(0,0,0));
	ravine_points.push_back(Vector3(0,-1,0));
	//left side
	ravine_points.push_back(Vector3(-1,0,10));
	ravine_points.push_back(Vector3(0,0,0));
	ravine_points.push_back(Vector3(0,-1,0));

	for (int i = 0; i < ravine_points.size(); i++) {
		Vector3 vec3_center = Vector3(center.x,center.y,center.z);
		ravine_points.set(i,rotate_point(Vector3(0,0,0),ravine_points[i],angle));
		// ravine_points.set(i,ravine_points[i] + vec3_center);
		print_line("cur ind:");
		print_line(itos(i));
		print_line("Now at position:");
		print_line(rtos(ravine_points[i].x));
		print_line(rtos(ravine_points[i].y));
		print_line(rtos(ravine_points[i].z));
		
	}

	//Make a ravine shaped convex checker
	ConvexChecker ravine_checker = ConvexChecker(ravine_points);

	//No SDF blending for now
	ravine_box.for_each_cell([this,center,angle,&ravine_checker] (Vector3i pos) {
		// _set_voxel_f(pos,1.0);
		Vector3 vec3_pos = Vector3(pos.x,pos.y,pos.z);
		if (ravine_checker.check_point_in_shape(vec3_pos)) {
			_set_voxel_f(pos,1.0);
		}
		
	});

	_post_edit(ravine_box);

}

void VoxelTool::do_box(Vector3i begin, Vector3i end) {
	Vector3i::sort_min_max(begin, end);
	Rect3i box = Rect3i::from_min_max(begin, end + Vector3i(1,1,1));

	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBuffer::CHANNEL_SDF) {

		box.for_each_cell([this](Vector3i pos) {
			_set_voxel_f(pos, sdf_blend(-1.0, get_voxel_f(pos), _mode));
		});

	} else {

		int value = _mode == MODE_REMOVE ? _eraser_value : _value;

		box.for_each_cell([this, value](Vector3i pos) {
			_set_voxel(pos, value);
		});
	}

	_post_edit(box);
}

void VoxelTool::paste(Vector3i p_pos, Ref<VoxelBuffer> p_voxels, uint64_t mask_value) {
	ERR_FAIL_COND(p_voxels.is_null());
	Ref<VoxelBuffer> voxels = Object::cast_to<VoxelBuffer>(*p_voxels);
	ERR_FAIL_COND(voxels.is_null());
	ERR_PRINT("Not implemented");
}

bool VoxelTool::is_area_editable(const Rect3i &box) const {
	ERR_PRINT("Not implemented");
	return false;
}

void VoxelTool::_post_edit(const Rect3i &box) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_PRINT("Not implemented");
}

Variant VoxelTool::get_voxel_metadata(Vector3i pos) {
	ERR_PRINT("Not implemented");
	return Variant();
}

void VoxelTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "v"), &VoxelTool::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &VoxelTool::get_value);

	ClassDB::bind_method(D_METHOD("set_channel", "v"), &VoxelTool::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelTool::get_channel);

	ClassDB::bind_method(D_METHOD("set_mode", "m"), &VoxelTool::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &VoxelTool::get_mode);

	ClassDB::bind_method(D_METHOD("set_eraser_value", "v"), &VoxelTool::set_eraser_value);
	ClassDB::bind_method(D_METHOD("get_eraser_value"), &VoxelTool::get_eraser_value);

	ClassDB::bind_method(D_METHOD("get_voxel", "pos"), &VoxelTool::_b_get_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel_f", "pos"), &VoxelTool::_b_get_voxel_f);
	ClassDB::bind_method(D_METHOD("set_voxel", "pos", "v"), &VoxelTool::_b_set_voxel);
	ClassDB::bind_method(D_METHOD("set_voxel_f", "pos", "v"), &VoxelTool::_b_set_voxel_f);
	ClassDB::bind_method(D_METHOD("do_point", "pos"), &VoxelTool::_b_do_point);
	ClassDB::bind_method(D_METHOD("do_sphere", "center", "radius"), &VoxelTool::_b_do_sphere); 
	ClassDB::bind_method(D_METHOD("do_box", "begin", "end"), &VoxelTool::_b_do_box);
	ClassDB::bind_method(D_METHOD("do_ravine", "center", "direction"), &VoxelTool::_b_do_ravine);

	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "meta"), &VoxelTool::_b_set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelTool::_b_get_voxel_metadata);

	ClassDB::bind_method(D_METHOD("paste", "dst_pos", "src_buffer", "src_mask_value"), &VoxelTool::_b_paste);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance", "collision_mask"),
			&VoxelTool::_b_raycast, DEFVAL(10.0), DEFVAL(0xffffffff));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "eraser_value"), "set_eraser_value", "get_eraser_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Add,Remove,Set"), "set_mode", "get_mode");

	BIND_ENUM_CONSTANT(MODE_ADD);
	BIND_ENUM_CONSTANT(MODE_REMOVE);
	BIND_ENUM_CONSTANT(MODE_SET);
}
