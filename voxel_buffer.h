#ifndef VOXEL_BUFFER_H
#define VOXEL_BUFFER_H

#include <reference.h>
#include <vector.h>
#include "vector3i.h"

// Dense voxels data storage.
// Organized in 8-bit channels like images, all optional.
// Note: for float storage (marching cubes for example), you can map [0..256] to [0..1] and save 3 bytes per cell

class VoxelBuffer : public Reference {
    OBJ_TYPE(VoxelBuffer, Reference)

    // Arbitrary value, 8 should be enough. Tweak for your needs.
    static const int MAX_CHANNELS = 8;

    struct Channel {
        // Allocated when the channel is populated.
        // Array of array of arrays, in order [z][x][y] because it makes vertical-wise access faster (the engine is Y-up).
        // SUGG: move to flat storage?
        uint8_t *** data;

        uint8_t defval; // Default value when data is null
        
        Channel() : data(NULL), defval(0) {}
    };

    // Each channel can store arbitary data.
    // For example, you can decide to store colors (R, G, B, A), gameplay types (type, state, light) or both.
    Channel _channels[MAX_CHANNELS];

    // How many voxels are there in the three directions. All populated channels have the same size.
    Vector3i _size;

    // Offset applied to coordinates when accessing voxels.
    // Use _local versions to bypass this.
    Vector3i _offset;

public:
    VoxelBuffer();
    ~VoxelBuffer();

    void create(int sx, int sy, int sz);
    void clear();
    void clear_channel(unsigned int channel_index, int clear_value=0);

    _FORCE_INLINE_ int get_size_x() const { return _size.x; }
    _FORCE_INLINE_ int get_size_y() const { return _size.y; }
    _FORCE_INLINE_ int get_size_z() const { return _size.z; }

    _FORCE_INLINE_ int get_offset_x() const { return _offset.x; }
    _FORCE_INLINE_ int get_offset_y() const { return _offset.y; }
    _FORCE_INLINE_ int get_offset_z() const { return _offset.z; }

    _FORCE_INLINE_ void set_offset(int x, int y, int z) { _offset = Vector3i(x,y,z); }

    int get_voxel(int x, int y, int z, unsigned int channel_index=0) const;
    int get_voxel_local(int x, int y, int z, unsigned int channel_index=0) const;
    void set_voxel(int value, int x, int y, int z, unsigned int channel_index=0);
    void set_voxel_v(int value, Vector3 pos, unsigned int channel_index = 0);

    void fill(int defval, unsigned int channel_index = 0);
    //void fill_min_max(int value, int x0, int y0, int z0, int x1, int y1, int z1, unsigned int channel_index = 0);

    bool is_uniform(unsigned int channel_index = 0);

    void optimize();

    //void copy_from(Ref<VoxelBuffer> other);

    _FORCE_INLINE_ bool validate_local_pos(unsigned int x, unsigned int y, unsigned int z) const {
        return x < _size.x
            && y < _size.y
            && z < _size.x;
    }

private:
    void create_channel(int i, Vector3i size, uint8_t defval=0);
    void delete_channel(int i, Vector3i size);

protected:
    static void _bind_methods();

};

#endif // VOXEL_BUFFER_H

