Generators
=============

`VoxelGenerator` allows to generate voxels given a specific area in space, or from a single position. They can serve as a base to automate the creation of large landscapes, or can be used in a game to generate worlds.


How generators work
----------------------

Generators currently run on the CPU and primarily work on blocks of voxels. For example, given a `VoxelBuffer` of 16x16x16 voxels, they decide what value each one will take. Using blocks makes it easier to split the work across multiple threads, and focus only on the area the player is located, especially if the world is infinite.

Voxel data is split into various channels, so depending on the kind of volume to generate, one or more different channels will be used. For example, a Minecraft generator will likely use the `TYPE` channel for voxel types, while a smooth terrain generator will use the `SDF` channel to fill in distance field values.

Generators have a thread-safe API. The same generator `generate_block` method may be used by multiple threads at once. However, depending on the class, some parameters might only be modifiable from the main thread, so check the documentation to be sure.

If a volume is not given a generator, blocks will be filled with air by default.


Basic generators
-------------------

The module provides several built-in generators. They are simple examples to get a quick result, and showing how the base API can be implemented (see source code).

Some of these generators have an option to choose which channel they will work on. If you use a smooth mesher, use the `SDF` channel (1), otherwise use the `TYPE` channel (0).

The following screenshots use a smooth `VoxelLodTerrain`.

### [Flat](api/VoxelGeneratorFlat.md)

Generates a flat ground.

![Screenshot of flat generator](images/generator_flat.png)

### [Waves](api/VoxelGeneratorWaves.md)

Generates waves.

![Screenshot of waves generator](images/generator_waves.png)

### [Image](api/VoxelGeneratorImage.md)

Generates a heightmap based on an image, repeated infinitely.

![Screenshot of image generator](images/generator_image.png)

!!! note
    With this generator, an `Image` resource is required. By default, Godot imports image files as `StreamTexture`. You may change this in the Import dock. At time of writing, in Godot 3, this requires an editor restart.

### [Noise2D](api/VoxelGeneratorNoise2D.md)

Generates a heightmap based on fractal noise.

![Screenshot of 2D noise generator](images/generator_noise2d.png)

### [Noise (3D)](api/VoxelGeneratorNoise3D)

Generates a blobby terrain with overhangs using 3D fractal noise. A gradient is applied along height so the volume becomes air when going up, and closes down into matter when going down.

![Screenshot of 3D noise generator](images/generator_noise3d.png)


Node-graph generators (VoxelGraphs)
------------------------------------

Basic generators may often not be suited to make a whole game from, but you don't necessarily need to program one. If you need smooth terrain, a graph-based generator is available, which offers a very customizable approach to make procedural volumes.

This generator currently doesn't support blocky terrains.

### Concept

TODO

### Examples

TODO

### Performance tuning

This generator uses a number of optimization strategies to make the calculations faster. You may want to fine-tune them in some cases depending on the kind of volume you want to generate.

#### Buffer processing

TODO

#### Range analysis

TODO

#### SDF clipping

TODO

#### Local optimization

TODO

#### Subdivision

TODO


Custom generator
-----------------

See [Scripting](scripting.md)
