Changelog
============

This is a high-level list of features, changes and fixes that have been made over time.

At the moment, this module doesn't have a distinct release schedule, so this changelog follows Godot's version numbers and binary releases. Almost each version mentionned here should have an associated Git branch (for THIS repo, not Godot's) containing features at the time of the version. Backports aren't done so far.

Semver is not yet in place, so each version can have breaking changes, although it shouldn't happen often across minor versions.


1.x - ongoing development - `master`
--------------------------------------

- General
    - Added shadow casting setting to both terrain types
    - Added editor shortcut to re-generate the selected terrain
    - Updated FastNoise2 to 0.10.0-alpha
    - `VoxelGeneratorGraph`:
        - Added `Spots2D` and `Spots3D` nodes, optimized for generating "ore patches"
    - `VoxelTerrain`:
        - Added `VoxelTerrainMultiplayerSynchronizer`, which simplifies replication using Godot's high-level multiplayer API
    - `VoxelTool`:
        - Added `smooth_sphere`, which smoothens terrain in a spherical area using box blur. Smooth/SDF terrain only. (Thanks to Piratux for the idea and initial implementation)
        - Separated `paste` into `paste` and `paste_masked` functions. The latter performs masking using a specific channel and value.
    - `VoxelToolLodTerrain`:
        - Added support for `paste`
    - `VoxelMesherCubes`:
        - Added helper function to convert an image into a 1-voxel thick "sprite mesh"
    - `VoxelInstanceLibrary`:
        - Added `get_all_item_ids()` to allow iterating over all items of a library
    - `VoxelVoxLoader`:
        - Added parameter to allow loading data in a custom channel (instead of the color channel)

- Fixes
    - Fixed editor not shrinking properly on narrow screens with a terrain selected. Stats appearing in bottom panel will use a scrollbar if the area is too small.
    - `VoxelLodTerrain`: fixed error spam when re-generating or destroying the terrain
    - `VoxelGeneratorGraph`:
        - Fixed crash if a graph contains a node with both used and unused outputs, and gets compiled with `debug=false`
        - Fixed error when adding Constant nodes
    - `VoxelInstanceLibrary`: fixed `find_item_by_name` was not finding items


1.0 - 12/03/2023 - `godot4.0`
------------------------------

Godot 4 is required from this version.

- General
    - Added `gi_mode` to terrain nodes to choose how they behave with Godot's global illumination
    - Added `FastNoise2` for faster SIMD noise
    - Added experimental support functions to help setting up basic multiplayer with `VoxelTerrain` (might change in the future)
    - Improved support for 64-bit floats
    - Added `ZN_ThreadedTask` to allow running custom tasks using the thread pool system
    - Added `VoxelMeshSDF` to bake SDF from meshes, which can be used in voxel sculpting.
    - Mesh resources are now fully built on threads with the Godot Vulkan renderer
    - Editor: terrain bounds are now shown in the inspector as min/max instead of position/size
    - Added `do_hemisphere` to `VoxelToolTerrain` and `VoxelToolLodTerrain`, which can be used as flattening brush
    - `VoxelGeneratorGraph`: added support for outputting to the TYPE channel, allowing use with `VoxelMesherBlocky`
    - `VoxelGeneratorGraph`: editor: unconnected inputs show their default value directly on the node
    - `VoxelGeneratorGraph`: editor: allow to change the axes on preview nodes 3D slices
    - `VoxelGeneratorGraph`: editor: replace existing connection if dragging from/to an input port having one already
    - `VoxelGeneratorGraph`: editor: creating noise and curve nodes now auto-create their resource instead of coming up null
    - `VoxelGeneratorGraph`: editor: added pin button to keep the graph editor shown even after deselecting the terrain.
    - `VoxelGeneratorGraph`: editor: added popout button to open the graph editor in a separate window
    - `VoxelGeneratorGraph`: added comment nodes
    - `VoxelGeneratorGraph`: added relay nodes
    - `VoxelGeneratorGraph`: added custom functions with the new `VoxelGraphFunction` resource (initial implementation, has limitations)
    - `VoxelGeneratorGraph`: added `OutputSingleTexture` node for outputting a single texture index per voxel, as an alternative to weights. This is specific to smooth voxels.
    - `VoxelGeneratorGraph`: added math expression node
    - `VoxelGeneratorGraph`: added Pow and Powi nodes
    - `VoxelGeneratorGraph`: Clamp now accepts min and max as inputs. For the version with constant parameters, use ClampC (might be faster in the current state of things).
    - `VoxelGeneratorGraph`: Added per-node profiling detail to see which ones take most of the time
    - `VoxelGeneratorGraph`: Added "live update" option, to automatically re-generate the terrain when the graph is modified
    - `VoxelGeneratorGraph`: Some nodes have default input connections, so it's no longer required to connect them manually to (X,Y,Z) inputs
    - `VoxelGeneratorGraph`: Added minor optimization to share branches of nodes doing the same calculations
    - `VoxelInstancer`: Added support for `VoxelTerrain`. This means only LOD0 works, but mesh-LODs should work.
    - `VoxelInstancer`: Editor: added basic UI to see how many instances exist
    - `VoxelInstancer`: Allow to dump VoxelInstancer as scene for debug inspection
    - `VoxelInstancer`: Editor: instance chunks are shown when the node is selected
    - `VoxelInstancer`: Changing mesh block size should no longer make saved instances invalid if they were saved with a different mesh block size
    - `VoxelInstanceLibraryMultiMeshItem`: Support setting up mesh LODs from a scene with name `LODx` suffixes
    - `VoxelInstanceLibraryMultiMeshItem`: Support setting a scene directly, which is converted to multimesh at runtime (fixes a few workflow issues: updates automatically when scene changes, doesn't create mesh and texture copies in the `.tres` file when using imported scenes)
    - `VoxelLodTerrain`: exposed debug drawing options for development versions

- Smooth voxels
    - SDF data is now encoded with `inorm8` and `inorm16`, instead of an arbitrary version of `unorm8` and `unorm16`. Migration code is in place to load old save files, but *do a backup before running your project with the new version*.
    - `VoxelTool`: Added `set_sdf_strength()` to control brush strength when sculpting smooth voxels (previously acted as if it was 1.0)
    - `VoxelLodTerrain`: added *experimental* `full_load_mode`, in which all edited data is loaded at once, allowing any area to be edited anytime. Useful for some fixed-size volumes.
    - `VoxelLodTerrain`: Added optional calculation of distant normalmaps to improve LOD quality. It can also run on the GPU for faster execution (`VoxelGeneratorGraph` only).
    - `VoxelLodTerrain`: Editor: added option to show octree nodes in editor
    - `VoxelLodTerrain`: Editor: added option to show octree grid in editor, now off by default
    - `VoxelLodTerrain`: Added option to run a major part of the process logic into another thread
    - `VoxelLodTerrain`: added debug gizmos to see mesh updates
    - `VoxelToolLodTerrain`: added *experimental* `do_sphere_async`, an alternative version of `do_sphere` which defers the task on threads to reduce stutter if the affected area is big.
    - `VoxelToolLodTerrain`: added `stamp_sdf` function to place a baked mesh SDF on the terrain
    - `VoxelToolLodTerrain`: added `do_graph` to run a custom brush based on `VoxelGeneratorGraph` in a specific area. An `InputSDF` node was added in order to support SDF modifications.
    - `VoxelMesherTransvoxel`: initial support for deep SDF sampling, to affine vertex positions at low levels of details (slow and limited proof of concept for now).
    - `VoxelMesherTransvoxel`: Variable LOD: regular and transition meshes are now combined in one single mesh per chunk. A shader is required to render it, but creates far less mesh resources and reduces the amount of draw calls.

- Blocky voxels
    - `VoxelMesherBlocky`: materials are now unlimited and specified in each model, either as overrides or directly from mesh (You still need to consider draw calls when using many materials)
    - `VoxelMesherBlocky`: each model can have up to 2 materials (aka surfaces)
    - `VoxelMesherBlocky`: mesh collisions: added support for specifying which surfaces have collision
    - `VoxelBoxMover`: added basic support for stair climbing

- Fixes
    - `VoxelBlockyLibrary`: the editor no longer crashes some time after having null model entries.
    - `VoxelBlockyModel`: properties of the inspector were not refreshed when changing `geometry_type`
    - `VoxelBuffer`: frequently creating buffers with always different sizes no longer wastes memory
    - `VoxelGeneratorGraph`: editor: fix inspector starting to throw errors after deleting a node, as it is still inspecting it
    - `VoxelGeneratorGraph`: editor: fixed crash when connecting an SdfPreview node to an input. However this is not supported yet.
    - `VoxelGeneratorGraph`: editor: fixed wrong position of context menu in some dual monitor configurations
    - `VoxelGeneratorGraph`: editor: fixed an occasional random crash happening when nodes UI layouts are updated
    - `VoxelGeneratorGraph`: fixed Image2D node not accepting image formats L8 and LA8
    - `VoxelGeneratorGraph`: fixed memory leaks when the graph contains resources
    - `VoxelGeneratorGraph`: some specific node graphs were not ordered properly
    - `VoxelGeneratorGraph`: SmoothUnion and SmoothSubtract were causing branches to be incorrectly skipped by runtime optimization, leading to empty blocks
    - `VoxelGeneratorFlat`: fixed underground SDF values being 0 instead of negative
    - `VoxelInstancer`: fix instances not refreshing when an item is modified and the mesh block size is 32
    - `VoxelInstancer`: fix crash when removing an item from the library while an instancer node is using it
    - `VoxelInstancer`: fix errors when removing scene instances
    - `VoxelInstancer`: fix position issues when scene instances are saved
    - `VoxelInstancer`: fix position issues when instances are saved while mesh block size is set to 32
    - `VoxelLodTerrain`: fix `lod_fade_duration` property was not accepting decimal numbers
    - `VoxelLodTerrain`: Cracks no longer appear at seams when LOD fading is enabled
    - `VoxelMesherCubes`: editor: color mode is now a proper dropdown
    - `VoxelMesherCubes`: fixed raw color mode not working properly
    - `VoxelMesherCubes`: wrong alpha check between transparent and solid cubes
    - `VoxelMesherTransvoxel`: fixed surface not appearing if it lines up exactly at integer coordinates
    - `VoxelMesherTransvoxel`: fixed occasional holes and "spikes" in geometry in some specific configurations
    - `VoxelStreamScript`: fix voxel data not getting retrieved when `BLOCK_FOUND` is returned
    - `VoxelTerrain`: fixed `Condition "mesh_block == nullptr" is true` which could happen in some conditions
    - `VoxelTerrain`: changing a material now updates existing meshes instead of only new ones
    - `VoxelTool`: `raycast` locking up if you send a Vector3 containing NaN
    - `VoxelToolLodTerrain`: fix inconsistent result with integer `do_sphere` radius
    - `VoxelToolTerrain`: `run_blocky_random_tick` no longer snaps area borders to chunk borders in unintuitive ways

- Breaking changes
    - Some functions now take `Vector3i` instead of `Vector3`. If you used to send `Vector3` without `floor()` or `round()`, it can have side-effects in negative coordinates.
    - `VoxelTerrain`: the main way to specify materials is no longer here, but in meshers instead.
    - `VoxelLodTerrain`: `set_process_mode` and `get_process_mode` were renamed `set_process_callback` and `get_process_callback` (due to a name conflict)
    - `VoxelLodTerrain`: `has_block` was renamed `has_data_block`
    - `VoxelMesherTransvoxel`: Shader API: The data in `COLOR` and `UV` was moved respectively to `CUSTOM0` and `CUSTOM1` (old attributes no longer work for this use case)
    - `VoxelMesherTransvoxel`: Variable LOD: a shader is now required to properly render transitions
    - `Voxel` was renamed `VoxelBlockyModel`
    - `VoxelLibrary` was renamed `VoxelBlockyLibrary`
    - `VoxelVoxImporter` was renamed `VoxelVoxSceneImporter`
    - `VoxelInstanceLibraryItem` was renamed `VoxelInstanceLibraryMultiMeshItem`
    - `VoxelInstanceLibraryItemBase` was renamed `VoxelInstanceLibraryItem`
    - `VoxelServer`: renamed `VoxelEngine`
    - `VoxelStream`: renamed `emerge_block` => `load_voxel_block`
    - `VoxelStream`: renamed `immerge_block` => `save_voxel_block`
    - `VoxelStreamScript`: renamed `_emerge_block` => `_load_voxel_block`
    - `VoxelStreamScript`: renamed `_immerge_block` => `_save_voxel_block`
    - `VoxelGeneratorGraph`: the `Select` node's `threshold` port is now a parameter instead.
    - `FastNoiseLite` was renamed `ZN_FastNoiseLite`, as now Godot 4 comes with its own implementation, with a few differences.
    - Removed `VoxelStreamBlockFiles`

- Known issues
    - Some nodes and resources no longer start with predefined properties due to a warning introduced in Godot4 when properties are resources.
    - SDFGI does not work all the time and can only be forced to update by moving away and coming back, pre-generating the terrain, or toggling it off and on. This is a limitation of Godot not supporting well meshes created dynamically.


0.5.x - Legacy Godot 3 branch - `godot3.x`
------------------------------------

This branch is the last supporting Godot 3

- Smooth voxels
    - `VoxelLodTerrain`: added *experimental* `full_load_mode`, in which all edited data is loaded at once, allowing any area to be edited anytime. Useful for some fixed-size volumes.
    - `VoxelToolLodTerrain`: added *experimental* `do_sphere_async`, an alternative version of `do_sphere` which defers the task on threads to reduce stutter if the affected area is big.
    - `VoxelInstanceLibraryItem`: Support setting up mesh LODs from a scene with name `LODx` suffixes

- Fixes
    - `VoxelBuffer`: frequently creating buffers with always different sizes no longer wastes memory
    - `Voxel`: properties were not refreshed when changing `geometry_type`
    - `VoxelGeneratorGraph`: fixed Image2D node not accepting image formats L8 and LA8
    - `VoxelGeneratorGraph`: editor: fixed crash when connecting an SdfPreview node to an input. However this is not supported yet.
    - `VoxelGeneratorGraph`: fixed memory leaks when the graph contains resources
    - `VoxelTerrain`: fixed `Condition "mesh_block == nullptr" is true` which could happen in some conditions
    - `VoxelTool`: `raycast` locking up if you send a Vector3 containing NaN
    - `VoxelToolLodTerrain`: fix inconsistent result with integer `do_sphere` radius
    - `VoxelInstancer`: fix instances not refreshing when an item is modified and the mesh block size is 32
    - `VoxelInstancer`: fix crash when removing an item from the library while an instancer node is using it
    - `VoxelInstancer`: fix errors when removing scene instances
    - `VoxelStreamScript`: fix voxel data not getting retrieved when `BLOCK_FOUND` is returned
    - Terrain was not visible if a room/portals system was used. For now it is not culled by rooms.


0.5 - 06/11/2021 - `godot3.4`
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


0.4 - 09/05/2021 - `godot3.3`
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


0.3 - 08/09/2020 - `godot3.2.3`
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


0.2 - 27/03/2020 - (no branch) - Tokisan Games binary version
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


0.1 - 03/10/2019 - `godot3.1`
------------------------

Initial reference version.

- Support for blocky, dual marching cubes and simple marching cubes meshers
- Terrain with constant LOD paging, or variable LOD (smooth voxels only)
- Voxels editable in game (limited to LOD0)
- Various simple streams to generate, save and load voxel terrain data
- Meshers and storage primitives available to use by scripts

...

- 01/05/2016 - Creation of the module, using Godot 3.0 beta

