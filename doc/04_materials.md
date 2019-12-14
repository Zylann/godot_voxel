# Materials

The terrain can be textured simply by adding a material to channel 0 in the material section.

Here are some notes for better results.

## Recommended Reading

* [SpatialMatieral](https://docs.godotengine.org/en/3.1/tutorials/3d/spatial_material.html) - demonstrates many of the shader options available in Godot.
* [Shading Index](https://docs.godotengine.org/en/3.1/tutorials/shading/index.html) - tutorials and the shader language API
* Shader API Reference - some of the most frequently accessed references
  * [Shading Language](https://docs.godotengine.org/en/3.1/tutorials/shading/shading_reference/shading_language.html)
  * [SpatialShader](https://docs.godotengine.org/en/3.1/tutorials/shading/shading_reference/spatial_shader.html)


## Triplanar Mapping

Projecting a flat, 2D image texture onto a rounded terrain is like wrapping a piece of paper wrapped around a sphere. It often warps the texture in undesirable ways. Triplanar mapping is a wrapping method that provides a reasonable result for spheres and terrains. 

The method involves projecting the texture onto only part of object that faces the X-axis directly. Then projecting it on the sides that face the Y-axis directly. Then again for the Z-axis. The edges of these projections are then blended together with a specified sharpness.

Look at how the brick textures are blended together on the top right sphere.

![Triplanar mapping image](https://docs.godotengine.org/en/3.1/_images/spatial_material25.png)

Read about [triplanar mapping in Godot](https://docs.godotengine.org/en/3.1/tutorials/3d/spatial_material.html?highlight=triplanar%20#triplanar-mapping).

The algorithm to implement this is a little complicated, which gets far worse if you also wish to add normal maps and others. However there is a very easy way to do this in the section below.

## Creating A Material

Rather than writing your own shader from scratch, especially with triplanar mapping code, the recommended process is to create a SpatialMaterial, then optionally convert it to a ShaderMaterial.

1. Create a new SpatialMaterial.
1. Add an albedo texture, and if desired, normal map, ambient occlusion, etc.
1. Under UV1, turn on triplanar mapping and adjust triplanar sharpness as desired.
1. Scale and style the material as desired.

If you want to start without a texture and just want to use a color, try turning down roughness, or adding some metalic to give the surface some reflectivity. This will allow light to reflect off the curves of the terrain in the distance. Otherwise you'll just see an undifferentiated mass of color.

## Enable Built-In Ambient Occlusion
VoxelTerrain adds Ambient Occlusion to the vertex colors on blocky terrains. You can add this to your material by enabling the `Vertex Color/Use As Albedo` flag in your material. Below is what that looks like on an otherwise plain white material. AO can also be added to any terrain using the AO feature built in to materials.
<img src="https://github.com/tinmanjuggernaut/voxelgame/raw/master/screenshots/blocky-vertex-color.jpg" width="800" />


## How To View Live Changes To Materials
You can't see the terrain in the viewport, so there are two options to view your material live while making changes:
* Add a sphere or cube with the same material, then adjust the material in the editor. This option is OK, but the UV scale is usually different than the terrain, so it's not ideal.
* Run your scene, focus the camera on the terrain, reduce the game window and move it to the side, then move the editor window to the other side. With both the editor and game displayed simultaneously, you can adjust the material in the inspector panel and it will update live. All edits to a SpatialMaterial or the shader parameters of your ShaderMaterial will update live. Editing your shader code will not update live. (Though it may be possible to trigger the engine to recompile your shader code.)

## Convert To Shader Code

The SpatialMaterial is a very good base, but can only go so far. If you need further customization, you'll want to convert it to shader code.

1. Add the material to any object.
1. In the inspector for that object, click the down arrow next to the material.
1. Click convert to ShaderMaterial.

Your material now has shader code that produces the same material your SpatialMaterial did, including code for triplanar albedo and normal maps!

Note, you can't edit the shader code and see live changes. You must stop and restart your scene. However you can change shader parameters live.

## Advanced Shading

One of the best ways to learn about shaders is to pick apart and experiment with other's shader code.

Here's a shader that supports two materials, such as grass on the top and rock on the sides, each with triplanar mapped albedo, normal and AO maps, then blended together based on if their normal faces the upward direction or the sides.

You can find a working example in the [fps demo](https://github.com/tinmanjuggernaut/voxelgame), or see the [shader](https://github.com/tinmanjuggernaut/voxelgame/blob/master/project/fps_demo/materials/triplanar.shader) itself. 

In the shader parameters, add your two albedo maps, and optionally normal, and AO maps. Then play with the `AB Mix 1` and `AB Mix 2` sliders to adjust how the top and sides blend together. The other settings should be self explanatory. The screenshot below also has a little bit of fog and far DOF added.

<img src="images/textured-terrain.jpg" width="800" />


---
* [Next Page](05_collision.md)
* [Doc Index](01_get-started.md)
