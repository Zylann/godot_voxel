Smooth terrains
===================

It is possible to work with smooth-looking terrains, using signed distance fields and `VoxelMesherTransvoxel`.


Signed distance fields
-------------------------

### Concept

TODO 

### Scaled values

TODO 


Transvoxel
-----------

### Definition

Transvoxel is an extension of Marching Cubes that can be used to create smooth meshes from voxel data. The advantage of this algorithm is to integrate stitching of different levels of details without causing cracks, so it can be used to render very large landscapes.

For more information, visit [https://transvoxel.org/](https://transvoxel.org/).


### Smooth stitches in vertex shader

Transvoxel uses special meshes to stitch blocks of different level of detail. However the seams may still be visible as occasional sharp little steps. To smooth this out a bit, meshes produced by `VoxelMesherTransvoxel` contain extra information in their `COLOR` attribute, telling how to move vertices to smooth those steps, and make room for them in the regular part of the mesh.

Create and setup a `ShaderMaterial` on your terrain, and integrate this snippet to it:

```glsl
// This is recognized and assigned automatically by the voxel engine
uniform int u_transition_mask;

vec3 get_transvoxel_position(vec3 vertex_pos, vec4 vertex_col) {
	int border_mask = int(vertex_col.a);
	int cell_border_mask = border_mask & 63; // Which sides the cell is touching
	int vertex_border_mask = (border_mask >> 6) & 63; // Which sides the vertex is touching

	// If the vertex is near a side where there is a low-resolution neighbor,
	// move it to secondary position
	int m = u_transition_mask & (cell_border_mask & 63);
	float t = float(m != 0);

	// If the vertex lies on one or more sides, and at least one side has no low-resolution neighbor,
	// don't move the vertex.
	t *= float((vertex_border_mask & ~u_transition_mask) == 0);

	// Position to use when border mask matches
	vec3 secondary_position = vertex_col.rgb;
	return mix(vertex_pos, secondary_position, t);
}

void vertex() {
	VERTEX = get_transvoxel_position(VERTEX, COLOR);
    //...
}
```

Research issue which led to this code: [Issue #2](https://github.com/Zylann/godot_voxel/issues/2)


### Low-poly / hard-edged look

Use this in your fragment shader:

```glsl
NORMAL = normalize(cross(dFdx(VERTEX), dFdy(VERTEX)));
```


Texturing
-----------

At the moment smooth meshers don't provide UVs and texturing information, so this has to be done entirely using shaders. In the future it may be possible to convey information from generators or edited voxels to vertex attributes.

### Triplanar mapping

Applying a texture to a voxel terrain can be quite complicated if classic UV-mapping is used, because of the arbitrary shapes it can contain. So instead, we can use triplanar mapping.

The method involves projecting the texture on to the part of object that directly faces the X-axis. Then projecting it on the sides that directly face the Y-axis. Then again for the Z-axis. The edges of these projections are then blended together with a specified sharpness.

Look at how the brick textures are blended together on the top right sphere.

![Triplanar mapping image](https://docs.godotengine.org/en/3.1/_images/spatial_material25.png)

Read about [triplanar mapping in Godot](https://docs.godotengine.org/en/latest/tutorials/3d/spatial_material.html?highlight=triplanar%20#triplanar-mapping).

It is also possible to choose a different texture for the 3 axes.

Here's a shader that supports two materials, such as grass on the top and rock on the sides, each with triplanar mapped albedo, normal and AO maps, then blended together based on if their normal faces the upward direction or the sides.

You can find a working example in the [fps demo](https://github.com/tinmanjuggernaut/voxelgame), or see the [shader](https://github.com/tinmanjuggernaut/voxelgame/blob/master/project/fps_demo/materials/triplanar.shader) itself. 

In the shader parameters, add your two albedo maps, and optionally normal, and AO maps. Then play with the `AB Mix 1` and `AB Mix 2` sliders to adjust how the top and sides blend together. The other settings should be self explanatory. The screenshot below also has a little bit of fog and far DOF added.

![Textured terrain](images/textured-terrain.jpg)


### Recommended Reading

- [SpatialMaterial](https://docs.godotengine.org/en/stable/classes/class_spatialmaterial.html) - demonstrates many of the shader options available in Godot.
- [Shading Index](https://docs.godotengine.org/en/stable/tutorials/shading/index.html) - tutorials and the shader language API
- Shader API Reference - some of the most frequently accessed references
	- [Shading Language](https://docs.godotengine.org/en/stable/tutorials/shading/shading_reference/shading_language.html)
	- [SpatialShader](https://docs.godotengine.org/en/stable/tutorials/shading/shading_reference/spatial_shader.html)



Level of detail (LOD)
-----------------------

`VoxelLodTerrain` implements dynamic level of detail for smooth terrain.

### Description

TODO

### LOD fading (experimental)

LOD changes can introduce some mild "popping" in the landscape, which might be a bit disturbing. One way to attenuate this problem is to fade meshes when they switch from two different levels of details. When a "parent" mesh subdivides into higher-resolution "child" meshes, they can be both rendered at the same time for a brief period of time, while the parent fades out and the children fade in, and vice-versa.
This trick requires you to use a `ShaderMaterial` on `VoxelLodTerrain`, as the rendering part needs an extra bit of code inside the fragment shader.

`VoxelLodTerrain` has a property `lod_fade_duration`, expressed in seconds. By default it is `0`, which makes it inactive. Setting it to a small value like `0.25` will enable it.

In your shader, add the following uniform:

```glsl
// This is recognized and assigned automatically by the voxel node
uniform vec2 u_lod_fade;
```

Add also this function (unless you have it already):

```glsl
float get_hash(vec2 c) {
	return fract(sin(dot(c.xy, vec2(12.9898,78.233))) * 43758.5453);
}
```

And *at the end* of `fragment()`, add this:

```glsl
// Discard pixels progressively.
// It has to be last to workaround https://github.com/godotengine/godot/issues/34966
float h = get_hash(SCREEN_UV);
if (u_lod_fade.y > 0.5) {
	// Fade in
	if (u_lod_fade.x < h) {
		discard;
	}
} else {
	// Fade out
	if (u_lod_fade.x > h) {
		discard;
	}
}
```

Note: this is an example of implementation. There might be more optimized ways to do it.

This will discard such that pixels of the two meshes will be complementary without overlap. `discard` is used so the mesh can remain rendered in the same pass (usually the opaque pass).

!!! warn
	This technique is still imperfect because of several limitations:
	- Transition meshes used to stitch blocks of different LOD are currently not faded. Doing so requires much more work, and in fact, the way these meshes are handled in the first place could be simplified if Godot allowed more fine-grained access to `ArrayMesh`, like switching to another index buffer without re-uploading the entire mesh.
	- Shadow maps still create self-shadowing. While both meshes are rendered to cross-fade, one of them will eventually project shadows on the other. This creates a lot of noisy patches. Turning off shadows from one of them does not fix the other, and turning shadows off completely will make shadows pop. I haven't found a solution yet. See https://github.com/godotengine/godot-proposals/issues/692#issuecomment-782331429

