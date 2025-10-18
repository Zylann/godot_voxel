# VoxelToolLodTerrain

Inherits: [VoxelTool](VoxelTool.md)

Implementation of [VoxelTool](VoxelTool.md) specialized for uses on [VoxelLodTerrain](VoxelLodTerrain.md).

## Description: 

Functions in this class are specific to [VoxelLodTerrain](VoxelLodTerrain.md). For generic functions, you may also check [VoxelTool](VoxelTool.md).

It's not a class to instantiate alone, you may get it from [VoxelLodTerrain](VoxelLodTerrain.md) using the `get_voxel_tool()` method.

## Methods: 


Return                                                                    | Signature                                                                                                                                                                                                                                                                                                                                                                                            
------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                 | [do_graph](#i_do_graph) ( [VoxelGeneratorGraph](VoxelGeneratorGraph.md) graph, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) area_size )                                                                                                                              
[void](#)                                                                 | [do_hemisphere](#i_do_hemisphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 )     
[void](#)                                                                 | [do_sphere_async](#i_do_sphere_async) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius )                                                                                                                                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [get_raycast_binary_search_iterations](#i_get_raycast_binary_search_iterations) ( ) const                                                                                                                                                                                                                                                                                                            
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [get_voxel_f_interpolated](#i_get_voxel_f_interpolated) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) const                                                                                                                                                                                                                                              
[void](#)                                                                 | [run_blocky_random_tick](#i_run_blocky_random_tick) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 )  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)  | [separate_floating_chunks](#i_separate_floating_chunks) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) parent_node )                                                                                                                                                                           
[void](#)                                                                 | [set_raycast_binary_search_iterations](#i_set_raycast_binary_search_iterations) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) iterations )                                                                                                                                                                                                                                  
[void](#)                                                                 | [stamp_sdf](#i_stamp_sdf) ( [VoxelMeshSDF](VoxelMeshSDF.md) mesh_sdf, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) isolevel, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_scale )  *(deprecated)*                                        
<p></p>

## Method Descriptions

### [void](#)<span id="i_do_graph"></span> **do_graph**( [VoxelGeneratorGraph](VoxelGeneratorGraph.md) graph, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) area_size ) 

Uses a [VoxelGeneratorGraph](VoxelGeneratorGraph.md) as a brush, which allows to program SDF operations performed by the brush.

The graph must have an SDF input and an SDF output, and preferably work assuming a shape of unit size. For example, an additive sphere brush may use an `SdfSphere` node with radius 1 with a `Min` to combine it with terrain SDF.

See also [online documentation](https://voxel-tools.readthedocs.io/en/latest/generators/#using-voxelgeneratorgraph-as-a-brush).

### [void](#)<span id="i_do_hemisphere"></span> **do_hemisphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 ) 

Operates on a hemisphere, where `flat_direction` is pointing away from the flat surface (like a normal). `smoothness` determines how the flat part blends with the rounded part, with higher values producing softer more rounded edge.

### [void](#)<span id="i_do_sphere_async"></span> **do_sphere_async**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius ) 

*(This method has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_raycast_binary_search_iterations"></span> **get_raycast_binary_search_iterations**( ) 

*(This method has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f_interpolated"></span> **get_voxel_f_interpolated**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) 

*(This method has no documentation)*

### [void](#)<span id="i_run_blocky_random_tick"></span> **run_blocky_random_tick**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) 

*(This method has no documentation)*

### [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_separate_floating_chunks"></span> **separate_floating_chunks**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) parent_node ) 

Turns floating voxels into RigidBodies.

Chunks of floating voxels are detected within a box. The box is relative to the voxel volume this VoxelTool is attached to. Chunks have to be contained entirely within that box to be considered floating. Chunks are removed from the source volume and transformed into RigidBodies with convex collision shapes. They will be added as child of the provided node. They will start "kinematic", and turn "rigid" after a short time, to allow the terrain to update its colliders after the removal (otherwise they will overlap). The function returns an array of these rigid bodies, which you can use to attach further behavior to them (such as disappearing after some time or distance for example).

This algorithm can become expensive quickly, so the box should not be too big. A size of around 30 voxels should be ok.

### [void](#)<span id="i_set_raycast_binary_search_iterations"></span> **set_raycast_binary_search_iterations**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) iterations ) 

Picks random voxels within the specified area and executes a function on them. This only works for terrains using [VoxelMesherBlocky](VoxelMesherBlocky.md). Only voxels where [Voxel.random_tickable](https://docs.godotengine.org/en/stable/classes/class_voxel.html#class-voxel-property-random-tickable) is `true` will be picked.

The given callback takes two arguments: voxel position (Vector3i), voxel value (int).

Only voxels at LOD 0 will be considered.

### [void](#)<span id="i_stamp_sdf"></span> **stamp_sdf**( [VoxelMeshSDF](VoxelMeshSDF.md) mesh_sdf, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) isolevel, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_scale ) 

*This method is deprecated. Use [VoxelTool.do_mesh](VoxelTool.md#i_do_mesh) instead.*


_Generated on Aug 09, 2025_
