Changelog
============

This is a high-level list of features, changes and fixes that have been made over time.

At the moment, this module doesn't have a distinct release schedule, so this changelog follows Godot's version numbers and binary releases. Almost each version mentionned here should have an associated Git branch (for THIS repo, not Godot's) containing features at the time of the version. Backports aren't done so far.

Semver is not yet in place, so each version can have breaking changes, although it shouldn't happen often.


06/11/2021 - `godot3.4`
-------------------------

- General
    - `VoxelTerrain`: added `get_data_block_size()`
    - `VoxelToolTerrain`: added  `for_each_voxel_metadata_in_area()` to quickly find all metadata in a box
    - `FastNoiseLiteGradient`: exposed missing warp functions
    - Added property to terrain nodes to configure collision margin
    - Thread count is automatically determined from the number of concurrent threads the CPU supports
    - Added project settings to configure thread count
    - Added third-party licenses to the About Window in the editor

- Blocky voxels
    - Added *.vox importers to import MagicaVoxel files as scenes or meshes

- Smooth voxels
    - `VoxelMesherTransvoxel`: Initial support for texturing data in voxels, using 4-bit indices and weights
    - `VoxelMesherTransvoxel`: optimized hot path, making it about 20% faster
    - `VoxelMesherTransvoxel`: added option to simplify meshes using MeshOptimizer
    - `VoxelToolLodTerrain`: added `copy` function
    - `VoxelToolLodTerrain`: added `get_voxel_f_interpolated` function, useful to obtain interpolated SDF
    - `VoxelToolLodTerrain`: added a function to separate floating chunks as rigid-bodies in a specified region
    - `VoxelInstanceGenerator`: added extra option to emit from faces more precisely, especially when meshes got simplified (slower than the other options)
    - `VoxelInstancer`: added menu to setup a multimesh item from a scene (similarly to GridMap), which also allows to set up colliders
    - `VoxelInstancer`: added initial support for instancing regular scenes (slower than multimeshes)
    - `VoxelInstancer`: added option to turn off random rotation
    - `VoxelInstanceLibrary`: moved menu to add/remove/update items to the inspector, instead of the 3D editor toolbar

- Breaking changes
    - `VoxelBuffer`: channels `DATA3` and `DATA4` were renamed `INDICES` and `WEIGHTS`
    - `VoxelInstanceGenerator`: `EMIT_FROM_FACES` got renamed `EMIT_FROM_FACES_FAST`. `EMIT_FROM_FACES` still exists but is a different algorithm.
    - `VoxelServer`: `get_stats()` format has changed, check documentation
    - `VoxelLodTerrain`: `get_statistics()` format has changed: `time_process_update_responses` and `remaining_main_thread_blocks` are no longer available
    - `VoxelLodTerrain`: Maximum LOD count was decreased to 24, which is still largely enough. Higher maximums were likely to cause integer overflows.
    - `VoxelTerrain`: `get_statistics()` format has changed: `time_process_update_responses` and `remaining_main_thread_blocks` are no longer available
    - `VoxelViewer`: `requires_collisions` is now `true` by default

- Fixes
    - `VoxelGeneratorGraph`: changes to node properties are now saved properly
    - `VoxelGeneratorGraph`: fix some per-thread memory not freed on exit
    - `VoxelGeneratorGraph`: `debug_analyze_range` was crashing when given a negative-size area
    - `VoxelBuffer`: `copy_voxel_metadata_in_area` was checking the source box incorrectly
    - `VoxelBuffer`: multiple calls to `create()` with different sizes could lead to heap corruption if a channel was not uniform
    - `VoxelBuffer`: `copy_channel_from_area` could lead to heap corruption if the source and destination had the same size and were copied entirely
    - `VoxelMesherTransvoxel`: no longer crashes when the input buffer is not cubic
    - `VoxelLodTerrain`: fixed errors and crashes when editing voxels near loading borders
    - `VoxelLodTerrain`: fixed crash occurring after a few edits when LOD count is set to 1
    - `VoxelLodTerrain`: fixed crash when `run stream in editor` is turned off while the terrain is loading in editor
    - `VoxelTool` channel no longer defaults to 7 when using `get_voxel_tool` from a terrain with a stream assigned. Instead it picks first used channel of the mesher (fallback order is mesher, then generator, then stream).
    - `VoxelInstancer`: fixed error when node visibility changes
    - `VoxelInstancer`: fixed no instances generated when density is 1 in vertex emission mode 
    - `VoxelInstanceLibraryItem`: fixed collision shapes setup in editor not being saved
    - `VoxelInstanceLibrarySceneItem`: fixed associated scene not being saved
    - `VoxelTerrain`: fixed materials shown under the wrong inspector category
    - `VoxelStreamRegionFiles`: fixed errors caused by meta file being sometimes written with wrong depth values
    - `VoxelStreamBlockFiles`: fixed warning about channels always shown in the scene tree
    - `VoxelStreamSQLite`: fixed blocks above LOD0 being saved at wrong locations, causing them to be reloaded often floating in the air
    - Fix some crashes occurring when all PoolVector allocs are in use (Godot 3.x limitation). It will print errors instead, but crashes can still occur inside Godot's code as it's not often checking for this
    - Fix some crashes occurring when negative sizes are sent to AABB function parameters


09/05/2021 - `godot3.3`
-----------------------

- General
    - Introduction of `VoxelServer`, which shares threaded tasks among all voxel nodes
    - Voxel data is no longer copied when sent to processing threads, reducing high memory spikes in some scenarios
    - Added a utility class to load `.vox` files created with MagicaVoxel (scripts only)
    - Voxel nodes can be moved, scaled and rotated
    - Voxel nodes can be limited to specific bounds, rather than being infinitely paging volumes (multiples of block size).
    - Meshers are now resources so you can choose and configure them per terrain
    - Added [FastNoiseLite](https://github.com/Auburn/FastNoise) for a wider variety of noises
    - Generators are no longer limited to a single background thread
    - Added `VoxelStreamSQLite`, allowing to save volumes as a single SQLite database
    - Implemented `copy` and `paste` for `VoxelToolTerrain`
    - Added ability to double block size used for meshes and instancing, improving rendering speed at the cost of slower modification
    - Added collision layer and mask properties

- Editor
    - Streaming/LOD can be set to follow the editor camera instead of being centered on world origin. Use with caution, fast big movements and zooms can cause lag
    - The amount of pending background tasks is now indicated when the node is selected
    - Added About window

- Smooth voxels
    - Shaders now have access to the transform of each block, useful for triplanar mapping on moving volumes
    - Transvoxel runs faster (almost x2 speedup)
    - The SDF channel is now 16-bit by default instead of 8-bit, which reduces terracing in big terrains
    - Optimized `VoxelGeneratorGraph` by making it detect empty blocks more accurately and process by buffers
    - `VoxelGeneratorGraph` now exposes performance tuning parameters
    - Added `SdfSphereHeightmap` and `Normalize` nodes to voxel graph, which can help making planets
    - Added `SdfSmoothUnion` and `SdfSmoothSubtract` nodes to voxel graph
    - Added `VoxelInstancer` to instantiate items on top of `VoxelLodTerrain`, aimed at spawning natural elements such as rocks and foliage
    - Implemented `VoxelToolLodterrain.raycast()`
    - Added experimental API for LOD fading, with some limitations.

- Blocky voxels
    - Introduced a second blocky mesher dedicated to colored cubes, with greedy meshing and palette support
    - Replaced `transparent` property with `transparency_index` for more control on the culling of transparent faces
    - The TYPE channel is now 16-bit by default instead of 8-bit, allowing to store up to 65,536 types (part of this channel might actually be used to store rotation in the future)
    - Added normalmaps support
    - `VoxelRaycastResult` now also contains hit distance, so it is possible to determine the exact hit position
    - `VoxelBoxMover` supports scaled/translated terrains and `VoxelMesherCubes` (limited to non-zero color voxels)
    - `VoxelColorPalette` colors can be edited in the inspector
    - `VoxelToolTerrain.raycast` accounts for scale and rotation, and supports VoxelMesherCubes (non-zero values)

- Breaking changes
    - `VoxelViewer` now replaces the `viewer_path` property on `VoxelTerrain`, and allows multiple loading points
    - Defined `COLOR` channel in `VoxelBuffer`, previously known as `DATA2`
    - `VoxelGenerator` is no longer the base for script-based generators, use `VoxelGeneratorScript` instead
    - `VoxelGenerator` no longer inherits `VoxelStream`
    - `VoxelStream` is no longer the base for script-based streams, use `VoxelStreamScript` instead
    - Generators and streams have been split. Streams are more dedicated to files and use a single background thread. Generators are dedicated to generation and can be used by more than one background thread. Terrains have one property for each.
    - The meshing system no longer "guesses" how voxels will look like. Instead it uses the mesher assigned to the terrain.
    - SDF and TYPE channels have different default depth, so if you relied on 8-bit depth, you may have to explicitely set that format in your generator, to avoid mismatch with existing savegames
    - The block serialization format has changed, and migration is not implemented, so old saves using it cannot be used. See documentation for more information.
    - Terrains no longer auto-save when they are destroyed while having a `stream` assigned. You have to call `save_modified_blocks()` explicitely before doing that.
    - `VoxelLodTerrain.lod_split_scale` has been replaced with `lod_distance` for clarity. It is the distance from the viewer where the first LOD level may extend. 

- Fixes
    - C# should be able to properly implement generator/stream functions

- Known issues
    - `VoxelLodTerrain` does not entirely support `VoxelViewer`, but a refactoring pass is planned for it.


08/09/2020 - `godot3.2.3`
--------------------------

- General
    - Added per-voxel and per-block metadata, which are saved by file streams along with voxel data
    - `StringName` is now used when possible to call script functions, to reduce overhead
    - Exposed block serializer to allow encoding voxels for network or files from script
    - Added terrain methods to trigger saves explicitely
    - The module only prints debug logs if the engine is in verbose mode
    - `VoxelTerrain` now emit signals when blocks are loaded and unloaded

- Editor
    - Terrain nodes now render in the editor, unless scripts are involved (can be changed with an option)

- Blocky voxels
    - Added collision masks to blocky voxels, which takes effect with `VoxelBoxMover` and voxel raycasts
    - Added random tick API for blocky terrains
    - Blocky mesh collision shapes now take all surfaces into account

- Smooth voxels
    - Added a graph-based generator, dedicated to SDF data for now

- Fixes
    - Fixed crash when `VoxelTerrain` is used without a stream
    - Fixed `Voxel.duplicate()` not implemented properly
    - Fixed collision shapes only being generated for the first mesh surface
    - Fixed `VoxelTool` truncating 64-bit values


27/03/2020 - (no branch) - Tokisan Games binary version
--------------------------------------------------------

- General
    - Optimized `VoxelBuffer` memory allocation with a pool (best when sizes are often the same)
    - Optimized block visibility by removing meshes from the world rather than setting the `visible` property
    - Added customizable bit depth to `VoxelBuffer` (8, 16, 32 and 64 bits)
    - Added node configuration warnings in case terrain nodes are wrongly setup
    - Implemented VoxelTool for VoxelBuffers
    - Added `VoxelStreamNoise2D`
    - Added blur option in `VoxelGeneratorImage`
    - Generators now all inherit `VoxelGenerator`

- Blocky voxels
    - Blocky voxels can be defined with a custom mesh, sides get culled automatically even when not a cube
    - Blocky voxels can have custom collision AABBs for use with `VoxelBoxMover`
    - The 3D noise generator now works in blocky mode with `VoxelTerrain`
    - Blocky voxels support 8 bit and 16 bit depths

- Smooth voxels
    - Implemented Transvoxel mesher, supporting transition meshes for seamless LOD. It now replaces DMC by default.
    - Heightmap generators now have an `iso_scale` tuning property which helps reducing terracing

- Fixes
    - Fixed dark normals occasionally appearing on smooth terrains
    - Fixed Transvoxel mesher being offset compared to other meshers
    - Fixed stuttering caused by `ShaderMaterial` creation, now they are pooled
    - Fixed cases where `VoxelLodTerrain` was getting stuck when teleporting far away
    - Fixed a few bugs with `VoxelStreamBlockFiles` and `VoxelStreamRegionFiles`

- Breaking changes
    - Removed channel enum from `Voxel`, it was redundant with `VoxelBuffer`
    - Renamed `VoxelBuffer.CHANNEL_ISOLEVEL` => `CHANNEL_SDF`
    - Removed `VoxelIsoSurfaceTool`, superseded by `VoxelTool`
    - Removed `VoxelMesherMC`, superseded by `VoxelMesherTransvoxel`
    - Replaced `VoxelGeneratorTest` by `VoxelGeneratorWaves` and `VoxelGeneratorFlat`


03/10/2019 - `godot3.1`
------------------------

Initial reference version.

- Support for blocky, dual marching cubes and simple marching cubes meshers
- Terrain with constant LOD paging, or variable LOD (smooth voxels only)
- Voxels editable in game (limited to LOD0)
- Various simple streams to generate, save and load voxel terrain data
- Meshers and storage primitives available to use by scripts

...

- 01/05/2016 - Creation of the module, using Godot 3.0 beta

