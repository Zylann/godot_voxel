#ifndef VOXEL_TOOL_H
#define VOXEL_TOOL_H

#include "math/rect3i.h"
#include <core/reference.h>

class VoxelBuffer;

// This class exists only to make the script API nicer.
class VoxelRaycastResult : public Reference {
	GDCLASS(VoxelRaycastResult, Reference)
public:
	Vector3i position;
	Vector3i previous_position;

private:
	Vector3 _b_get_position() const;
	Vector3 _b_get_previous_position() const;

	static void _bind_methods();
};

// High-level generic voxel edition utility.
// Ease of use comes at cost.
// It's not a class to instantiate alone, get it from the voxel objects you want to work with
class VoxelTool : public Reference {
	GDCLASS(VoxelTool, Reference)
public:
	enum Mode {
		MODE_ADD,
		MODE_REMOVE,
		MODE_SET
	};

	void set_value(int val);
	int get_value() const;

	void set_channel(int channel);
	int get_channel() const;

	void set_mode(Mode mode);
	Mode get_mode() const;

	void set_eraser_value(int value);
	int get_eraser_value() const;

	int get_voxel(Vector3i pos);
	float get_voxel_f(Vector3i pos);

	// The following methods represent one edit each. Pick the correct one for the job.
	// For example, using `do_box` will be more efficient than calling `do_point` many times.
	virtual void set_voxel(Vector3i pos, int v);
	virtual void set_voxel_f(Vector3i pos, float v);
	virtual void do_point(Vector3i pos);
	virtual void do_line(Vector3i begin, Vector3i end);
	virtual void do_circle(Vector3i pos, int radius, Vector3i direction);
	virtual void do_sphere(Vector3 center, float radius);
	virtual void do_box(Vector3i begin, Vector3i end);

	virtual void paste(Vector3i pos, Ref<VoxelBuffer> p_voxels, int mask_value);

	virtual Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance);

	// Checks if an edit affecting the given box can be applied, fully or partially
	virtual bool is_area_editable(const Rect3i &box) const;

protected:
	static void _bind_methods();

	// These methods never go alone, but may be used in others.
	// They don't represent an edit, they only abstract the lower-level API
	virtual int _get_voxel(Vector3i pos);
	virtual float _get_voxel_f(Vector3i pos);
	virtual void _set_voxel(Vector3i pos, int v);
	virtual void _set_voxel_f(Vector3i pos, float v);
	virtual void _post_edit(const Rect3i &box);

private:
	// Bindings to convert to more specialized C++ types and handle virtuality cuz I don't know if it works by binding straight
	int _b_get_voxel(Vector3 pos) { return get_voxel(Vector3i(pos)); }
	float _b_get_voxel_f(Vector3 pos) { return get_voxel_f(Vector3i(pos)); }
	void _b_set_voxel(Vector3 pos, int v) { set_voxel(Vector3i(pos), v); }
	void _b_set_voxel_f(Vector3 pos, float v) { set_voxel_f(Vector3i(pos), v); }
	Ref<VoxelRaycastResult> _b_raycast(Vector3 pos, Vector3 dir, float max_distance) { return raycast(pos, dir, max_distance); }
	void _b_do_point(Vector3 pos) { do_point(Vector3i(pos)); }
	void _b_do_line(Vector3i begin, Vector3i end) { do_line(Vector3i(begin), Vector3i(end)); }
	void _b_do_circle(Vector3 pos, float radius, Vector3 direction) { do_circle(Vector3i(pos), radius, Vector3i(direction)); }
	void _b_do_sphere(Vector3 pos, float radius) { do_sphere(pos, radius); }
	void _b_do_box(Vector3 begin, Vector3 end) { do_box(Vector3i(begin), Vector3i(end)); }
	void _b_paste(Vector3 pos, Ref<Reference> voxels, int mask_value) { paste(Vector3i(pos), voxels, mask_value); }

protected:
	int _value = 0;
	int _channel = 0;
	Mode _mode = MODE_ADD;
	int _eraser_value = 0; // air
};

VARIANT_ENUM_CAST(VoxelTool::Mode)

#endif // VOXEL_TOOL_H
