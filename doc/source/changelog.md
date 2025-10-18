Changelog
============

This is a high-level list of features, changes and fixes that have been made over time.

At the moment, this module doesn't have a distinct release schedule, so this changelog follows Godot's version numbers and binary releases. Almost each version mentionned here should have an associated Git branch (for THIS repo, not Godot's) containing features at the time of the version. Backports aren't done so far.

I try to minimize breaking changes, but there are usually a few in each release which I list in detail, so watch out for that section.

Dev 1.5.1 - `master`
--------------------

- `VoxelGeneratorGraph`: 
    - Editor: added `Add Node` item to the context menu
    - Added support for domain warp on `FastNoiseLite` and `ZN_FastNoiseLite` resources when using GPU (previously required to prepend `FastNoiseLiteGradient` noise)
    - Added methods to get the index of node inputs and output by their name
    - Added `generate_image_from_sdf`
    - Added `raycast_sdf_approx` to find where surface is from a ray
- `VoxelStream`: added option to compress saves using ZSTD instead of LZ4

- Fixes
    - `VoxelTool`: 
        `run_blocky_random_tick`: fixed uniform blocks were not picked up (PR #794)
        `run_blocky_random_tick`: will now run over the remainder if voxel count is not divisible by batch size
    - `VoxelGeneratorGraph`: fixed `FastNoiseGradient` was incorrect when fractal type isn't `None` and using GPU
    - `VoxelGraphFunction`: fixed `set_node_default_input_by_name` would match parameters but it should have been inputs


1.5 - 16/09/2025 - tag `v1.5`
------------------------------

- Improvements
    - `VoxelBuffer`: added functions to rotate/mirror contents
    - `VoxelEngine`: added function to manually change thread count (thanks to wildlachs)
    - `VoxelGeneratorGraph`:
        - Editor: selected nodes and connections can be removed using a right-click context menu (may be long-press on Android)
        - implemented constant reduction, which slightly optimizes graphs running on CPU if they contain constant branches
    - `VoxelGeneratorHeightmap`: added `offset` property
    - `VoxelGraphFunction`: Editor: preview nodes should now work
    - `VoxelInstanceLibraryItem`: Exposed `floating_sdf_*` parameters to tune how floating instances are detected after digging ground around them.
    - `VoxelInstanceLibraryMultiMeshItem`: 
        - Added `removal_behavior` property to trigger something when instances get removed
        - Added `collision_distance` to only create colliders when below a certain distance to chunks
    - `VoxelInstanceGenerator`: 
        - Added an option to snap instances based on the voxel generator SDF (only available with `VoxelGeneratorGraph`).
        - Exposed threshold for voxel texture filtering 
        - Added falloff settings for height, slope and noise filtering, so density can fade progressively (issue 784).
        - Added noise threshold to expand or shrink areas filtered by noise.
    - `VoxelInstancer`: 
        - Added `remove_instances_in_sphere`
        - Added fading system so a shader can be used to fade instances as they load in and out
        - Slightly improved random spread of instances over triangles
    - `VoxelMesherBlocky`: added tint mode to modulate voxel colors using the `COLOR` channel.
    - `VoxelMesherTransvoxel`: added `Single` texturing mode, which uses only one byte per voxel to store a texture index. `VoxelGeneratorGraph` was also updated to include this mode.
    - `VoxelTool`: 
        - added `do_mesh` to replace `stamp_sdf`. Supported on terrains only.
        - The `channels_mask` parameter of `copy` and `paste` functions is now optional, defaulting to all channels
    - `VoxelTerrain`: added debug flag to draw locations of voxel metadatas
    - `FastNoise2`: 
        - Exposed `CELLULAR_VALUE` noise type 
        - Exposed properties to choose cell indices used in distance/value calculations
    - Build system: added options to turn off features when doing custom builds
    - Introduced `VoxelFormat` to allow overriding default channel depths (was required to use the new `Single` voxel textures mode)

- Fixes
    - `VoxelBlockyType`: fixed models added using `set_variant_model` were not always returned by `_variant_models_data` (however it can still happen for different reasons, check the documentation of `set_variant_model`)
    - `VoxelBlockyTypeLibrary`: 
        - fixed crash when setting `types` to empty array
        - fixed incorrect loading of the ID map when a type has more than two attributes
    - `VoxelBuffer`: fixed `copy_voxel_metadata_in_area` could crash in some cases with the error `Assertion failed: "is_position_valid(dst_pos)" is false.`
    - `VoxelInstancer`: 
        - Fixed instance removal failing randomly after at least one chunk gets unloaded
        - Fixed instances getting generated when digging down or building up in *already meshed* chunks that had no geometry before, when using `VoxelLodTerrain`
        - Fixed transition meshes should not be used as spawning surfaces, they caused density bias and position bias at chunk borders
        - Fixed error when loading a scene with an instancer set with `up_mode = SPHERE` (issue 786; `voxel_instancer.cpp:906 - Condition "_parent == nullptr" is true`)
        - Fixed some potential issues occurring after `mesh_block_size` gets modified while the terrain is already in the scene tree, where the instancer would not use the right block size (leading to `ERROR: Condition "render_to_data_factor <= 0 || render_to_data_factor > 2" is true`)
    - `VoxelGeneratorGraph`: 
        - Editor: fixed error sometimes printing after closing the graph editor
        - Editor: fixed error spam `Invalid param name` after editing a graph (in some yet unknown situations)
        - Editor: fixed node dialog didn't auto-select the first item when searching
        - Editor: decimal numbers that have no exact float representation are now displayed rounded instead of widening nodes excessively. Instead, the exact value is shown with a tooltip.
        - Editor: fixed node names were incorrectly translated
        - Fixed incorrect texture painting leading to black triangles when using Mixel4 with OutputSingleTexture and GPU generation
        - Fixed crash with specific setups where equivalent nodes are connected multiple times to equivalent ancestors (issue 783; `FATAL: Assertion failed: "p != equivalence" is false`)
        - Fixed error when creating multiple nodes referring to a common resource (`ERROR: Signal 'changed' is already connected to given callable 'VoxelGraphFunction::_on_subresource_changed'`)
        - Fixed incorrect "walls" showing up in areas that are assumed uniform by range analysis, when GPU generation is enabled (For example, when using a Select node to output SDF=1.0 in an area; commit: 350d3897dcec7e37016e4fb95851cd389947371b)
    - `VoxelMesherBlocky`: Fixed crash when invalid model IDs are present at chunk borders with `VoxelLodTerrain`
    - `VoxelMeshSDF`: Fixed error when baking from a non-indexed mesh (which is exceptionally the case with Godot's CSG nodes)
    - `VoxelMesherTransvoxel`: Fixed some incorrect geometry changes near positive LOD borders, notably when voxel textures are used. Edge cases remain but can be fixed with a shader hack for now.
    - `VoxelStreamRegionFiles`: GDExtension: fixed error creating directories
    - `VoxelStreamSQLite`: 
        - `preferred_coordinate_format` was incorrectly exposed (fixed thanks to @beicause)
        - Replaced error spam with a single warning when the stream has no path configured, notably when assigning a new stream in the editor
    - `VoxelTool`:
        - `is_area_editable` was off by one in size, and was always returning `true` if the size of the AABB had any component smaller than 1
        - `paste_masked` didn't check the right coordinates to clear metadata in destinations containing at least one. It also caused a spam of `get_voxel` being at invalid position
        - `paste_masked_writable_list` caused an index out of bounds error (spotted thanks to @HiperSlug)
        - `set_voxel_metadata`: terrains: fixed passing `null` wasn't erasing metadata like with `VoxelBuffers`. 
        - `copy`: fixed voxel metadata was not copied when the source is a terrain
    - `VoxelToolLodTerrain`: fixed `do_graph` tended to produce boxes when the transform was scaled and `sdf_strength` was not 1
    - `VoxelViewer`: reparenting (`remove_child` followed by `add_child`) should no longer reload terrain around the viewer
    - `VoxelAStarGrid3D`: fixed crash if `find_path` is called without setting a terrain first
    - `ZN_SpotNoise`: fixed `get_spot_positions_in_area` functions were not working outside of the (0,0) cell

- Breaking changes
    - `VoxelGeneratorGraph`: `SdfSphere` node: `radius` is now an input instead of a parameter (compat breakage only occurs if you used a script to set it: replace `set_node_param(id, 0, radius)` with `set_node_default_input(id, 3, radius)`)
    - `VoxelTool.set_voxel_metadata`: on terrains, passing `null` now erases metadata, instead of creating a metadata with the value `null`, to be consistent with `VoxelBuffer` and fix issue 773.


1.4.1 - 29/03/2025 - tag `v1.4.1`
--------------------------------------

- `VoxelToolMultipassGenerator`: implemented `get/set_voxel_metadata`

- Fixes
    - Fixed leak when GPU is enabled for voxel generation or detail normalmap rendering, which could lead to a crash after a while
    - Terrains no longer interpolate unnecessarily when physics interpolation is enabled (terrain is static so it is not supported for now).
    - `VoxelGeneratorGraph`: 
        - Fixed `Curve` node was incorrect when used on the GPU.
        - Fixed single-voxel queries not working with blocky voxels (notably fixes raycast in VoxelLodTerrain)
        - Editor: auto-connects stopped working when copying a node having some (like noise; workaround was to reload the graph)
    - `VoxelGeneratorMultipassCB`: Fixed voxel metadata wasn't preserved when outputting final blocks


1.4 - 03/03/2025 - tag `v1.4`
------------------------------

Primarily developped with Godot 4.4.

- `VoxelBlockyModel`: Added option to turn off "LOD skirts" when used with `VoxelLodTerrain`, which may be useful with transparent models
- `VoxelBlockyModelCube`: Added support for mesh rotation like `VoxelBlockyMesh` (prior to that, rotation buttons in the editor only swapped tiles around)
- `VoxelEngine`: Added the `tasks.gpu` entry to the dictionary returned by `get_stats`, which is useful for loading screens when GPU features are used (notably asynchronous compiling of compute shaders, which can delay generation)
- `VoxelBuffer`:
    - Added functions to create/update a `Texture3D` from the SDF channel
    - Added functions to get/set a whole channel as a raw `PackedByteArray`
- `VoxelInstanceGenerator`: 
    - Added `OnePerTriangle` emission mode
    - Added ability to filter spawning by voxel texture indices, when using `VoxelMesherTransvoxel` with `texturing_mode` set to `4-blend over 16 textures`
- `VoxelTool`: `raycast` also returns a `normal` based on voxel data (it may be different from a physics raycast in some cases)
- `VoxelToolLodTerrain`: Implemented raycast when the mesher is `VoxelMesherBlocky` or `VoxelMesherCubes`
- `VoxelMesherBlocky`: Added basic support for fluid models

- Fixes
    - Fixed potential deadlock when using detail rendering and various editing features (thanks to lenesxy, issue #693)
    - `VoxelInstanceLibrary`: Editor: reworked the way items are exposed as a Blender-style list. Now removing an item while the library is open as a sub-inspector is no longer problematic
    - `VoxelInstancer`: 
        - Fixed persistent instances reloading with wrong positions (in the air, underground...) when mesh block size is set to 32
        - Editor: fixed `!is_inside_world()` errors when editing a `VoxelBlockyLibrary` after deleting a `VoxelInstancer` that was using it
    - `VoxelLodTerrain`:
        - Fixed potential crash when when using the Clipbox streaming system with threaded update (thanks to lenesxy, issue #692)
        - Fixed blocks were saved with incorrect LOD index when they get unloaded using Clipbox, leading to holes and mismatched terrain (#691)
        - Fixed incorrect loading of chunks near terrain borders when viewers are far away from bounds, when using the Clipbox streaming system
    - `VoxelStreamSQLite`: fixed connection leaks (thanks to lenesxy, issue #713)
    - `VoxelTerrain`: 
        - Edits and copies across fixed bounds no longer behave as if terrain generates beyond (was causing "walls" to appear).
        - Viewers with collision-only should no longer cause visual meshes to appear
    - `VoxelGeneratorGraph`: 
        - Fixed wrong values when using `OutputWeight` with optimized execution map enabled, when weights are determined to be locally constant
        - Fixed occasional holes in terrain when using `FastNoise3D` nodes with the `OpenSimplex2S` noise type
        - Fixed shader generation error when using the `Distance3D` node (vec2 instead of vec3, thanks to scwich)
        - Fixed crash when assigning an empty image to the `Image` node
    - `VoxelMesherTransvoxel`: revert texturing logic that attempted to prevent air voxels from contributing, but was lowering quality. It is now optional as an experimental property.
    - `VoxelStreamSQLite`: Fixed "empty size" errors when loading areas with edited `VoxelInstancer` data
    - `VoxelTool`: `raycast`: when using blocky voxels, the returned `distance_along_ray` now accounts for non-cube voxels 
    - `VoxelVoxLoader`: Fixed loading `.vox` files saved with versions of MagicaVoxel following 0.99.7
    - `.vox` scene importer: disabled threaded import to workaround the editor freezing when saving meshes

- Breaking changes
    - `VoxelInstanceLibrary`: Items should no longer be accessed using generated properties (`item1`, `item2` etc). Use `get_item` instead.
    - `VoxelMesherTransvoxel`: Removed `deep_sampling` experimental option
    - `VoxelTool`: The `flat_direction` of `do_hemisphere` now points away from the flat side of the hemisphere (like its normal), instead of pointing towards it
    - `VoxelToolLodTerrain`: `raycast` used to take coordinates in terrain space. It is now in world space, for consistency with `VoxelToolTerrain`.


1.3 - 17/08/2024 - branch `1.3` - tag `v1.3.0`
----------------------------------------------

Primarily developped with Godot 4.3.

- Added project setting `voxel/ownership_checks` to turn off sanity checks done by certain virtual functions that pass an object (such as `_generate_block`). Relevant for C#, where the garbage collection model prevents such checks from working properly.
- `VoxelBuffer`: Added several functions to do arithmetic operations on all voxels
- `VoxelInstanceGenerator`: allow to set density beyond 1, up to 10, by typing it in the field
- `VoxelMesherBlocky`:
    - Can be used with `VoxelLodTerrain`. Basic support: meshes scale with LOD and LOD>1 chunks have extra geometry to reduce cracks between LODs
    - Added experimental "shadow occluders": generates quads on chunk sides if they are covered by opaque voxels, to force shadows to project in caves when  there is no surface for DirectionalLight to project from (see #622).
- `VoxelMesherTransvoxel`:
    - added `edge_clamp_margin` property to prevent triangles from becoming too small, at the cost of slightly lower fidelity
    - reverted removal of degenerate triangles
- `VoxelStreamSQLite`: Added option to change the coordinate format, now defaulting to a format allowing larger coordinates. Existing saves keep their original format.
- `VoxelToolLodTerrain`: added `run_blocky_random_tick`
- `VoxelViewer`: added `view_distance_vertical_ratio` to use different vertical view distance proportionally to the horizontal distance

- Fixes
    - `VoxelBlockyModelMesh`: Fixed materials present directly in the mesh resource were not applied (only overrides in the model or on the terrain were applied)
    - `VoxelBlockyType`: Fixed configuration warning about missing variants when there is a base model specified
    - `VoxelGeneratorGraph`: Fixed crash when using the `Image` node with a non-square image
    - `VoxelStreamSQLite`: 
        - Fixed `set_key_cache_enabled(true)` caused nothing to load
        - Fixed slow loading when the database path contains `res://` or `user://`
        - Fixed crash if the database has an invalid path and `flush()` is called after `set_key_cache_enabled(true)`
    - `VoxelInstancer`:
        - Fixed instances with LOD > 0 were generated on `VoxelTerrain` even though LOD isn't supported (ending up in weird positions). No instances should generate.
        - Fixed error spam in the editor when instancing the node without a terrain parent
    - `VoxelInstanceLibrary`: Fixed `Assertion failed: "p_id < 0 || p_id >= MAX_ID" is false` when removing items from a VoxelInstanceLibrary
    - `VoxelMeshSDF`: Fixed error in the editor when trying to visualize the last slice (which turns out to be off by 1)
    - `VoxelModifierMesh`: 
        - Fixed setting `isolevel` had no effect
        - Fixed missing configuration warning when parenting under `VoxelTerrain` (only `VoxelLodTerrain` is supported)

- Breaking changes
    - `VoxelBlockyLibrary`: removed deprecated method `get_voxel_index_from_name`, use `get_model_index_from_resource_name` instead
    - `VoxelBlockyModel`: removed `transparent` deprecated property, use `transparency_index` instead
    - `VoxelBuffer`: removed `optimize` deprecated method, use `compress_uniform_channels` instead
    - `VoxelRaycastResult`: position properties are now `Vector3i` instead of `Vector3` (they were always integer but forgot to change them when Godot introduced `Vector3i`)
    - `VoxelStream`:
        - removed `emerge_block` deprecated method, use `load_voxel_block` instead
        - removed `immerge_block` deprecated method, use `save_voxel_block` instead
    - `VoxelVoxLoader`: methods are now static, so no instance of the class need to be created
    - Removed `VoxelMesherDMC`


1.2 - 20/04/2024 - branch `1.2` - tag `v1.2.0`
------------------------------------------------

Primarily developped with Godot 4.2.

- Added `ZN_SpotNoise`, exposing the same algorithm as the `SpotNoise2D` and `SpotNoise3D` nodes of graph generators
- Saving with `save_all_modified_blocks` now automatically flushes eventual caches implemented by `VoxelStream` upon completion
- Added `VoxelStreamMemory`, which stores in memory instead of the filesystem. This is mainly for testing purposes.
- More memory allocations are now tracked by Godot (you might notice `OS.get_static_memory_usage()` returns slightly more)
- `VoxelBlockyModelMesh`: exposed `side_vertex_tolerance` to tune when geometry is considered on sides of the voxel
- `VoxelBuffer`: exposed `fill_area_f`
- `VoxelEngine`: added methods to get the version of the voxel engine
- `VoxelGeneratorGraph`: Added GPU support for the `Select` node
- `VoxelLodTerrain`:
    - `save_all_modified_blocks` now returns a completion tracker similar to `VoxelTerrain`
    - Added new optional LOD streaming system `Clipbox` (advanced settings):
        - Uses concentric boxes instead of octree traversal, although some logic remains similar to what an octree does
        - Uses less CPU
        - Supports multiple viewers
        - Supports collision-only viewers
        - Adds secondary LOD distance parameter controlling the extents of LOD1 and beyond, separately from LOD0 (unused in the legacy system)
        - Has its own limitations and pending improvements, may be addressed over time
        - The original system is now referenced as "Legacy Octree".
    - Debug drawing is now exposed as properties. Editor checkboxes were removed from the terrain menu
- `VoxelMesherTransvoxel`: textures from air voxels (SDF>0) no longer contribute to the mesh
- `VoxelStream`:
    - Added `flush` method to force writing to the filesystem in case the stream's implementation uses caching
- `VoxelStreamSQLite`: Added support for `user://` paths (via internal call to `ProjectSettings.globalize_path()`)
- `VoxelTool`:
    - Added `grow_sphere` as alternate way to progressively grow or shrink matter in a spherical region with smooth voxels (thanks to Piratux)
    - `do_box` with smooth voxels now uses a proper box SDF, to improve quality. Before it was a solid fill, which could cause artifacts
    - Added `paste_masked_writable_list`, which also determines what is copied based the destination voxels
- `VoxelToolBuffer`: edits are now allowed even if the affected area is partially out of bounds of the target buffer. Results will be clipped.
- `VoxelToolLodTerrain`:
    - Improved quality of `separate_floating_chunks` on smooth terrains by expanding cutting-off area to include more gradients

- Fixes
    - Fixed chunk loading was prioritized incorrectly around the player in specific game start conditions
    - Fixed `"plugins_list.has(p_plugin)" is true` errors in the editor, at the cost of slight behavior changes. This was caused by existing workarounds to prevent UIs from hiding unexpectedly, which were modified to avoid the error, but are still needed unfortunately.
    - Fixed error `Unimplemented _get_import_order in add-on` when importing `.vox` files
    - Fixed some corner cases where quickly leaving and coming back to an edited area would revert edits to their previous state, due to chunks reloading before those edits got saved asynchronously
    - Fixed possible artifacts near terrain borders when using generators that sometimes avoid filling the output buffer, assuming they are initialized to defaults (issue #603)
    - `VoxelBlockyModel`: Fixed `material_override_*` properties all acting like the same material
    - `VoxelBlockyTypeLibrary`: Fixed a crash when saving a library with a null type entry (thanks to ArchLinus)
    - `VoxelBoxMover`: Fixed performance slowdown when `VoxelBlockyLibrary` contains a lot of models.
    - `VoxelGeneratorGraph`: 
        - Fixed ambiguous voxel texture indices produced by `OutputSingleTexture` caused painting to fail in some situations
        - Fixed default input values of output nodes were always 0 when using GPU generation
        - Fixed crash when using 16 weight output nodes (which is the maximum)
        - Fixed error when using more than 12 weight output nodes
        - Fixed using a graph as brush wasn't working with some transforms
        - Fixed wrapping error with the `Image` node in negative coordinates
        - Fixed wrong behavior and crashes when generating chunks large enough to trigger the "subdivision" feature
    - `VoxelInstanceLibraryMultimeshItem`: fixed error when using "Update From Scene" and trying to undo/redo it
    - `VoxelStreamSQLite`: fixed crash when using `set_key_cache_enabled(true)`
    - `VoxelTool`: fixed `paste` wrongly printing an error despite working fine
    - `VoxelToolLodTerrain`: 
        - `do_point` and `set_voxel` were not always updating meshes near chunk borders, leaving holes
        - `get_voxel` would always return 0 in indices and weight channels if the area was never edited, data streaming is on and the generator is a `VoxelGeneratorGraph` producing single-texture information
        - `copy` would return incorrect buffers when used on non-edited areas when data streaming is on and a generator is assigned
        - Fixed errors printed when moving away from edited chunks while no stream is assigned
        - Fixed `separate_floating_chunks` was spawning chunks not exactly at the same place
        - Fixed `stamp_sdf` was randomly not working correctly

- Breaking changes
    - `VoxelBuffer`:
        - `get_voxel_f` and `set_voxel_f` now automatically rescale quantized values. They are no longer normalized in -1..1 and may represent signed distances, so no need to scale manually (imprecisions caused by fixed-point encoding still applies).
        - `debug_print_sdf_y_slices` now returns a typed array instead of untyped array
    - `VoxelGeneratorGraph`: the fix to `Image` coordinate wrapping means results will be different than the previous broken version (the broken version was partially offsetting the image in negative coordinates)
    - `VoxelGraphFunction`: some members of the `NodeTypeID` enum have changed value. However, this enum's values shouldn't be used raw, and shouldn't be used in saves.
    - `VoxelStream`: save and load methods for voxels now take a position in blocks instead of a position in voxels
    - `VoxelTool`: due to the automatic internal SDF rescaling, if you modify `sdf_scale`, you may have to adjust it (or remove it if you set it to 0.002).
    - `VoxelToolMultipassGenerator`: changed `get_editable_area_max` to return an exclusive position instead of inclusive


1.1 - 29/12/2023 - branch `1.1` - tag `v1.1.0`
-----------------------------------------------

Primarily developped with Godot 4.1

- General
    - Added shadow casting setting to both terrain types
    - Added editor shortcut to re-generate the selected terrain
    - Added support for block generation on the GPU (only available with generators that support both CPU and GPU, for now `VoxelGeneratorGraph` only).
    - Updated FastNoise2 to 0.10.0-alpha
    - Started an experimental type system for the blocky voxels workflow. However it is not fully functional, its API may change in the future or have parts removed.
    - Added experimental `VoxelAStarGrid3D` for grid-based pathfinding on blocky voxels 
    - Added experimental `VoxelGeneratorMultipassCB` to implement column-based generation in multiple passes that works across chunks
    - Added `render_layers_mask` property to `VoxelTerrain` and `VoxelLodTerrain`
    - Voxel engine processing no longer stops when the SceneTree is paused
    - `VoxelGeneratorGraph`:
        - Added `Spots2D` and `Spots3D` nodes, optimized for generating "ore patches"
        - Added shader support for `FastNoiseGradient2D` and `FastNoiseGradient3D` nodes
        - Added bilinear filter option to the `Image` node
        - Editor: reworked context menu to add nodes, similar to VisualShader. Now has search bar, tree view and node descriptions.
        - Editor: added copy/paste with Ctrl+C/Ctrl+V shortcuts
    - `VoxelGraphFunction`:
        - Editor: the graph will now get compiled while editing, which provides some checks
        - Editor: I/Os are setup automatically when the graph is compiled by default. Manual setup might be exposed in the future if necessary.
    - `VoxelTerrain`:
        - Added `VoxelTerrainMultiplayerSynchronizer`, which simplifies replication using Godot's high-level multiplayer API
        - Added `is_area_meshed` as an alternative to `VoxelTool.is_area_editable` for games using mesh colliders
        - Added `do_path` to build or carve "worms" of varying radius
        - Editor: added warning when bounds are empty
    - `VoxelTool`:
        - Added `smooth_sphere`, which smoothens terrain in a spherical area using box blur. Smooth/SDF terrain only. (Thanks to Piratux for the idea and initial implementation)
        - Separated `paste` into `paste` and `paste_masked` functions. The latter performs masking using a specific channel and value.
    - `VoxelToolTerrain`:
        - Raycasting a terrain using `VoxelMesherBlocky` now takes collision boxes into account (thanks to Lry722)
    - `VoxelToolLodTerrain`:
        - Added support for `paste`
    - `VoxelMesherCubes`:
        - Added helper function to convert an image into a 1-voxel thick "sprite mesh"
    - `VoxelInstancer`:
        - Added `get_library_item_id` on multimesh instances with physics body nodes, so raycasts hitting them can tell which item they are
    - `VoxelInstanceGenerator`:
        - Added noise graph property, so instances can also be filtered with a custom VoxelGraphFunction
    - `VoxelInstanceLibrary`:
        - Added `get_all_item_ids()` to allow iterating over all items of a library
    - `VoxelLibraryMultiMeshItem `:
        - Added `render_layer` property (thanks to m4nu3lf)
        - Added `gi_mode` property
        - Exposed custom distance ratios for the secondary distance-based LOD system
        - Added option to hide instances when beyond their max distance-based LOD (only relevant for terrains with no LOD, or on the last LOD of `VoxelLodTerrain`)
        - Node groups on the template scene are now added to instance colliders if present
    - `VoxelLodTerrain`:
        - Added debug drawing for modifier bounds
        - Added `is_area_meshed` as an alternative to `VoxelTool.is_area_editable` for games using mesh colliders
        - Terrain now updates when the `material` property is assigned in the editor
        - Editor: added warning when bounds are empty
    - `VoxelVoxLoader`:
        - Added parameter to allow loading data in a custom channel (instead of the color channel)
    - `VoxelBlockyModel`:
        - Added 3D preview in editor
        - Added ability to rotate the model in editor (not just for preview, actually rotate the baked model)
        - Changed names to be handled with `Resource.name`, so it also shows them in the list of models in the editor
        - Added `culls_neighbors` property to control whether the sides of a model can cull sides of other models (thanks to spazzylemons)

- Fixes
    - Fixed editor not shrinking properly on narrow screens with a terrain selected. Stats appearing in bottom panel will use a scrollbar if the area is too small.
    - `VoxelBoxMover`: Workaround floating point error with stair climbing, which could cause characters to fall through the step (depends on gameplay code inducing errors when converting the returned motion)
    - `VoxelGeneratorGraph`:
        - Fixed crash if a graph contains a node with both used and unused outputs, and gets compiled with `debug=false`
        - Fixed error when adding Constant nodes
        - Fixed graph not always saving when saving the scene
        - Fixed shader generator crash when a node has an unconnected input
        - Fixed cellular noise when used on GPU
        - Fixed Image node issues when sampling at negative coordinates
    - `VoxelGraphFunction`:
        - Fixed default input values were not properly loaded
        - Fixed unexpected "missing node" error when more than one custom inputs are used
    - `VoxelInstancer`:
        - Fixed crash when hiding the node in the editor
        - Fixed crash when closing the scene while an instancer node is selected
        - Fixed instances were not cleared when using the "Re-generate" menu in the editor when terrain shape changed
    - `VoxelInstanceLibrary`: 
        - Fixed `find_item_by_name` was not finding items
        - Fixed newly added items in the editor rendering badly by default when the terrain doesn't have LOD. For now they always default to LOD 0 instead of LOD 2.
    - `VoxelTerrain`: Fixed crash when the terrain tries to update while it has no mesher assigned
    - `VoxelLodTerrain`: Fixed error spam when re-generating or destroying the terrain
    - `VoxelMesherBlocky`: Fixed materials "wrapping around" when more than 256 are used. Raised limit to 65536.
    - `VoxelMesherTransvoxel`: Removed rare degenerate/microscopic triangles, which caused errors with Jolt Physics. However, doing those checks makes meshing about 15% slower (untextured).
    - `VoxelStreamRegionFiles`: Fixed `block_size_po2` wasn't working correctly
    - `VoxelToolTerrain`: Fixed terrain was not marked as modified when setting voxel metadata
    - `VoxelToolLodTerrain`: 
        - Fixed `stamp_sdf` wasn't working due to an error when providing a baked mesh
        - Fixed `set_voxel` was creating artifacts
        - Fixed `separate_floating_chunks` was creating artifacts
    - `VoxelMeshSDF`: fixed saved resource was not loading properly

- Breaking changes
    - `VoxelBlockyLibrary`:
        - Changed the list of models to be handled by a typed array instead of individual properties. When opened in the editor, old resources will get converted. Re-save them to make the conversion persist.
    - `VoxelBlockyModel`:
        - The class was split into several subclasses for each type of geometry. When opened in the editor, old resources will get converted, but only if they are part of a `VoxelBlockyLibaray`. They won't work if they are individual resource files.
    - `VoxelNode`:
        - Removed `GIMode` enum, replaced with `GeometryInstance3D.GIMode`


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
    - `VoxelGeneratorGraph`:
        - added support for outputting to the TYPE channel, allowing use with `VoxelMesherBlocky`
        - editor: unconnected inputs show their default value directly on the node
        - editor: allow to change the axes on preview nodes 3D slices
        - editor: replace existing connection if dragging from/to an input port having one already
        - editor: creating noise and curve nodes now auto-create their resource instead of coming up null
        - editor: added pin button to keep the graph editor shown even after deselecting the terrain.
        - editor: added popout button to open the graph editor in a separate window
        - added comment nodes
        - added relay nodes
        - added custom functions with the new `VoxelGraphFunction` resource (initial implementation, has limitations)
        - added `OutputSingleTexture` node for outputting a single texture index per voxel, as an alternative to weights. This is specific to smooth voxels.
        - added math expression node
        - added Pow and Powi nodes
        - Clamp now accepts min and max as inputs. For the version with constant parameters, use ClampC (might be faster in the current state of things).
        - Added per-node profiling detail to see which ones take most of the time
        - Added "live update" option, to automatically re-generate the terrain when the graph is modified
        - Some nodes have default input connections, so it's no longer required to connect them manually to (X,Y,Z) inputs
        - Added minor optimization to share branches of nodes doing the same calculations
    - `VoxelInstancer`:
        - Added support for `VoxelTerrain`. This means only LOD0 works, but mesh-LODs should work.
        - Editor: added basic UI to see how many instances exist
        - Allow to dump VoxelInstancer as scene for debug inspection
        - Editor: instance chunks are shown when the node is selected
        - Changing mesh block size should no longer make saved instances invalid if they were saved with a different mesh block size
    - `VoxelInstanceLibraryMultiMeshItem`:
        - Support setting up mesh LODs from a scene with name `LODx` suffixes
        - Support setting a scene directly, which is converted to multimesh at runtime (fixes a few workflow issues: updates automatically when scene changes, doesn't create mesh and texture copies in the `.tres` file when using imported scenes)
    - `VoxelLodTerrain`: exposed debug drawing options for development versions

- Smooth voxels
    - SDF data is now encoded with `inorm8` and `inorm16`, instead of an arbitrary version of `unorm8` and `unorm16`. Migration code is in place to load old save files, but *do a backup before running your project with the new version*.
    - `VoxelTool`: Added `set_sdf_strength()` to control brush strength when sculpting smooth voxels (previously acted as if it was 1.0)
    - `VoxelLodTerrain`: added *experimental* `full_load_mode`, in which all edited data is loaded at once, allowing any area to be edited anytime. Useful for some fixed-size volumes.
    - `VoxelLodTerrain`: Added optional calculation of distant normalmaps to improve LOD quality. It can also run on the GPU for faster execution (`VoxelGeneratorGraph` only).
    - `VoxelLodTerrain`:
        - Editor: added option to show octree nodes in editor
        - Editor: added option to show octree grid in editor, now off by default
        - Added option to run a major part of the process logic into another thread
        - added debug gizmos to see mesh updates
    - `VoxelToolLodTerrain`:
        - added *experimental* `do_sphere_async`, an alternative version of `do_sphere` which defers the task on threads to reduce stutter if the affected area is big.
        - added `stamp_sdf` function to place a baked mesh SDF on the terrain
        - added `do_graph` to run a custom brush based on `VoxelGeneratorGraph` in a specific area. An `InputSDF` node was added in order to support SDF modifications.
    - `VoxelMesherTransvoxel`:
        - initial support for deep SDF sampling, to affine vertex positions at low levels of details (slow and limited proof of concept for now).
        - Variable LOD: regular and transition meshes are now combined in one single mesh per chunk. A shader is required to render it, but creates far less mesh resources and reduces the amount of draw calls.

- Blocky voxels
    - `VoxelMesherBlocky`:
        - materials are now unlimited and specified in each model, either as overrides or directly from mesh (You still need to consider draw calls when using many materials)
        - each model can have up to 2 materials (aka surfaces)
        - mesh collisions: added support for specifying which surfaces have collision
    - `VoxelBoxMover`: added basic support for stair climbing

- Fixes
    - `VoxelBlockyLibrary`: the editor no longer crashes some time after having null model entries.
    - `VoxelBlockyModel`: properties of the inspector were not refreshed when changing `geometry_type`
    - `VoxelBuffer`: frequently creating buffers with always different sizes no longer wastes memory
    - `VoxelGeneratorGraph`:
        - editor: fix inspector starting to throw errors after deleting a node, as it is still inspecting it
        - editor: fixed crash when connecting an SdfPreview node to an input. However this is not supported yet.
        - editor: fixed wrong position of context menu in some dual monitor configurations
        - editor: fixed an occasional random crash happening when nodes UI layouts are updated
        - fixed Image2D node not accepting image formats L8 and LA8
        - fixed memory leaks when the graph contains resources
        - some specific node graphs were not ordered properly
        - SmoothUnion and SmoothSubtract were causing branches to be incorrectly skipped by runtime optimization, leading to empty blocks
    - `VoxelGeneratorFlat`: fixed underground SDF values being 0 instead of negative
    - `VoxelInstancer`:
        - fix instances not refreshing when an item is modified and the mesh block size is 32
        - fix crash when removing an item from the library while an instancer node is using it
        - fix errors when removing scene instances
        - fix position issues when scene instances are saved
        - fix position issues when instances are saved while mesh block size is set to 32
    - `VoxelLodTerrain`:    
        - fix `lod_fade_duration` property was not accepting decimal numbers
        - Cracks no longer appear at seams when LOD fading is enabled
    - `VoxelMesherCubes`:
        - editor: color mode is now a proper dropdown
        - fixed raw color mode not working properly
        - wrong alpha check between transparent and solid cubes
    - `VoxelMesherTransvoxel`:
        - fixed surface not appearing if it lines up exactly at integer coordinates
        - fixed occasional holes and "spikes" in geometry in some specific configurations
    - `VoxelStreamScript`: fix voxel data not getting retrieved when `BLOCK_FOUND` is returned
    - `VoxelTerrain`:
        - fixed `Condition "mesh_block == nullptr" is true` which could happen in some conditions
        - changing a material now updates existing meshes instead of only new ones
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
    - `VoxelStream`:
        - renamed `emerge_block` => `load_voxel_block`
        - renamed `immerge_block` => `save_voxel_block`
    - `VoxelStreamScript`:
        - renamed `_emerge_block` => `_load_voxel_block`
        - renamed `_immerge_block` => `_save_voxel_block`
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
    - `VoxelGeneratorGraph`:
        - fixed Image2D node not accepting image formats L8 and LA8
        - editor: fixed crash when connecting an SdfPreview node to an input. However this is not supported yet.
        - fixed memory leaks when the graph contains resources
    - `VoxelTerrain`: fixed `Condition "mesh_block == nullptr" is true` which could happen in some conditions
    - `VoxelTool`: `raycast` locking up if you send a Vector3 containing NaN
    - `VoxelToolLodTerrain`: fix inconsistent result with integer `do_sphere` radius
    - `VoxelInstancer`:
        - fix instances not refreshing when an item is modified and the mesh block size is 32
        - fix crash when removing an item from the library while an instancer node is using it
        - fix errors when removing scene instances
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
    - `VoxelMesherTransvoxel`:
        - Initial support for texturing data in voxels, using 4-bit indices and weights
    - `VoxelMesherTransvoxel`:
        - optimized hot path, making it about 20% faster
        - added option to simplify meshes using MeshOptimizer
    - `VoxelToolLodTerrain`: 
        - added `copy` function
        - added `get_voxel_f_interpolated` function, useful to obtain interpolated SDF
        - added a function to separate floating chunks as rigid-bodies in a specified region
    - `VoxelInstanceGenerator`: added extra option to emit from faces more precisely, especially when meshes got simplified (slower than the other options)
    - `VoxelInstancer`:
        - added menu to setup a multimesh item from a scene (similarly to GridMap), which also allows to set up colliders
        - added initial support for instancing regular scenes (slower than multimeshes)
        - added option to turn off random rotation
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
    - `VoxelGeneratorGraph`:
        - changes to node properties are now saved properly
        - fix some per-thread memory not freed on exit
        - `debug_analyze_range` was crashing when given a negative-size area
    - `VoxelBuffer`:
        - `copy_voxel_metadata_in_area` was checking the source box incorrectly
        - multiple calls to `create()` with different sizes could lead to heap corruption if a channel was not uniform
        - `copy_channel_from_area` could lead to heap corruption if the source and destination had the same size and were copied entirely
    - `VoxelMesherTransvoxel`: no longer crashes when the input buffer is not cubic
    - `VoxelLodTerrain`:
        - fixed errors and crashes when editing voxels near loading borders
        - fixed crash occurring after a few edits when LOD count is set to 1
        - fixed crash when `run stream in editor` is turned off while the terrain is loading in editor
    - `VoxelTool` channel no longer defaults to 7 when using `get_voxel_tool` from a terrain with a stream assigned. Instead it picks first used channel of the mesher (fallback order is mesher, then generator, then stream).
    - `VoxelInstancer`:
        - fixed error when node visibility changes
        - fixed no instances generated when density is 1 in vertex emission mode 
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

