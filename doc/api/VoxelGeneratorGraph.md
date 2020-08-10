# Class: VoxelGeneratorGraph

Inherits: VoxelGenerator

_Godot version: 3.2_


## Online Tutorials: 



## Constants:

#### » NodeTypeID.NODE_CONSTANT = 0


#### » NodeTypeID.NODE_INPUT_X = 1


#### » NodeTypeID.NODE_INPUT_Y = 2


#### » NodeTypeID.NODE_INPUT_Z = 3


#### » NodeTypeID.NODE_OUTPUT_SDF = 4


#### » NodeTypeID.NODE_ADD = 5


#### » NodeTypeID.NODE_SUBTRACT = 6


#### » NodeTypeID.NODE_MULTIPLY = 7


#### » NodeTypeID.NODE_DIVIDE = 8


#### » NodeTypeID.NODE_SIN = 9


#### » NodeTypeID.NODE_FLOOR = 10


#### » NodeTypeID.NODE_ABS = 11


#### » NodeTypeID.NODE_SQRT = 12


#### » NodeTypeID.NODE_FRACT = 13


#### » NodeTypeID.NODE_STEPIFY = 14


#### » NodeTypeID.NODE_WRAP = 15


#### » NodeTypeID.NODE_MIN = 16


#### » NodeTypeID.NODE_MAX = 17


#### » NodeTypeID.NODE_DISTANCE_2D = 18


#### » NodeTypeID.NODE_DISTANCE_3D = 19


#### » NodeTypeID.NODE_CLAMP = 20


#### » NodeTypeID.NODE_MIX = 21


#### » NodeTypeID.NODE_REMAP = 22


#### » NodeTypeID.NODE_SMOOTHSTEP = 23


#### » NodeTypeID.NODE_CURVE = 24


#### » NodeTypeID.NODE_SELECT = 25


#### » NodeTypeID.NODE_NOISE_2D = 26


#### » NodeTypeID.NODE_NOISE_3D = 27


#### » NodeTypeID.NODE_IMAGE_2D = 28


#### » NodeTypeID.NODE_SDF_PLANE = 29


#### » NodeTypeID.NODE_SDF_BOX = 30


#### » NodeTypeID.NODE_SDF_SPHERE = 31


#### » NodeTypeID.NODE_SDF_TORUS = 32


#### » NodeTypeID.NODE_SDF_PREVIEW = 33


#### » NodeTypeID.NODE_TYPE_COUNT = 34



## Properties:


## Methods:

#### » void add_connection ( int src_node_id, int src_port_index, int dst_node_id, int dst_port_index ) 


#### » bool can_connect ( int src_node_id, int src_port_index, int dst_node_id, int dst_port_index )  const


#### » void clear (  ) 


#### » void compile (  ) 


#### » int create_node ( int type_id, Vector2 position, int id=0 ) 


#### » void debug_load_waves_preset (  ) 


#### » float debug_measure_microseconds_per_voxel (  ) 


#### » float generate_single ( Vector3 arg0 ) 


#### » Array get_connections (  )  const


#### » Variant get_node_default_input ( int node_id, int input_index )  const


#### » Vector2 get_node_gui_position ( int node_id )  const


#### » PoolIntArray get_node_ids (  )  const


#### » Variant get_node_param ( int node_id, int param_index )  const


#### » int get_node_type_count (  )  const


#### » int get_node_type_id ( int node_id )  const


#### » Dictionary get_node_type_info ( int type_id )  const


#### » bool is_compressing_uniform_channels (  )  const


#### » void remove_connection ( int src_node_id, int src_port_index, int dst_node_id, int dst_port_index ) 


#### » void remove_node ( int node_id ) 


#### » void set_compress_uniform_channels ( bool enabled ) 


#### » void set_node_default_input ( int node_id, int input_index, Variant value ) 


#### » void set_node_gui_position ( int node_id, Vector2 position ) 


#### » void set_node_param ( int node_id, int param_index, Variant value ) 


#### » void set_node_param_null ( int node_id, int param_index ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Aug 10, 2020_
