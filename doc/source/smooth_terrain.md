Smooth terrains
===================

It is possible to work with smooth-looking terrains, using signed distance fields and `VoxelMesherTransvoxel`.


Signed distance fields
-------------------------

### Concept

In order to represent smooth terrain, using a grid with points being either 0 or 1 is not enough. Such a grid is good for polygonizing blocky surfaces, but not curves. It could be averaged or blurred somehow, but this would be expensive.

For any point in space, a signed distance field (SDF) is a distance to the closest surface. It is "signed" because if the point is below a surface ("inside of something"), that distance becomes negative. That means the surface is defined by all the points for which the SDF is 0. That `0` is usually called the `isolevel`.

SDF is commonly used in raymarching shaders as a way to define volumes. It is also used in the Transvoxel algorithm (a variant of Marching Cubes), which is implemented by this engine. So instead of discrete values, voxels store a sort of smooth "gradient" of distances.


### Technical explanation

Voxels use SDF in 3D, but to help visualizing it, so we'll have a look at 2D examples.
If we were to represent a sphere with blocky voxels, we would apply the following pseudo-code to every voxel:

```
if distance(center, position) < radius:
	voxel = 1
else:
	voxel = 0
```

Which gives the following:

![Blocky SDF](images/sdf_example_blocky.png)

Each voxel has binary values, either 1 or 0. But that gives no information about how the transition occurs between "matter" and "air", so if we were to render this using Transvoxel, the result would be:

![SDF sphere blocky](images/sdf_sphere_blocky.png)

It is kinda blocky. Now, we might indeed want this result (see section about shaders). But if we dont, we will need to change the code. In fact, if we walk back one step, the answer is already there:

```
voxel = distance(origin, position) - radius
```

This is the signed distance of a sphere. Here shown normalized, so voxels close to `0` are grey:

![True SDF](images/sdf_example_true.png)

Every voxel now contains a slowly changing gradient, so when Transvoxel marches through all cells to find the surface, it will see much more precise variations than just `0` or `1`, which allows it to produce smooth polygons.

![SDF sphere blocky](images/sdf_sphere_smooth.png)


### Scaling and clamping

This engine allows to edit voxels and save them. Storing true SDF can be expensive for games. For example, because it is a *distance*, if a player builds a small tower on the ground, we would have to keep voxels up to date far away in the sky, just because the tower made the ground slightly closer to them. So in practice, it is not required to deal with exact SDF. We only need something that's good enough, so the gradients can vary at different speeds and modifications can stay "local".

Voxels far away in the sky are actually not interesting for us. The surface is what we really need. So we can clamp distances, such that voxels far enough from the surface will have the same value. And if a whole chunk has the same value, it can be optimized out as "uniform".

So the sphere SDF we've seen earlier would actually look like this in the data:

![Clamped SDF](images/sdf_example_clamped.png)

Over multiple chunks, all regions without a gradient will take very little space in memory.

To save further memory, this engine does not store SDF using 32-bit `float` numbers (not by default). Instead, it uses either 8-bit or 16-bit integers, which are interpreted as fixed-point decimal numbers between -1 and 1. Anything lower or higher is clamped. That means the distance we want to store has to be scaled to best exploit this interval.

In practice, it means before storing SDF in a `VoxelBuffer`, we scale it by `0.1`, or lower when using 16-bit. The lower the scale, the longer the gradient will span before it gets clamped, but the more memory will be used to store that variation. It should not be too low either, because 16-bit cannot represent variations that are too small. This scale may need to be tweaked if you use a lot of LOD levels, because if voxels are seen from very far away, the gradient will need to extend for long enough to remain smooth.

For more information about SDF and other domains where they are used, you can check out some of these videos:

- [Glyphs, shapes, fonts, signed distance fields. (Martin Donald)](https://www.youtube.com/watch?v=1b5hIMqz_wM)
- [Coding Adventure: Marching Cubes (Sebastian Lague)](https://www.youtube.com/watch?v=M3iI2l0ltbE)
- [Painting a Cartoon Girl using Mathematics (Inigo Quilez)](https://www.youtube.com/watch?v=8--5LwHRhjk)
- [Code for a bunch of SDF functions and operations (Inigo Quilez)](https://iquilezles.org/articles/distfunctions/)


Transvoxel
-----------

### Definition

Transvoxel is an extension of Marching Cubes that can be used to create smooth meshes from voxel data. The advantage of this algorithm is to integrate stitching of different levels of details without causing cracks, so it can be used to render very large landscapes.

For more information, visit [https://transvoxel.org/](https://transvoxel.org/).


### Smooth stitches in vertex shader

Transvoxel uses special meshes to stitch blocks of different level of detail. However the seams may still be visible as occasional sharp little steps. To smooth this out a bit, meshes produced by `VoxelMesherTransvoxel` contain extra information in their `CUSTOM0` attribute, telling how to move vertices to smooth those steps, and make room for them in the regular part of the mesh.

Create and setup a `ShaderMaterial` on your terrain, and integrate this snippet to it:

```glsl
// This is recognized and assigned automatically by the voxel engine
uniform int u_transition_mask;

float get_transvoxel_secondary_factor(int idata) {
	int cell_border_mask = idata & 63; // Which sides the cell is touching
	int vertex_border_mask = (idata >> 8) & 63; // Which sides the vertex is touching
	// If the vertex is near a side where there is a low-resolution neighbor,
	// move it to secondary position
	int m = u_transition_mask & cell_border_mask;
	float t = float(m != 0);
	// If the vertex lies on one or more sides, and at least one side has no low-resolution neighbor,
	// don't move the vertex.
	t *= float((vertex_border_mask & ~u_transition_mask) == 0);
	return t;
}

vec3 get_transvoxel_position(vec3 vertex_pos, vec4 fdata) {
	int idata = floatBitsToInt(fdata.a);

	// Move vertices to smooth transitions
	float secondary_factor = get_transvoxel_secondary_factor(idata);
	vec3 secondary_position = fdata.xyz;
	vec3 pos = mix(vertex_pos, secondary_position, secondary_factor);

	// If the mesh combines transitions and the vertex belongs to a transition,
	// when that transition isn't active we change the position of the vertices so
	// all triangles will be degenerate and won't be visible.
	// This is an alternative to rendering them separately,
	// which has less draw calls and less mesh resources to create in Godot.
	// Ideally I would tweak the index buffer like LOD does but Godot does not
	// expose anything to use it that way.
	int itransition = (idata >> 16) & 0xff; // Is the vertex on a transition mesh?
	float transition_cull = float(itransition == 0 || (itransition & u_transition_mask) != 0);
	pos *= transition_cull;

	return pos;
}

void vertex() {
	VERTEX = get_transvoxel_position(VERTEX, CUSTOM0);
    //...
}
```

Research issue which led to this code: [Issue #2](https://github.com/Zylann/godot_voxel/issues/2)


Texturing
-----------

Texturing a voxel surface can be more difficult than classic 3D meshes, because the geometry isn't known in advance, and can have almost any shape. So in this section we'll review ways to solve UV-mapping, procedural techniques and blending textures from voxel data.

### Triplanar mapping

Classic UV-mapping cannot be used on smooth voxel surfaces, because of the arbitrary shapes it can contain. In fact, smooth meshers don't provide any proper UV. So instead, we can use triplanar mapping.

The method involves projecting the texture on to the part of object that directly faces the X-axis. Then projecting it on the sides that directly face the Y-axis. Then again for the Z-axis. The edges of these projections are then blended together with a specified sharpness.

Look at how the brick textures are blended together on the top right sphere.

![Triplanar mapping image](https://docs.godotengine.org/en/3.1/_images/spatial_material25.png)

Read about [triplanar mapping in Godot](https://docs.godotengine.org/en/latest/tutorials/3d/standard_material_3d.html#triplanar-mapping).

It is also possible to choose a different texture for the 3 axes.

Here's a shader that supports two materials, such as grass on the top and rock on the sides, each with triplanar mapped albedo, normal and AO maps, then blended together based on if their normal faces the upward direction or the sides.

You can find a working example in the [fps demo](https://github.com/tinmanjuggernaut/voxelgame), or see the [shader](https://github.com/tinmanjuggernaut/voxelgame/blob/master/project/fps_demo/materials/triplanar.shader) itself. 

In the shader parameters, add your two albedo maps, and optionally normal, and AO maps. Then play with the `AB Mix 1` and `AB Mix 2` sliders to adjust how the top and sides blend together. The other settings should be self explanatory. The screenshot below also has a little bit of fog and far DOF added.

![Textured terrain](images/textured-terrain.jpg)


### Procedural texturing

Voxel data is heavy, so if texturing rules of your game are simple enough to be determined from a shader and don't impact gameplay, you won't need to define any extra data in the voxels. For example, you can check the normal of a terrain surface to blend between a grass and rock texture, and use snow above a certain height.


### 4-blend over 16 textures

#### Voxel data

If you want textures to come from voxel data, `VoxelMesherTransvoxel` has a `texture_mode` property which can be set to `TEXTURES_BLEND_4_OVER_16`. This mode allows up to 16 textures and blends only the 4 most used ones per voxel. It expects voxel data in the `INDICES` and `WEIGHTS` channels, encoded into 16-bit depth values. There are 4 weights and 4 indices per voxel, each using 4 bits. It is very tight and does not allow for long gradients, but should be enough for most cases.

```
          1st byte    2nd byte
INDICES:  aaaa bbbb   cccc dddd
WEIGHTS:  aaaa bbbb   cccc dddd
```

By default, these channels default to indices `(0,1,2,3)` and weights `(1,0,0,0)`, meaning voxels always start with texture `0`.

The feature is recent and will need further work or changes in this area.
At the moment, indices and weights are mostly applied manually. It is possible to set them directly with `VoxelTool.set_voxel` but it is up to you to pack them properly. One easy way to paint is to use `VoxelTool.do_sphere()`:

```gdscript
# Paints texture 2 in a sphere area (does not create matter)
voxel_tool.set_mode(VoxelTool.MODE_TEXTURE_PAINT)
voxel_tool.set_texture_index(2)
voxel_tool.set_texture_opacity(1.0)
voxel_tool.do_sphere(hit_position, radius)
```

It is also possible to generate this in `VoxelGeneratorGraph` using special outputs, but it still requires a bit of math to produce valid data.

#### Mesh data

The mesher will include texturing information in the `CUSTOM1` attribute of vertices. Contrary to voxel values, the packed information will have 8 bits of precision:

- `CUSTOM1.x` will contain 4 indices, encoded as 4 bytes, which can be obtained by reinterpreting the float number as an integer and using bit-shifting operators.
- `CUSTOM1.y` will contain 4 weights, again encoded as 4 bytes.

Each index tell which texture needs to be used, and each weight respectively tells how much of that texture should be blended. It is essentially the same as a classic color splatmap, except textures can vary. One minor downside is that you cannot blend more than 4 textures per voxel, so if this happens, it might cause artifacts. But in practice, it is assumed this case is so infrequent it can be ignored.


#### Shader

Here is the shader code you will need:

```glsl
// Textures should preferably be in a TextureArray, so looking them up is cheap
uniform sampler2DArray u_texture_array : hint_albedo;

// We'll need to pass data from the vertex shader to the fragment shader
varying vec4 v_indices;
varying vec4 v_weights;
varying vec3 v_normal;
varying vec3 v_pos;

// We'll use a utility function to decode components.
// It returns 4 values in the range [0..255].
vec4 decode_8bit_vec4(float v) {
	uint i = floatBitsToUint(v);
	return vec4(
		float(i & uint(0xff)),
		float((i >> uint(8)) & uint(0xff)),
		float((i >> uint(16)) & uint(0xff)),
		float((i >> uint(24)) & uint(0xff)));
}

// A voxel mesh can have overhangs in any direction,
// so we may have to use triplanar mapping functions.
vec3 get_triplanar_blend(vec3 world_normal) {
	vec3 blending = abs(world_normal);
	blending = normalize(max(blending, vec3(0.00001))); // Force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	return blending / vec3(b, b, b);
}

vec4 texture_array_triplanar(sampler2DArray tex, vec3 world_pos, vec3 blend, float i) {
	vec4 xaxis = texture(tex, vec3(world_pos.yz, i));
	vec4 yaxis = texture(tex, vec3(world_pos.xz, i));
	vec4 zaxis = texture(tex, vec3(world_pos.xy, i));
	// blend the results of the 3 planar projections.
	return xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
}

void vertex() {
	// Indices are integer values so we can decode them as-is
	v_indices = decode_8bit_vec4(CUSTOM1.x);

	// Weights must be in [0..1] so we divide them
	v_weights = decode_8bit_vec4(CUSTOM1.y) / 255.0;

	v_pos = VERTEX;
	v_normal = NORMAL;

	//...
}

void fragment() {
	// Define a texture scale for convenience.
	// We can use an array instead if different scales per index is needed.
	float uv_scale = 0.5;

	// Sample the 4 blending textures, all with triplanar mapping.
	// We can re-use the same triplanar blending factors for all of them so separating that part
	// of the function improves performance a little.
	vec3 blending = get_triplanar_blend(v_normal);
	vec3 col0 = texture_array_triplanar(u_texture_array, v_pos * uv_scale, blending, v_indices.x).rgb;
	vec3 col1 = texture_array_triplanar(u_texture_array, v_pos * uv_scale, blending, v_indices.y).rgb;
	vec3 col2 = texture_array_triplanar(u_texture_array, v_pos * uv_scale, blending, v_indices.z).rgb;
	vec3 col3 = texture_array_triplanar(u_texture_array, v_pos * uv_scale, blending, v_indices.w).rgb;

	// Get weights and make sure they are normalized.
	// We may add a tiny safety margin so we can afford some degree of error.
	vec4 weights = v_weights;
	weights /= (weights.x + weights.y + weights.z + weights.w + 0.00001);

	// Calculate albedo
	vec3 col = 
		col0 * weights.r + 
		col1 * weights.g + 
		col2 * weights.b + 
		col3 * weights.a;

	ALBEDO = col;

	//...
}
```

![Smooth voxel painting prototype](images/smooth_voxel_painting_on_plane.png)

!!! note
	If you only need 4 textures, then you can leave indices to their default values (which contains `0,1,2,3`) and only use weights. When using `VoxelTool`, you may only use texture indices 0, 1, 2 or 3. Texture arrays are less relevant in this case.


### Recommended Reading

- [SpatialMaterial](https://docs.godotengine.org/en/stable/classes/class_spatialmaterial.html) - demonstrates many of the shader options available in Godot.
- [Shading Index](https://docs.godotengine.org/en/stable/tutorials/shading/index.html) - tutorials and the shader language API
- Shader API Reference - some of the most frequently accessed references
	- [Shading Language](https://docs.godotengine.org/en/stable/tutorials/shading/shading_reference/shading_language.html)
	- [SpatialShader](https://docs.godotengine.org/en/stable/tutorials/shading/shading_reference/spatial_shader.html)



Shading
---------

By default smooth voxels also produce smooth meshes by sharing vertices. This also contributes to meshes being smaller in memory.

### Low-poly / flat-shaded look

It is currently not possible to make the mesher produce vertices with split flat triangles, but you can use this in your fragment shader:

```glsl
NORMAL = normalize(cross(dFdx(VERTEX), dFdy(VERTEX)));
```

![Flat shading](images/flat_shading.png)

### Blocky look

It is also possible to give a "blocky" look to "smooth" voxels:

![Flat shading](images/blocky_sdf.png)

This can be done by saturating SDF values in the voxel generator: they have to be always -1 or 1, without transition values. Since values are clamped when using `set_voxel_f`, multiplying by a large number also works. Built-in basic generators might not have this option, but you can do it if you use your own generator script or `VoxelGeneratorGraph`.

You may also make shading hard-edged in your shader for better results.


### Special uniforms

If you use a `ShaderMaterial` on a voxel node, the module may exploit some uniform (shader parameter) names to provide extra information.

Parameter name             | Type     | Description
---------------------------|----------|-----------------------
`u_lod_fade`               | `vec2`   | Information for progressive fading between levels of detail. Only available with `VoxelLodTerrain`. See [Lod fading](#lod-fading-experimental)
`u_block_local_transform`  | `mat4`   | Transform of the rendered blockl, local to the whole volume, as they may be rendered with multiple meshes. Useful if the volume is moving, to fix triplanar mapping. Only available with `VoxelLodTerrain` at the moment.


Level of detail (LOD)
-----------------------

`VoxelLodTerrain` implements dynamic level of detail for smooth terrain.

### Description

LOD (Level Of Detail) is a technique used to change the amount of geometry dymamically, such that meshes close to the viewer have high definition, while meshes far from the viewer are simplified down. This aims at improving performance.

![LOD example](images/lod_example.png)

!!! note
	Careful: in this engine, `LOD` *levels* are frequently represented with numbers from `0` to `N-1`, where `N` is the number of LODs. `0` is the *highest level of detail*, while LOD `1`, `2` etc up to `N-1` are *lower levels of detail*.

![Illustration of level of detail with a grid of voxels](images/lod_density_schema.png)

When going from LOD `i` to `i+1`, voxels and blocks double in size, covering more space. However resolution of blocks doesn't change, so detail density is lower and consumes less resources.


### Octrees

LOD is implemented using a *grid of octrees*. Each octree may then be subdivided into chunks of variable size, where the smallest size will be LOD 0.

Subdivision occurs as the viewer gets closer. The threshold upon which it happens is controlled with the `lod_distance` property. It represents how far LOD 0 will spread around the viewer. It also affects how far other LODs will go, so it controls quality overall.

Similarly to `VoxelTerrain`, as the viewer moves around, octrees are loaded in front and those getting too far are unloaded. This allows to keep support for "infinite" terrain, without having to setup a single octree with unnecessary depth levels.

In the editor, gizmos are showing the *grid of octrees*. Block bounds can be shown by checking the `Terrain -> Show octree nodes` menu.

The size of the grid around the viewer depends on two factors:

- The `view_distance` parameter of `VoxelLodTerrain`
- The `view_distance` parameter on `VoxelViewer`.


### Number of LODs

Increasing the number of LODs allows the terrain to have larger octrees, which in turns allows to increase view distance. It does not actually make `LOD0` sharper, it goes the other way around (if you expected otherwise, maybe you need to tweak your generator to produce larger shapes, reduce voxel size or change the scales of your game which might be too small).
You might notice the grid of octrees changes size if you change LOD count: this is because it rounds to the current `view_distance`.

Reducing the number of LODs reduces the size of octrees, however it also means there will be much more of them to fill the grid up to `view_distance`. Make sure to keep a good sweetspot between LOD count and `view_distance` so that the density of octrees is not too high.

If you are not making an infinite terrain, you may give it fixed bounds with the `bounds` property, as well as a very large view distance so it stays in view.
`bounds` will be rounded to octree size: for example, with 4 LODs and mesh block size of 16, LOD0 blocks will be 16, LOD1 will be 32, LOD2 will be 64... and LOD3 (the biggest) will be 128. Since the current implementation keeps at minimum 8 octrees around the origin, optimal bounds for this setup would be 256.

![Screenshot of fixed bounds LOD terrain](images/fixed_bounds_octrees.png)

Following the same logic, fixed bounds of 512 is optimal with 5 LODs, 1024 is optimal with 6 LODs and so on.
This is based on mesh block size of `16`, so if you set it to `32`, you may set one less LOD since meshes are twice as big.

For information about LOD behavior in the editor, see [Camera options in editor](editor.md#camera-options).


### Voxel size

Currently, the size of voxels is fixed to 1 space unit. It might be possible in a future version to change it. For now, a workaround is to scale down the node. However, make sure it is a uniform scale, and careful not to scale too low otherwise it might blow up.

`scale` from Node3D must not be confused with the concept of *size*. If you change `scale`, *it will also scale the voxel grid*, view distances, all the dimensions you might have set in generators, and of course it will apply to child nodes as well. The result will *look the same*, just bigger, no more details. So if you want something larger *with more details as a result*, it is recommended to change these sizes instead of scaling everything.
For example, if your generator contains a sphere and Perlin noise, you may change the radius of the sphere and the frequency/period of the noise instead of scaling the node. Doing it this way preserve the size of voxels and so it preserves accuracy.

Godot also allows you to scale non-uniformly, but it's not recommended (might cause collision issues too).


### Full load mode

LOD applies both to meshes and to voxel data, keeping memory usage relatively constant. Depending on your settings, distant voxels will not load full-resolution data. Only full-resolution voxels can be edited, so that means you can only modify terrain in a limited distance around the viewer.

If this limitation isn't suitable for your game, a workaround is to enable `full_load_mode`. This will load all edited chunks present in the `stream` (if any), such that all the data is available and can be edited anywhere without wait. Non-edited chunks will cause the generator to be queried on the fly instead of being cached. Because data streaming won't take place, keep in mind more memory will be used the more edited chunks the terrain contains.


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

This technique is still imperfect because of several limitations:

- Transition meshes used to stitch blocks of different LOD are currently not faded. Doing so requires much more work, and in fact, the way these meshes are handled in the first place could be simplified if Godot allowed more fine-grained access to `ArrayMesh`, like switching to another index buffer without re-uploading the entire mesh.
- Shadow maps still create self-shadowing. While both meshes are rendered to cross-fade, one of them will eventually project shadows on the other. This creates a lot of noisy patches. Turning off shadows from one of them does not fix the other, and turning shadows off completely will make shadows pop. I haven't found a solution yet. See https://github.com/godotengine/godot-proposals/issues/692#issuecomment-782331429

