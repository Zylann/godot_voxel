# Class: VoxelBuffer

Inherits: Reference

_Godot version: 3.2.1_


## Online Tutorials: 



## Constants:

#### » ChannelId.CHANNEL_TYPE = 0


#### » ChannelId.CHANNEL_SDF = 1


#### » ChannelId.CHANNEL_DATA2 = 2


#### » ChannelId.CHANNEL_DATA3 = 3


#### » ChannelId.CHANNEL_DATA4 = 4


#### » ChannelId.CHANNEL_DATA5 = 5


#### » ChannelId.CHANNEL_DATA6 = 6


#### » ChannelId.CHANNEL_DATA7 = 7


#### » ChannelId.MAX_CHANNELS = 8


#### » Depth.DEPTH_8_BIT = 0


#### » Depth.DEPTH_16_BIT = 1


#### » Depth.DEPTH_32_BIT = 2


#### » Depth.DEPTH_64_BIT = 3


#### » Depth.DEPTH_COUNT = 4



## Properties:


## Methods:

#### » void clear (  ) 


#### » void copy_channel_from ( VoxelBuffer other, int channel ) 


#### » void copy_channel_from_area ( VoxelBuffer other, Vector3 src_min, Vector3 src_max, Vector3 dst_min, int channel ) 


#### » void create ( int sx, int sy, int sz ) 


#### » void downscale_to ( VoxelBuffer dst, Vector3 src_min, Vector3 src_max, Vector3 dst_min )  const


#### » void fill ( int value, int channel=0 ) 


#### » void fill_area ( int value, Vector3 min, Vector3 max, int channel=0 ) 


#### » void fill_f ( float value, int channel=0 ) 


#### » int get_channel_depth ( int channel )  const


#### » Vector3 get_size (  )  const


#### » int get_size_x (  )  const


#### » int get_size_y (  )  const


#### » int get_size_z (  )  const


#### » int get_voxel ( int x, int y, int z, int channel=0 )  const


#### » float get_voxel_f ( int x, int y, int z, int channel=0 )  const


#### » VoxelTool get_voxel_tool (  ) 


#### » bool is_uniform ( int channel )  const


#### » void optimize (  ) 


#### » void set_channel_depth ( int channel, int depth ) 


#### » void set_voxel ( int value, int x, int y, int z, int channel=0 ) 


#### » void set_voxel_f ( float value, int x, int y, int z, int channel=0 ) 


#### » void set_voxel_v ( int value, Vector3 pos, int channel=0 ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Feb 13, 2020_
