
# This file is for compiling as a module. It may not be used when compiling as an extension.

def can_build(env, platform):
    return True


def configure(env):
    from SCons.Script import BoolVariable, Variables, Help

    env_vars = Variables()

    env_vars.Add(BoolVariable("voxel_tests",
        "Build with tests for the voxel module, which will run on startup of the engine", False))

    env_vars.Add(BoolVariable("voxel_fast_noise_2", "Build FastNoise2 support (x86-only)", True))

    env_vars.Add(BoolVariable("tracy", "Build with enabled Tracy Profiler integration", False))

    env_vars.Update(env)
    Help(env_vars.GenerateHelpText(env))


def get_icons_path():
    return "editor/icons"


def get_doc_classes():
    return [
        "FastNoise2",
        "VoxelAStarGrid3D",
        "VoxelBlockSerializer",
        "VoxelBlockyAttribute",
        "VoxelBlockyAttributeAxis",
        "VoxelBlockyAttributeCustom",
        "VoxelBlockyAttributeDirection",
        "VoxelBlockyAttributeRotation",
        "VoxelBlockyLibrary",
        "VoxelBlockyLibraryBase",
        "VoxelBlockyModel",
        "VoxelBlockyModelCube",
        "VoxelBlockyModelEmpty",
        "VoxelBlockyModelMesh",
        "VoxelBlockyType",
        "VoxelBlockyTypeLibrary",
        "VoxelBoxMover",
        "VoxelBuffer",
        "VoxelColorPalette",
        "VoxelDataBlockEnterInfo",
        "VoxelEngine",
        "VoxelGenerator",
        "VoxelGeneratorFlat",
        "VoxelGeneratorGraph",
        "VoxelGeneratorHeightmap",
        "VoxelGeneratorImage",
        "VoxelGeneratorMultipassCB",
        "VoxelGeneratorNoise",
        "VoxelGeneratorNoise2D",
        "VoxelGeneratorScript",
        "VoxelGeneratorWaves",
        "VoxelGraphFunction",
        "VoxelInstanceComponent",
        "VoxelInstanceGenerator",
        "VoxelInstanceLibrary",
        "VoxelInstanceLibraryItem",
        "VoxelInstanceLibraryMultiMeshItem",
        "VoxelInstanceLibrarySceneItem",
        "VoxelInstancer",
        "VoxelInstancerRigidBody",
        "VoxelLodTerrain",
        "VoxelMesher",
        "VoxelMesherBlocky",
        "VoxelMesherCubes",
        "VoxelMesherTransvoxel",
        "VoxelMeshSDF",
        "VoxelModifier",
        "VoxelModifierMesh",
        "VoxelModifierSphere",
        "VoxelNode",
        "VoxelRaycastResult",
        "VoxelSaveCompletionTracker",
        "VoxelStream",
        "VoxelStreamMemory",
        "VoxelStreamRegionFiles",
        "VoxelStreamScript",
        "VoxelStreamSQLite",
        "VoxelTerrain",
        "VoxelTerrainMultiplayerSynchronizer",
        "VoxelTool",
        "VoxelToolBuffer",
        "VoxelToolLodTerrain",
        "VoxelToolMultipassGenerator",
        "VoxelToolTerrain",
        "VoxelViewer",
        "VoxelVoxLoader",
        "ZN_FastNoiseLite",
        "ZN_FastNoiseLiteGradient",
        "ZN_SpotNoise",
        "ZN_ThreadedTask",
    ]


def get_doc_path():
    return "doc/classes"
