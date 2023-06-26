# VoxelGeneratorGraph nodes

This page lists all nodes that can be used in [VoxelGeneratorGraph](api/VoxelGeneratorGraph.md) and [VoxelGraphFunction](api/VoxelGraphFunction.md).

## Input

### CustomInput

Outputs: `value`

Outputs values from the custom input having the same name as the node. May be used in [VoxelGraphFunction](api/VoxelGraphFunction.md). It won't be used in [VoxelGeneratorGraph](api/VoxelGeneratorGraph.md).

### InputSDF

Outputs: `sdf`

Outputs the existing signed distance at the current voxel. This may only be used in specific situations, such as using the graph as a procedural brush.

### InputX

Outputs: `x`

Outputs the X coordinate of the current voxel.

### InputY

Outputs: `y`

Outputs the Y coordinate of the current voxel.

### InputZ

Outputs: `z`

Outputs the Z coordinate of the current voxel.

## Mapping

### Curve

Inputs: `x`
Outputs: `out`
Parameters: `curve`

Returns the value of a custom `curve` at coordinate `x`, where `x` is in the range `[0..1]`. The `curve` is specified with a [Curve](https://docs.godotengine.org/en/stable/classes/class_curve.html) resource.

### Image

Inputs: `x`, `y`
Outputs: `out`
Parameters: `image`

Returns the value of the red channel of an image at coordinates `(x, y)`, where `x` and `y` are in pixels and the return value is in the range `[0..1]` (or more if the image has an HDR format). If coordinates are outside the image, they will be wrapped around. No filtering is performed. The image must have an uncompressed format.

## Math

### Abs

Inputs: `x`
Outputs: `out`

If `x` is negative, returns `x` as a positive number. Otherwise, returns `x`.

### Clamp

Inputs: `x`, `min`, `max`
Outputs: `out`

If `x` is lower than `min`, returns `min`. If `x` is higher than `max`, returns `max`. Otherwise, returns `x`.

### ClampC

Inputs: `x`
Outputs: `out`
Parameters: `min`, `max`

If `x` is lower than `min`, returns `min`. If `x` is higher than `max`, returns `max`. Otherwise, returns `x`.
This node is an alternative to `Clamp`, used internally as an optimization if `min` and `max` are constant.

### Expression

Outputs: `out`
Parameters: `expression`

Evaluates a math expression. Variable names can be written as inputs of the node. Some functions can be used, but they must be supported graph nodes in the first place, as the expression will be converted to nodes internally.
Available functions:
```
sin(x)
floor(x)
abs(x)
sqrt(x)
fract(x)
stepify(x, step)
wrap(x, length)
min(a, b)
max(a, b)
clamp(x, min, max)
lerp(a, b, ratio)
```

### Floor

Inputs: `x`
Outputs: `out`

Returns the result of `floor(x)`, the nearest integer that is equal or lower to `x`.

### Fract

Inputs: `x`
Outputs: `out`

Returns the decimal part of `x`. The result is always positive regardless of sign.

### Max

Inputs: `a`, `b`
Outputs: `out`

Returns the highest value between `a` and `b`.

### Min

Inputs: `a`, `b`
Outputs: `out`

Returns the lowest value between `a` and `b`.

### Mix

Inputs: `a`, `b`, `ratio`
Outputs: `out`

Interpolates between `a` and `b`, using parameter value `t`. If `t` is `0`, `a` will be returned. If `t` is `1`, `b` will be returned. If `t` is beyond the `[0..1]` range, the returned value will be an extrapolation.

### Pow

Inputs: `x`, `p`
Outputs: `out`

Returns the result of the power function (`x ^ power`). It can be relatively slow.

### Powi

Inputs: `x`
Outputs: `out`
Parameters: `power`

Returns the result of the power function (`x ^ power`), where the exponent is a constant positive integer. May be faster than `Pow`.

### Remap

Inputs: `x`
Outputs: `out`
Parameters: `min0`, `max0`, `min1`, `max1`

For an input value `x` in the range `[min0, max0]`, converts linearly into the `[min1, max1]` range. For example, if `x` is `min0`, then `min1` will be returned. If `x` is `max0`, then `max1` will be returned. If `x` is beyond the `[min0, max0]` range, the result will be an extrapolation.

### Select

Inputs: `a`, `b`, `t`
Outputs: `out`
Parameters: `threshold`

If `t` is lower than `threshold`, returns `a`. Otherwise, returns `b`. 

### Sin

Inputs: `x`
Outputs: `out`

Returns the result of `sin(x)`

### Smoothstep

Inputs: `x`
Outputs: `out`
Parameters: `edge0`, `edge1`

Returns the result of smoothly interpolating the value of `x` between `0` and `1`, based on the where `x` lies with respect to the edges `egde0` and `edge1`. The return value is `0` if `x <= edge0`, and `1` if `x >= edge1`. If `x` lies between `edge0` and `edge1`, the returned value follows an S-shaped curve that maps `x` between `0` and `1`. This S-shaped curve is the cubic Hermite interpolator, given by `f(y) = 3*y^2 - 2*y^3` where `y = (x-edge0) / (edge1-edge0)`.

### Sqrt

Inputs: `x`
Outputs: `out`

Returns the square root of `x`.
Note: unlike classic square root, if `x` is negative, this function returns `0` instead of `NaN`.

### Stepify

Inputs: `x`, `step`
Outputs: `out`

Snaps `x` to a given step, similar to GDScript's function `stepify`.

### Wrap

Inputs: `x`, `length`
Outputs: `out`

Wraps `x` between `0` and `length`, similar to GDScript's function `wrapf(x, 0, max)`.
Note: if `length` is 0, this node will return `NaN`. If it happens, it should not crash, but results will be messed up.

## Misc

### Comment

Parameters: `text`

A rectangular area with a description, to help organizing a graph.

### Constant

Outputs: `value`
Parameters: `value`

Outputs a constant number.

### Function

Parameters: `_function`

Runs a custom function, like a re-usable sub-graph. The first parameter (parameter 0) of this node is a reference to a [VoxelGraphFunction](api/VoxelGraphFunction.md). Further parameters (starting from 1) are those exposed by the function.

### Relay

Inputs: `in`
Outputs: `out`

Pass-through node, allowing to better organize the path of long connections.

## Noise

### FastNoise2D

Inputs: `x`, `y`
Outputs: `out`
Parameters: `noise`

Returns computation of 2D noise at coordinates `(x, y)` using the FastNoiseLite library. The `noise` parameter is specified with an instance of the [ZN_FastNoiseLite](api/ZN_FastNoiseLite.md) resource.
Note: this node might be a little faster than `Noise2D`.

### FastNoise2_2D

Inputs: `x`, `y`
Outputs: `out`
Parameters: `noise`

Returns computation of 2D SIMD noise at coordinates `(x, y)` using the FastNoise2 library. The `noise` parameter is specified with an instance of the [FastNoise2](api/FastNoise2.md) resource. This is the fastest noise currently supported.

### FastNoise2_3D

Inputs: `x`, `y`, `z`
Outputs: `out`
Parameters: `noise`

Returns computation of 3D SIMD noise at coordinates `(x, y, z)` using the FastNoise2 library. The `noise` parameter is specified with an instance of the [FastNoise2](api/FastNoise2.md) resource. This is the fastest noise currently supported.

### FastNoise3D

Inputs: `x`, `y`, `z`
Outputs: `out`
Parameters: `noise`

Returns computation of 3D noise at coordinates `(x, y, z)` using the FastNoiseLite library. The `noise` parameter is specified with an instance of the [ZN_FastNoiseLite](api/ZN_FastNoiseLite.md) resource.
Note: this node might be a little faster than `Noise3D`.

### FastNoiseGradient2D

Inputs: `x`, `y`
Outputs: `out_x`, `out_y`
Parameters: `noise`

Warps 2D coordinates `(x, y)` using a noise gradient from the FastNoiseLite library. The `noise` parameter is specified with an instance of the [FastNoiseLiteGradient](https://docs.godotengine.org/en/stable/classes/class_fastnoiselitegradient.html) resource.

### FastNoiseGradient3D

Inputs: `x`, `y`, `z`
Outputs: `out_x`, `out_y`, `out_z`
Parameters: `noise`

Warps 3D coordinates `(x, y, z)` using a noise gradient from the FastNoiseLite library. The `noise` parameter is specified with an instance of the [FastNoiseLiteGradient](https://docs.godotengine.org/en/stable/classes/class_fastnoiselitegradient.html) resource.

### Noise2D

Inputs: `x`, `y`
Outputs: `out`
Parameters: `noise`

Returns 2D noise at coordinates `(x, y)` using one of the [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html) subclasses provided by Godot.

### Noise3D

Inputs: `x`, `y`, `z`
Outputs: `out`
Parameters: `noise`

Returns 3D noise at coordinates `(x, y, z)` using one of the [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html) subclasses provided by Godot.

### Spots2D

Inputs: `x`, `y`, `spot_radius`
Outputs: `out`
Parameters: `seed`, `cell_size`, `jitter`

Cellular noise optimized for "ore patch" generation: divides space into a 2D grid where each cell contains a circular "spot". Returns 1 when the position is inside the spot, 0 otherwise. `jitter` more or less randomizes the position of the spot inside each cell. Limitation: high jitter can make spots clip with cell borders. This is intentional. If you need more generic cellular noise, use another node.

### Spots3D

Inputs: `x`, `y`, `z`, `spot_radius`
Outputs: `out`
Parameters: `seed`, `cell_size`, `jitter`

Cellular noise optimized for "ore patch" generation: divides space into a 3D grid where each cell contains a circular "spot". Returns 1 when the position is inside the spot, 0 otherwise. `jitter` more or less randomizes the position of the spot inside each cell. Limitation: high jitter can make spots clip with cell borders. This is intentional. If you need more generic cellular noise, use another node.

## Ops

### Add

Inputs: `a`, `b`
Outputs: `out`

Returns the sum of `a` and `b`

### Divide

Inputs: `a`, `b`
Outputs: `out`

Returns the result of `a / b`.
Note: dividing by zero outputs NaN. It should not cause crashes, but will likely mess up results. Consider using Multiply when possible.

### Multiply

Inputs: `a`, `b`
Outputs: `out`

Returns the result of `a * b`.

### Subtract

Inputs: `a`, `b`
Outputs: `out`

Returns the result of `a - b`

## Output

### CustomOutput

Inputs: `value`

Sets the value of the custom output having the same name as the node. May be used in [VoxelGraphFunction](api/VoxelGraphFunction.md). It won't be used in [VoxelGeneratorGraph](api/VoxelGeneratorGraph.md).

### OutputSDF

Inputs: `sdf`

Sets the Signed Distance Field value of the current voxel.

### OutputSingleTexture

Inputs: `index`

Sets the texture index of the current voxel. This is an alternative to using `OutputWeight` nodes, if your voxels only have one texture. This is easier to use but does not allow for long gradients. Using this node in combination with `OutputWeight` is not supported.

### OutputType

Inputs: `type`

Sets the TYPE index of the current voxel. This is for use with [VoxelMesherBlocky](api/VoxelMesherBlocky.md). If you use this output, you don't need to use `OutputSDF`.

### OutputWeight

Inputs: `weight`
Parameters: `layer`

Sets the value of a specific texture weight for the current voxel. The texture is specified as an index with the `layer` parameter. There can only be one output using a given layer index.

## SDF

### SdfBox

Inputs: `x`, `y`, `z`
Outputs: `sdf`
Parameters: `size_x`, `size_y`, `size_z`

Returns the signed distance field of an axis-aligned box centered at the origin, of size `(size_x, size_y, size_z)`, at coordinates `(x, y, z)`.

### SdfPlane

Inputs: `y`, `height`
Outputs: `sdf`

Returns the signed distance field of a plane facing the Y axis located at a given `height`, at coordinate `y`.

### SdfPreview

Inputs: `value`
Parameters: `min_value`, `max_value`, `fraction_period`, `mode`

Debug node, not used in the final result. In the editor, shows a slice of the values emitted from the output it is connected to, according to boundary `[min_value, max_value]`. The slice will be either along the XY plane or the XZ plane, depending on current settings.

### SdfSmoothSubtract

Inputs: `a`, `b`
Outputs: `sdf`
Parameters: `smoothness`

Subtracts signed distance field `b` from `a`, using the same smoothing as with the `SdfSmoothUnion` node.

### SdfSmoothUnion

Inputs: `a`, `b`
Outputs: `sdf`
Parameters: `smoothness`

Returns the smooth union of two signed distance field values `a` and `b`. Smoothness is controlled with the `smoothness` parameter. Higher smoothness will create a larger "weld" between the shapes formed by the two inputs.

### SdfSphere

Inputs: `x`, `y`, `z`
Outputs: `sdf`
Parameters: `radius`

Returns the signed distance field of a sphere centered at the origin, of given `radius`, at coordinates `(x, y, z)`.

### SdfSphereHeightmap

Inputs: `x`, `y`, `z`
Outputs: `sdf`
Parameters: `image`, `radius`, `factor`

Returns an approximation of the signed distance field of a spherical heightmap, at coordinates `(x, y, z)`. The heightmap is an `image` using panoramic projection, similar to those used for environment sky in Godot. The radius of the sphere is specified with `radius`. The heights from the heightmap can be scaled using the `factor` parameter. The image must use an uncompressed format.

### SdfTorus

Inputs: `x`, `y`, `z`
Outputs: `sdf`
Parameters: `radius1`, `radius2`

Returns the signed distance field of a torus centered at the origin, facing the Y axis, at coordinates `(x, y, z)`. The radius of the ring is `radius1`, and its thickness is `radius2`.

## Vector

### Distance2D

Inputs: `x0`, `y0`, `x1`, `y1`
Outputs: `out`

Returns the distance between two 2D points `(x0, y0)` and `(x1, y1)`.

### Distance3D

Inputs: `x0`, `y0`, `z0`, `x1`, `y1`, `z1`
Outputs: `out`

Returns the distance between two 3D points `(x0, y0, z0)` and `(x1, y1, z1)`.

### Normalize

Inputs: `x`, `y`, `z`
Outputs: `nx`, `ny`, `nz`, `len`

Returns the normalized coordinates of the given `(x, y, z)` 3D vector, such that the length of the output vector is 1.

