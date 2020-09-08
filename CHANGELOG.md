Changelog
==============

This is a high-level changelog for what has been worked on so far.

At the moment, this module doesn't have a distinct release schedule, so this changelog follows Godot's version numbers and binary releases. Each version mentionned here should have an associated Git branch containing features at the time of the version. Backports aren't usually done so far, but PRs are welcome if it's necessary.

Semver is not yet in place, so each version can have breaking changes, although it shouldn't happen often.


`godot3.2.3` - 08/09/2020
---------------------------

- General
    - Terrain nodes now render in the editor, unless scripts are involved (can be changed with an option)
    - Added per-voxel and per-block metadata, which are saved by file streams along with voxel data
    - `StringName` is now used when possible to call script functions, to reduce overhead
    - Exposed block serializer to allow encoding voxels for network or files from script
    - Added terrain methods to trigger saves explicitely
    - The module only prints debug logs if the engine is in verbose mode
    - `VoxelTerrain` now emit signals when blocks are loaded and unloaded

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


(no branch) 27/03/2020 Tokisan Games binary version
------------------------------------------------------

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


`godot3.1` - 03/10/2019
--------------------------

Initial reference version.

- Support for blocky, dual marching cubes and simple marching cubes meshers
- Terrain with constant LOD paging, or variable LOD (smooth voxels only)
- Voxels editable in game (limited to LOD0)
- Various simple streams to generate, save and load voxel terrain data
- Meshers and storage primitives available to use by scripts

...

- 01/05/2016 - Creation of the module, using Godot 3.0 beta
