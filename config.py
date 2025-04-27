# This file is for compiling as a module. It may not be used when compiling as an extension.

import common


def can_build(env, platform):
    return True


def configure(env):
    common.register_scons_options(env, False)


def get_icons_path():
    # return "editor/icons"
    # GDExtension icons have to be shipped as external files instead of being compiled in the library.
    # So we put them here to avoid having to copy them.
    return "project/addons/zylann.voxel/editor/icons"


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
        "VoxelBlockyModelFluid",
        "VoxelBlockyModelMesh",
        "VoxelBlockyFluid",
        "VoxelBlockyType",
        "VoxelBlockyTypeLibrary",
        "VoxelBoxMover",
        "VoxelBuffer",
        "VoxelColorPalette",
        "VoxelDataBlockEnterInfo",
        "VoxelEngine",
        "VoxelFormat",
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
