# Voxel Tools API Overview

The Voxel Tools API is under development and has not yet settled. This document contains the following:

1. GDScript API reference
1. Terminology used throughout the codebase
1. A description of classes exposed to GDScript


# API Reference
You can refer to the API that is exposed to GDScript in two places:

* You'll find class definitions in the [docs/api](api/Class_List.md) directory.

* Within the Godot editor, access the definitions in the help: 
1. Click *Search Help* in the GDScript editor.
1. Type "Voxel".
1. Browse through the classes, methods, and properties exposed to GDScript.


# Terminology Used Throughout The Codebase

**Voxel** - A 3D pixel in a Euclidean world where the coordinates (X, Y, Z) are restricted to integers.  That is, space is a 3D grid where each point in space can only be an integer (...-3, -2, -1, 0, 1, 2, 3...) and there is no infinite space between points.  As an example, from (0,0,0), the next point in space along the X axis is (1,0,0). There is no (0.5, 0, 0) or (0.25, 0, 0). Each point in space is a cube. 

In Minecraft type games, building blocks are cubes. However, they do not have to be full cubes. Minecraft has half-cubes, etc. They can also be smoother shapes, as described below under Smooth [Terrain].

Note that the engine and your graphics card do not work with voxels. They are converted and drawn as triangles like all other meshes at render time. This means you can use voxels and polygonal meshes simultaneously.


**Block** - While a voxel is a single unit of space, a block refers to an N\*N\*N voxel chunk of terrain, where N is 8, 16(default), or 32. Blocks of voxels are paged in and out by the terrain, depending their distance to the viewer. Blocks are therefore much larger than voxels, and the project maintains two grids: voxel space and block space. The latter is 8/16/32 times larger.


**Voxel Data** - This refers to a high resolution source of data, often coming from the real world or an analog source. Imagine an airplane with it's smooth, aerodynamic curves. If we 3D scanned this plane, that would be high resolution voxel data. If we built that airplane out of Legos, that would be a low resolution, blocky representation of the source voxel data. See Blocky below.


**Blocky [Terrain]** - An algorithm that uses full cubes to approximate Voxel data. This is Minecraft, building an airplane out of Legos, or representing a bell curve with a bar chart. It is better suited for man-made shapes with hard corners.


**Smooth [Terrain]** - One of many isosurface extraction algorithms that attempts to represent voxel data with smoother surfaces and is a better fit for organic shapes. Rather than using a full cube to describe the surface, a cube with one or more planes at potentially any angle is used to provide a far more accurate representation of the source data. As an example if you took a Lego block and sanded down the sharp corners to better represent the airplane curves, or sanded down the tops of the bar chart to better represent the bell curve. However you can only sand down to flat planes, no curves within the cubes.

Voxel Tools supports Dual Marching Cubes, Transvoxel (WIP) and is structured to support other algorithms in the future. Some are faster, use less memory, provide manifold meshes (cleaner meshes), and/or provide more accurate representations of the voxel data.

**LOD - Level of Detail** - As the camera gets further away, detail decreases to improve performance. 


# Description of Classes

What follows are notes and descriptions about each of the classes that have been exposed to GDScript.


## Scene Node Classes

These nodes can be added to your scene.

Note: Terrains use their VoxelStream and VoxelMeshers from multiple threads. Access is granted in the editor because I turned off terrain generation specifically here, but at runtime some things (like getting the stream from the terrain and asking it to generate a block) are prone to undefined behavior. I still need to add locks here to suspend threads while configurations are changed.

### VoxelTerrain 
An infinite, paged terrain made of voxel blocks, all with the same level of detail. Voxels are polygonized around the viewer by distance in a large cubic space. 

* Voxel data is streamed using a VoxelStream (e.g. noise image, 3D noise texture, etc).
* Stores a terrain in a VoxelMap.
* Supports blocky and smooth terrains.
* Handles the main Godot \_process() call to update the terrain, load and unload voxel blocks, and generate the mesh and collision.
* Provides raycasts (for manual collision and click detection) in addition to physics collision based raycasts.


### VoxelLodTerrain
An infinite, paged terrain made of voxel blocks with a variable level of detail. Designed for high view distances. Voxels are polygonized around the viewer by distance in a very large sphere. 

* Data is streamed using a VoxelStream, which must support LOD.
* Stores a terrain in a VoxelMap.
* Handles the main Godot \_process() call to update the terrain, load and unload voxel blocks, and generate the mesh and collision.
* Supports only smooth terrains.
* Supports raycasts via physics based collision.
* Manages Levels of detail (LOD).


## Voxel Data Provider Classes

These classes provide the source voxel data.

### VoxelStream
Base class to provide infinite, paged voxel data as blocks. Doesn't provide data itself, but can be extended with GDScript. 

If you choose to implement your own stream that does not offer LOD, it is recommended to `assert(lod == 0)`. It will be executed in a separate thread, so access the main thread with care.

The classes below provide voxel data.

### VoxelStreamTest
Provides a flat plane or 2D sine waves.

* Blocky only.

### VoxelStreamImage
Creates a heightmap based on a user provided, black and white image. 

This class expects the image to be imported as an Image. By default, image files are imported as a Texture, which are stored in video RAM and are not accessible to the engine. On the Import tab, you need to change the import method of your image to Image and may need to restart Godot before VoxelStreamImage will recognize your file.

* Blocky or smooth.

### VoxelStreamNoise
Generates 3D noise via OpenSimplexNoise. 

* Smooth only.


## Mesher Classes

These classes use full cubes or an isosurface extraction algorithm to represent the source voxel data. They polygonize arbitrary-sized chunks of voxels. 

Note: Their initial use case was for terrains, so the input data must be padded by a certain amount of voxels to work (get_minimum_padding() gives you that number. It's usually 1 or 2).

### VoxelMesher
Abstract base class to build a polygonal terrain mesh from a given VoxelBuffer. Unlike VoxelStream, this class cannot be implemented with a script. You must extend it with a C++ class to implement another meshing algorithm.

### VoxelMesherBlocky
Builds a mesh using full cubes only. Looks like Minecraft.

* The fastest mesher.
* No LOD support currently.
* Offers baked ambient occlusion around edges.
* Has the capability to support any kind of shape inside the block, (e.g. half-cubes, grass shapes, etc), but this API is not yet tested or exposed very well or at all.


### VoxelMesherDMC
Builds a smooth mesh using the Dual Marching Cubes algorithm. 

* Supports LOD by simply scaling up the result.
* Uses marching squares skirts to hide the cracks between sections with a different LOD.


### VoxelMesherTransvoxel
Builds a smooth mesh using the Transvoxel algorithm. 

* Development is on hold for now. Use DMC.
* Partial implementation, which may or may not be broken.



## Storage Classes

### VoxelMap 

Where Terrains store their voxels, as an infinite, 3D, sparse grid. It can be thought of as the infinite equivalent of VoxelBuffer.

* Stores a 3D grid in a HashMap containing integer vector3s as keys, and VoxelBlocks as values. i.e. key:Vector3int => value:VoxelBlock pairs.
* Provides functions to convert between the voxel grid coordinates and block grid coordinates, which is 8/16/32 times larger.
* The default grid size is 16 and while some support is there for 8 and 32, this setting is not yet adjustable.

Note: There are two grid coordinate systems. One addresses voxels individually, the other addresses blocks. It was initially required to allow streaming of the world, because loading voxels individually would have been very expensive. So instead I load chunks (like Minecraft), and those chunks lie in a grid having a step 16 times larger (because each block contains 16\*16\*16 voxels). When LOD is considered, there are even multiple block grids, but each having a scale higher by powers of two.


### VoxelBlock (Unregistered)
Internal structure for holding a reference to mesh visuals, collision, and a block of voxel data. Not available to GDScript, but supports both the above and below classes.

* Stores 3D grid position and LOD index.
* Creates StaticBody and Shapes to enable collision, and CollisionShapes to support the editor `Debug\Visual Collsion Shapes` feature.
* References a VoxelBuffer and a Mesh.

### VoxelBuffer 

The underlying storage for voxels. Also used to pass voxels between functions.

* Stores an arbitrary number of voxels (Vector3int specifying how many in each direction).
* Stores up to 8 channels of arbitrary data per voxel. Each channel is allocated only upon use. (e.g. R,G,B,A, game play types (type, state, light), etc).


## Supporting Classes

### VoxelIsoSurfaceTool
Provides functions to add or remove smooth terrain. Currently supported shapes are sphere, plane, cube, heightmap.

* You can generate a 2D heightmap and ask the tool to "rasterize" that portion of the world in a VoxelBuffer, which is what you will have to fill when implementing a custom VoxelStream.

* Since Godot does not support 3D images, a custom shape could be added using an existing VoxelBuffer as input. This is not yet implemented.


### VoxelBoxMover
Provides Axis Aligned Bounding Box based collision. (blocky only)


### Voxel
A basic voxel unit. Creating Voxels with VoxelLibrary is recommended.  (blocky only)

* Stores ID, name, material, transparency and type


### VoxelLibrary
A factory for creating Voxels. (blocky only)

---
* [API Class List](api/Class_List.md)
* [Doc Index](01_get-started.md)
