#include "voxel_buffer.h"


VoxelBuffer::VoxelBuffer() {

}

VoxelBuffer::~VoxelBuffer() {
    clear();
}

void VoxelBuffer::create(int sx, int sy, int sz) {
    if (sx <= 0 || sy <= 0 || sz <= 0) {
        return;
    }
    Vector3i new_size(sx, sy, sz);
    if (new_size != _size) {
        for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
            Channel & channel = _channels[i];
            if (channel.data) {
                // TODO Optimize with realloc
                delete_channel(i, _size);
                create_channel(i, new_size);
            }
        }
        _size = new_size;
    }
}

void VoxelBuffer::clear() {
    for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
        Channel & channel = _channels[i];
        if (channel.data) {
            delete_channel(i, _size);
        }
    }
}

void VoxelBuffer::clear_channel(unsigned int channel_index, int clear_value) {
    ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
    delete_channel(channel_index, _size);
    _channels[channel_index].defval = clear_value;
}

int VoxelBuffer::get_voxel(int x, int y, int z, unsigned int channel_index) const {
    ERR_FAIL_INDEX_V(channel_index,  MAX_CHANNELS, 0);
    
    x -= _offset.x;
    y -= _offset.y;
    z -= _offset.z;

    const Channel & channel = _channels[channel_index];

    if (validate_local_pos(x, y, z) && channel.data) {
        return channel.data[z][x][y];
    }
    else {
        return channel.defval;
    }
}

int VoxelBuffer::get_voxel_local(int x, int y, int z, unsigned int channel_index) const {
    ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, 0);
    
    const Channel & channel = _channels[channel_index];

    if (validate_local_pos(x, y, z) && channel.data) {
        return channel.data[z][x][y];
    }
    else {
        return channel.defval;
    }
}

void VoxelBuffer::set_voxel(int value, int x, int y, int z, unsigned int channel_index) {
    ERR_FAIL_INDEX(channel_index,  MAX_CHANNELS);

    x -= _offset.x;
    y -= _offset.y;
    z -= _offset.z;

    ERR_FAIL_COND(!validate_local_pos(x, y, z));

    Channel & channel = _channels[channel_index];

    if (channel.defval != value) {
        if (channel.data == NULL) {
            create_channel(channel_index, _size);
        }
        channel.data[z][x][y] = value;
    }
}

void VoxelBuffer::set_voxel_v(int value, Vector3 pos, unsigned int channel_index) {
    set_voxel(value, pos.x, pos.y, pos.z, channel_index);
}

void VoxelBuffer::fill(int defval, unsigned int channel_index) {
    ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

    Channel & channel = _channels[channel_index];
    
    for (unsigned int z = 0; z < _size.z; ++z) {
        for (unsigned int x = 0; x < _size.x; ++x) {
            uint8_t * column = channel.data[z][x];
            for (unsigned int y = 0; y < _size.y; ++y) {
                column[y] = defval;
            }
        }
    }
}

bool VoxelBuffer::is_uniform(unsigned int channel_index) {
    ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);

    Channel & channel = _channels[channel_index];
    if (channel.data == NULL)
        return true;
    uint8_t voxel = channel.data[0][0][0];
    for (unsigned int z = 0; z < _size.z; ++z) {
        for (unsigned int x = 0; x < _size.x; ++x) {
            uint8_t * column = channel.data[z][x];
            for (unsigned int y = 0; y < _size.y; ++y) {
                if (column[y] != voxel) {
                    return false;
                }
            }
        }
    }
    return true;
}

void VoxelBuffer::optimize() {
    for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
        if (_channels[i].data && is_uniform(i)) {
            clear_channel(i, _channels[i].data[0][0][0]);
        }
    }
}

void VoxelBuffer::create_channel(int i, Vector3i size, uint8_t defval) {

    Channel & channel = _channels[i];
    channel.data = (uint8_t***)memalloc(size.z * sizeof(uint8_t**));

    for (unsigned int z = 0; z < size.z; ++z) {

        uint8_t ** plane = (uint8_t**)memalloc(size.x * sizeof(uint8_t*));
        channel.data[z] = plane;

        for (unsigned int x = 0; x < size.x; ++x) {

            uint8_t * column = (uint8_t*)memalloc(size.y * sizeof(uint8_t));
            plane[x] = column;

            for (unsigned int y = 0; y < size.y; ++y) {
                column[y] = defval;
            }
        }
    }
}

void VoxelBuffer::delete_channel(int i, Vector3i size) {

    Channel & channel = _channels[i];
    
    for (unsigned int z = 0; z < size.z; ++z) {
        for (unsigned int x = 0; x < size.x; ++x) {
            memfree(channel.data[z][x]);
        }
        memfree(channel.data[z]);
    }
    memfree(channel.data);
    channel.data = NULL;
}

void VoxelBuffer::_bind_methods() {

    ObjectTypeDB::bind_method(_MD("create", "sx", "sy", "sz"), &VoxelBuffer::create);
    ObjectTypeDB::bind_method(_MD("clear"), &VoxelBuffer::clear);

    ObjectTypeDB::bind_method(_MD("get_size_x"), &VoxelBuffer::get_size_x);
    ObjectTypeDB::bind_method(_MD("get_size_y"), &VoxelBuffer::get_size_y);
    ObjectTypeDB::bind_method(_MD("get_size_z"), &VoxelBuffer::get_size_z);

    ObjectTypeDB::bind_method(_MD("get_offset_x"), &VoxelBuffer::get_offset_x);
    ObjectTypeDB::bind_method(_MD("get_offset_y"), &VoxelBuffer::get_offset_y);
    ObjectTypeDB::bind_method(_MD("get_offset_z"), &VoxelBuffer::get_offset_z);

    ObjectTypeDB::bind_method(_MD("set_offset", "x", "y", "z"), &VoxelBuffer::set_offset);

    ObjectTypeDB::bind_method(_MD("set_voxel", "value", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel, DEFVAL(0));
    ObjectTypeDB::bind_method(_MD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::set_voxel, DEFVAL(0));
    ObjectTypeDB::bind_method(_MD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel, DEFVAL(0));

    ObjectTypeDB::bind_method(_MD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));

    ObjectTypeDB::bind_method(_MD("is_uniform", "channel"), &VoxelBuffer::is_uniform, DEFVAL(0));
    ObjectTypeDB::bind_method(_MD("optimize"), &VoxelBuffer::optimize);

}

