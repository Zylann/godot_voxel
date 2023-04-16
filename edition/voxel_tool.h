#ifndef VOXEL_TOOL_H
#define VOXEL_TOOL_H

#include "../storage/funcs.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/math/box3i.h"
#include "../util/math/sdf.h"
#include "funcs.h"
#include "voxel_raycast_result.h"

// TODO Need to review VoxelTool to account for transformed volumes

namespace zylann::voxel {

// High-level generic voxel edition utility.
// Ease of use comes at cost.
// It's not a class to instantiate alone, get it from the voxel objects you want to work with
class VoxelTool : public RefCounted {
	GDCLASS(VoxelTool, RefCounted)
public:
	enum Mode {
		MODE_ADD = ops::MODE_ADD,
		MODE_REMOVE = ops::MODE_REMOVE,
		MODE_SET = ops::MODE_SET,
		MODE_TEXTURE_PAINT = ops::MODE_TEXTURE_PAINT
	};

	VoxelTool();

	void set_value(uint64_t val);
	uint64_t get_value() const;

	void set_channel(VoxelBufferInternal::ChannelId p_channel);
	VoxelBufferInternal::ChannelId get_channel() const;

	void set_mode(Mode mode);
	Mode get_mode() const;

	void set_eraser_value(uint64_t value);
	uint64_t get_eraser_value() const;

	uint64_t get_voxel(Vector3i pos) const;
	float get_voxel_f(Vector3i pos) const;

	float get_sdf_scale() const;
	void set_sdf_scale(float s);

	void set_texture_index(int ti);
	int get_texture_index() const;

	void set_texture_opacity(float opacity);
	float get_texture_opacity() const;

	void set_texture_falloff(float falloff);
	float get_texture_falloff() const;

	void set_sdf_strength(float strength);
	float get_sdf_strength() const;

	// TODO Methods working on a whole area must use an implementation that minimizes locking!

	// The following methods represent one edit each. Pick the correct one for the job.
	// For example, using `do_box` will be more efficient than calling `do_point` many times.
	virtual void set_voxel(Vector3i pos, uint64_t v);
	virtual void set_voxel_f(Vector3i pos, float v);
	virtual void do_point(Vector3i pos);
	virtual void do_sphere(Vector3 center, float radius);
	virtual void do_box(Vector3i begin, Vector3i end);

	void sdf_stamp_erase(Ref<gd::VoxelBuffer> stamp, Vector3i pos);

	virtual void copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) const;
	virtual void paste(Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask);
	virtual void paste_masked(Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel,
			uint64_t mask_value);

	void smooth_sphere(Vector3 sphere_center, float sphere_radius, int blur_radius);

	virtual Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask);

	// Checks if an edit affecting the given box can be applied, fully or partially
	virtual bool is_area_editable(const Box3i &box) const;

	virtual void set_voxel_metadata(Vector3i pos, Variant meta);
	virtual Variant get_voxel_metadata(Vector3i pos) const;

protected:
	static void _bind_methods();

	// These methods never go alone, but may be used in others.
	// They don't represent an edit, they only abstract the lower-level API
	virtual uint64_t _get_voxel(Vector3i pos) const;
	virtual float _get_voxel_f(Vector3i pos) const;
	virtual void _set_voxel(Vector3i pos, uint64_t v);
	virtual void _set_voxel_f(Vector3i pos, float v);
	virtual void _post_edit(const Box3i &box);

private:
	// Bindings to convert to more specialized C++ types and handle virtuality,
	// cuz I don't know if it works by binding straight

	uint64_t _b_get_voxel(Vector3i pos);
	float _b_get_voxel_f(Vector3i pos);
	void _b_set_voxel(Vector3i pos, uint64_t v);
	void _b_set_voxel_f(Vector3i pos, float v);
	Ref<VoxelRaycastResult> _b_raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask);
	void _b_do_point(Vector3i pos);
	void _b_do_sphere(Vector3 pos, float radius);
	void _b_do_box(Vector3i begin, Vector3i end);
	void _b_copy(Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channel_mask);
	void _b_paste(Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channels_mask);
	void _b_paste_masked(
			Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channels_mask, int mask_channel, int64_t mask_value);
	Variant _b_get_voxel_metadata(Vector3i pos) const;
	void _b_set_voxel_metadata(Vector3i pos, Variant meta);
	bool _b_is_area_editable(AABB box) const;
	void _b_set_channel(gd::VoxelBuffer::ChannelId p_channel);
	gd::VoxelBuffer::ChannelId _b_get_channel() const;

protected:
	uint64_t _value = 0;
	uint64_t _eraser_value = 0; // air
	VoxelBufferInternal::ChannelId _channel = VoxelBufferInternal::CHANNEL_TYPE;
	float _sdf_scale = 1.f;
	float _sdf_strength = 1.f;
	Mode _mode = MODE_ADD;

	// Used on smooth terrain
	ops::TextureParams _texture_params;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelTool::Mode)

#endif // VOXEL_TOOL_H
