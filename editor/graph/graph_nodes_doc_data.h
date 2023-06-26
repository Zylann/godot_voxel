// <GENERATED>
// clang-format off
namespace GraphNodesDocData {

struct Node {
    const char *name;
    const char *category;
    const char *description;
};

static const unsigned int COUNT = 59;
static const Node g_data[COUNT] = {
    {"Abs", "Math", "If [code]x[/code] is negative, returns [code]x[/code] as a positive number. Otherwise, returns [code]x[/code]."},
    {"Add", "Ops", "Returns the sum of [code]a[/code] and [code]b[/code]"},
    {"Clamp", "Math", "If [code]x[/code] is lower than [code]min[/code], returns [code]min[/code]. If [code]x[/code] is higher than [code]max[/code], returns [code]max[/code]. Otherwise, returns [code]x[/code]."},
    {"ClampC", "Math", "If [code]x[/code] is lower than [code]min[/code], returns [code]min[/code]. If [code]x[/code] is higher than [code]max[/code], returns [code]max[/code]. Otherwise, returns [code]x[/code].\nThis node is an alternative to [code]Clamp[/code], used internally as an optimization if [code]min[/code] and [code]max[/code] are constant."},
    {"Comment", "Misc", "A rectangular area with a description, to help organizing a graph."},
    {"Constant", "Misc", "Outputs a constant number."},
    {"Curve", "Mapping", "Returns the value of a custom [code]curve[/code] at coordinate [code]x[/code], where [code]x[/code] is in the range [code][0..1][/code]. The [code]curve[/code] is specified with a [url=Curve]Curve[/url] resource."},
    {"CustomInput", "Input", "Outputs values from the custom input having the same name as the node. May be used in [url=VoxelGraphFunction]VoxelGraphFunction[/url]. It won't be used in [url=VoxelGeneratorGraph]VoxelGeneratorGraph[/url]."},
    {"CustomOutput", "Output", "Sets the value of the custom output having the same name as the node. May be used in [url=VoxelGraphFunction]VoxelGraphFunction[/url]. It won't be used in [url=VoxelGeneratorGraph]VoxelGeneratorGraph[/url]."},
    {"Distance2D", "Vector", "Returns the distance between two 2D points [code](x0, y0)[/code] and [code](x1, y1)[/code]."},
    {"Distance3D", "Vector", "Returns the distance between two 3D points [code](x0, y0, z0)[/code] and [code](x1, y1, z1)[/code]."},
    {"Divide", "Ops", "Returns the result of [code]a / b[/code].\nNote: dividing by zero outputs NaN. It should not cause crashes, but will likely mess up results. Consider using Multiply when possible."},
    {"Expression", "Math", "Evaluates a math expression. Variable names can be written as inputs of the node. Some functions can be used, but they must be supported graph nodes in the first place, as the expression will be converted to nodes internally.\nAvailable functions:\n[code]\nsin(x)\nfloor(x)\nabs(x)\nsqrt(x)\nfract(x)\nstepify(x, step)\nwrap(x, length)\nmin(a, b)\nmax(a, b)\nclamp(x, min, max)\nlerp(a, b, ratio)\n[/code]"},
    {"FastNoise2D", "Noise", "Returns computation of 2D noise at coordinates [code](x, y)[/code] using the FastNoiseLite library. The [code]noise[/code] parameter is specified with an instance of the [url=ZN_FastNoiseLite]ZN_FastNoiseLite[/url] resource.\nNote: this node might be a little faster than [code]Noise2D[/code]."},
    {"FastNoise2_2D", "Noise", "Returns computation of 2D SIMD noise at coordinates [code](x, y)[/code] using the FastNoise2 library. The `noise` parameter is specified with an instance of the [url=FastNoise2]FastNoise2[/url] resource. This is the fastest noise currently supported."},
    {"FastNoise2_3D", "Noise", "Returns computation of 3D SIMD noise at coordinates [code](x, y, z)[/code] using the FastNoise2 library. The [code]noise[/code] parameter is specified with an instance of the [url=FastNoise2]FastNoise2[/url] resource. This is the fastest noise currently supported."},
    {"FastNoise3D", "Noise", "Returns computation of 3D noise at coordinates [code](x, y, z)[/code] using the FastNoiseLite library. The [code]noise[/code] parameter is specified with an instance of the [url=ZN_FastNoiseLite]ZN_FastNoiseLite[/url] resource.\nNote: this node might be a little faster than [code]Noise3D[/code]."},
    {"FastNoiseGradient2D", "Noise", "Warps 2D coordinates [code](x, y)[/code] using a noise gradient from the FastNoiseLite library. The [code]noise[/code] parameter is specified with an instance of the [url=FastNoiseLiteGradient]FastNoiseLiteGradient[/url] resource."},
    {"FastNoiseGradient3D", "Noise", "Warps 3D coordinates [code](x, y, z)[/code] using a noise gradient from the FastNoiseLite library. The [code]noise[/code] parameter is specified with an instance of the [url=FastNoiseLiteGradient]FastNoiseLiteGradient[/url] resource."},
    {"Floor", "Math", "Returns the result of [code]floor(x)[/code], the nearest integer that is equal or lower to [code]x[/code]."},
    {"Fract", "Math", "Returns the decimal part of [code]x[/code]. The result is always positive regardless of sign."},
    {"Function", "Misc", "Runs a custom function, like a re-usable sub-graph. The first parameter (parameter 0) of this node is a reference to a [url=VoxelGraphFunction]VoxelGraphFunction[/url]. Further parameters (starting from 1) are those exposed by the function."},
    {"Image", "Mapping", "Returns the value of the red channel of an image at coordinates [code](x, y)[/code], where [code]x[/code] and [code]y[/code] are in pixels and the return value is in the range `[0..1]` (or more if the image has an HDR format). If coordinates are outside the image, they will be wrapped around. No filtering is performed. The image must have an uncompressed format."},
    {"InputSDF", "Input", "Outputs the existing signed distance at the current voxel. This may only be used in specific situations, such as using the graph as a procedural brush."},
    {"InputX", "Input", "Outputs the X coordinate of the current voxel."},
    {"InputY", "Input", "Outputs the Y coordinate of the current voxel."},
    {"InputZ", "Input", "Outputs the Z coordinate of the current voxel."},
    {"Max", "Math", "Returns the highest value between [code]a[/code] and [code]b[/code]."},
    {"Min", "Math", "Returns the lowest value between [code]a[/code] and [code]b[/code]."},
    {"Mix", "Math", "Interpolates between [code]a[/code] and [code]b[/code], using parameter value [code]t[/code]. If [code]t[/code] is [code]0[/code], [code]a[/code] will be returned. If [code]t[/code] is [code]1[/code], [code]b[/code] will be returned. If [code]t[/code] is beyond the [code][0..1][/code] range, the returned value will be an extrapolation."},
    {"Multiply", "Ops", "Returns the result of [code]a * b[/code]."},
    {"Noise2D", "Noise", "Returns 2D noise at coordinates `(x, y)` using one of the [url=Noise]Noise[/url] subclasses provided by Godot."},
    {"Noise3D", "Noise", "Returns 3D noise at coordinates `(x, y, z)` using one of the [url=Noise]Noise[/url] subclasses provided by Godot."},
    {"Normalize", "Vector", "Returns the normalized coordinates of the given [code](x, y, z)[/code] 3D vector, such that the length of the output vector is 1."},
    {"OutputSDF", "Output", "Sets the Signed Distance Field value of the current voxel."},
    {"OutputSingleTexture", "Output", "Sets the texture index of the current voxel. This is an alternative to using [code]OutputWeight[/code] nodes, if your voxels only have one texture. This is easier to use but does not allow for long gradients. Using this node in combination with [code]OutputWeight[/code] is not supported."},
    {"OutputType", "Output", "Sets the TYPE index of the current voxel. This is for use with [url=VoxelMesherBlocky]VoxelMesherBlocky[/url]. If you use this output, you don't need to use [code]OutputSDF[/code]."},
    {"OutputWeight", "Output", "Sets the value of a specific texture weight for the current voxel. The texture is specified as an index with the [code]layer[/code] parameter. There can only be one output using a given layer index."},
    {"Pow", "Math", "Returns the result of the power function ([code]x ^ power[/code]). It can be relatively slow."},
    {"Powi", "Math", "Returns the result of the power function ([code]x ^ power[/code]), where the exponent is a constant positive integer. May be faster than [code]Pow[/code]."},
    {"Relay", "Misc", "Pass-through node, allowing to better organize the path of long connections."},
    {"Remap", "Math", "For an input value [code]x[/code] in the range [code][min0, max0][/code], converts linearly into the [code][min1, max1][/code] range. For example, if [code]x[/code] is [code]min0[/code], then [code]min1[/code] will be returned. If [code]x[/code] is [code]max0[/code], then [code]max1[/code] will be returned. If [code]x[/code] is beyond the [code][min0, max0][/code] range, the result will be an extrapolation."},
    {"SdfBox", "SDF", "Returns the signed distance field of an axis-aligned box centered at the origin, of size [code](size_x, size_y, size_z)[/code], at coordinates [code](x, y, z)[/code]."},
    {"SdfPlane", "SDF", "Returns the signed distance field of a plane facing the Y axis located at a given [code]height[/code], at coordinate [code]y[/code]."},
    {"SdfPreview", "SDF", "Debug node, not used in the final result. In the editor, shows a slice of the values emitted from the output it is connected to, according to boundary [code][min_value, max_value][/code]. The slice will be either along the XY plane or the XZ plane, depending on current settings."},
    {"SdfSmoothSubtract", "SDF", "Subtracts signed distance field [code]b[/code] from [code]a[/code], using the same smoothing as with the [code]SdfSmoothUnion[/code] node."},
    {"SdfSmoothUnion", "SDF", "Returns the smooth union of two signed distance field values [code]a[/code] and [code]b[/code]. Smoothness is controlled with the [code]smoothness[/code] parameter. Higher smoothness will create a larger \"weld\" between the shapes formed by the two inputs."},
    {"SdfSphere", "SDF", "Returns the signed distance field of a sphere centered at the origin, of given [code]radius[/code], at coordinates [code](x, y, z)[/code]."},
    {"SdfSphereHeightmap", "SDF", "Returns an approximation of the signed distance field of a spherical heightmap, at coordinates [code](x, y, z)[/code]. The heightmap is an [code]image[/code] using panoramic projection, similar to those used for environment sky in Godot. The radius of the sphere is specified with [code]radius[/code]. The heights from the heightmap can be scaled using the [code]factor[/code] parameter. The image must use an uncompressed format."},
    {"SdfTorus", "SDF", "Returns the signed distance field of a torus centered at the origin, facing the Y axis, at coordinates [code](x, y, z)[/code]. The radius of the ring is [code]radius1[/code], and its thickness is [code]radius2[/code]."},
    {"Select", "Math", "If [code]t[/code] is lower than [code]threshold[/code], returns [code]a[/code]. Otherwise, returns [code]b[/code]. "},
    {"Sin", "Math", "Returns the result of [code]sin(x)[/code]"},
    {"Smoothstep", "Math", "Returns the result of smoothly interpolating the value of [code]x[/code] between [code]0[/code] and [code]1[/code], based on the where [code]x[/code] lies with respect to the edges [code]egde0[/code] and [code]edge1[/code]. The return value is [code]0[/code] if [code]x <= edge0[/code], and [code]1[/code] if [code]x >= edge1[/code]. If [code]x[/code] lies between [code]edge0[/code] and [code]edge1[/code], the returned value follows an S-shaped curve that maps [code]x[/code] between [code]0[/code] and [code]1[/code]. This S-shaped curve is the cubic Hermite interpolator, given by [code]f(y) = 3*y^2 - 2*y^3[/code] where [code]y = (x-edge0) / (edge1-edge0)[/code]."},
    {"Spots2D", "Noise", "Cellular noise optimized for \"ore patch\" generation: divides space into a 2D grid where each cell contains a circular \"spot\". Returns 1 when the position is inside the spot, 0 otherwise. [code]jitter[/code] more or less randomizes the position of the spot inside each cell. Limitation: high jitter can make spots clip with cell borders. This is intentional. If you need more generic cellular noise, use another node."},
    {"Spots3D", "Noise", "Cellular noise optimized for \"ore patch\" generation: divides space into a 3D grid where each cell contains a circular \"spot\". Returns 1 when the position is inside the spot, 0 otherwise. [code]jitter[/code] more or less randomizes the position of the spot inside each cell. Limitation: high jitter can make spots clip with cell borders. This is intentional. If you need more generic cellular noise, use another node."},
    {"Sqrt", "Math", "Returns the square root of [code]x[/code].\nNote: unlike classic square root, if [code]x[/code] is negative, this function returns [code]0[/code] instead of [code]NaN[/code]."},
    {"Stepify", "Math", "Snaps [code]x[/code] to a given step, similar to GDScript's function [code]stepify[/code]."},
    {"Subtract", "Ops", "Returns the result of [code]a - b[/code]"},
    {"Wrap", "Math", "Wraps [code]x[/code] between [code]0[/code] and [code]length[/code], similar to GDScript's function [code]wrapf(x, 0, max)[/code].\nNote: if [code]length[/code] is 0, this node will return [code]NaN[/code]. If it happens, it should not crash, but results will be messed up."},
};

} // namespace GraphNodesDocData
// clang-format on
// </GENERATED>
