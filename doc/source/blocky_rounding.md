# Optional rounded block edges

Rounding is stored on each model already present in a library's array. Set
`VoxelBlockyModel.rounding_enabled` to opt in; its default is `false`. This works
for `VoxelBlockyLibrary.models` and for the base/variant models of
`VoxelBlockyTypeLibrary.types`. Automatically rotated variants keep the flag.

## Shared controls

Both library classes inherit these settings from `VoxelBlockyLibraryBase`:

| Property | Default | Meaning |
| --- | --- | --- |
| `rounding_amount` | `0.1` | Radius in voxel units, from 0 through 0.25. Values below 0.001 resolve to zero and restore the original geometry. |
| `rounding_taper_distance` | `0.4` | Length of a protected edge transition, from 0.01 through 1 voxel. |
| `rounding_taper_profile` | `ROUNDING_TAPER_LINEAR` | Linear, ease-in, ease-out, or smoothstep. |

All enabled models share one radius and taper. Neighboring materials therefore
meet at the same cross-section. A length of `0.8` means 80% of an edge; it does
not mean an 80% bevel radius. If both ends of an edge need protection and the
tapers overlap, their factors multiply symmetrically.

Profiles use normalized distance `t` from a protected square endpoint toward
the full bevel:

| Profile | Radius multiplier | Shape |
| --- | --- | --- |
| Linear | `t` | Constant rate; abrupt slope change where the full bevel begins. |
| Ease In | `t*t` | Starts slowly, then rises quickly. |
| Ease Out | `t*(2-t)` | Starts quickly, then approaches the full bevel gently. |
| Smoothstep | `t*t*(3-2*t)` | Zero slope at both ends of the transition. |

## GDScript setup

Configure the library before attaching it to an active terrain:

```gdscript
var library: VoxelBlockyLibrary = preload("res://blocks/voxel_library.tres").duplicate(true)
var selected_models: Array[String] = ["grass", "dirt", "planks", "log_y"]

var rounded: Array[bool] = []
rounded.resize(library.models.size())
rounded.fill(false)
for id in library.models.size():
    var model: VoxelBlockyModel = library.models[id]
    if model != null:
        rounded[id] = model.resource_name in selected_models
        model.rounding_enabled = rounded[id]

library.rounding_amount = 0.1
library.rounding_taper_distance = 0.8
library.rounding_taper_profile = VoxelBlockyLibraryBase.ROUNDING_TAPER_SMOOTHSTEP
library.bake()

var mesher := VoxelMesherBlocky.new()
mesher.library = library
$VoxelTerrain.mesher = mesher
```

For a type library, apply the flag to `type.base_model`, or to each explicit
variant model. Do not add a block-state attribute just to store this rendering
option; that would unnecessarily multiply model IDs.

At runtime, rebake the changed library and assign a duplicated mesher to the
terrain to request a remesh through the existing API:

```gdscript
library.bake()
terrain.mesher = terrain.mesher.duplicate()
```

Changing a shared resource affects all terrains using it. Duplicate the library
and its models first when a terrain needs independent settings.

## Geometry and compatibility

All 12 cube edges are treated equally, including the underside. An exposed edge
is rounded only when both adjoining cells are empty space or native fluids.
If its continuation meets a square model or a blocked concave contact, the
bevel tapers to an exact square at that contact. The same rule applies along X,
Y and Z. Mesh blocks need only the existing one-voxel halo.

Supported shapes are full unit `VoxelBlockyModelCube` models and imported cube
meshes with two triangles per face and consistent UVs at shared corners. Each
face can use either original surface/material; UVs interpolate within the
original triangles. Slabs, stairs, plants, native fluid models and cutout-side
models retain their existing geometry when opted in and issue a bake warning.
Imported full-cube water is geometrically eligible, so leave its flag off if
water should remain square. The blocky_game comparison does this explicitly.

Stored voxel IDs, model-array lengths and `VoxelBoxMover` collision AABBs do not
change. Triangle collision surfaces follow the rounded render geometry when
requested. Static model previews show the original model because rounding
depends on its neighbors. Chunk shadow-occluder shortcuts conservatively skip
rounded contacts; the regular mesh still casts shadows.

Rounded cubes contain more triangles than square cubes. Material-independent
geometry is generated on demand and cached per worker, bounded at 512 shapes.
Changing shared settings invalidates that worker's cache. The disabled path
uses the original mesher, with one additional opt-in condition per model.

## Validation entry points

The module's existing test suite includes `test_blocky_rounding`. It checks
collision/render agreement, voxel-ID preservation, zero-radius compatibility,
and a mixed rounded/square join split across mesh chunks. Run it in a build made
with `voxel_tests=yes`:

```gdscript
VoxelEngine.run_tests({"includes": ["test_blocky_rounding"]})
```

`tests/voxel/test_round_geometry_standalone.cpp` can be compiled as an independent
C++17 executable with `VOXEL_ROUNDING_STANDALONE_TEST` defined. It checks closed
manifolds, triangle winding, finite normals and the radius bounds across the
taper/profile matrix.

The companion voxelgame project's `rounding_review` directory contains the
selection array, integration tests, and deterministic captures. The captures
instantiate `blocky_game.tscn`; landscape views retain its generator, while
close-up views substitute a small fixture generator. Camera, seed, flags and
profile are recorded beside each PNG. Simulation is frozen for comparison.

## Transparent neighbors

Set `model.rounding_affects_neighbors = false` for decorations or mesh-based water that should count as air for adjacent rounded cubes. Keep `rounding_enabled = false` on those models. This independent per-model flag defaults to true and does not change the model IDs, materials, culling, or collision rules. Use the existing library array to configure it, then bake.
