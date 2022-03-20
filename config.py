
def can_build(env, platform):
    return True


def configure(env):
    from SCons.Script import BoolVariable, Variables, Help

    env_vars = Variables()

    env_vars.Add(BoolVariable("voxel_tests", 
        "Build with tests for the voxel module, which will run on startup of the engine", False))

    env_vars.Update(env)
    Help(env_vars.GenerateHelpText(env))


def get_icons_path():
    return "editor/icons"


def get_doc_classes():
    return [
        "VoxelServer",

        "Voxel",
        "VoxelLibrary",
        "VoxelColorPalette",
        "VoxelInstanceLibrary",
        "VoxelInstanceLibraryItem",
        "VoxelInstanceLibraryItemBase",
        "VoxelInstanceLibrarySceneItem",
        "VoxelInstanceGenerator",

        "VoxelBuffer",

        "VoxelNode",
        "VoxelTerrain",
        "VoxelLodTerrain",
        "VoxelViewer",
        "VoxelInstancer",

        "VoxelStream",
        "VoxelStreamFile",
        "VoxelStreamBlockFiles",
        "VoxelStreamRegionFiles",
        "VoxelStreamSQLite",
        "VoxelStreamScript",

        "VoxelGenerator",
        "VoxelGeneratorFlat",
        "VoxelGeneratorWaves",
        "VoxelGeneratorHeightmap",
        "VoxelGeneratorImage",
        "VoxelGeneratorNoise2D",
        "VoxelGeneratorNoise",
        "VoxelGeneratorGraph",
        "VoxelGeneratorScript",

        "VoxelBoxMover",
        "VoxelRaycastResult",
        "VoxelTool",
        "VoxelToolTerrain",
        "VoxelToolLodTerrain",
        "VoxelToolBuffer",
        "VoxelBlockSerializer",
        "VoxelVoxLoader",
        "VoxelDataBlockEnterInfo",

        "FastNoiseLite",
        "FastNoiseLiteGradient",

        "VoxelMesher",
        "VoxelMesherBlocky",
        "VoxelMesherTransvoxel",
        "VoxelMesherDMC",
        "VoxelMesherCubes"
    ]


def get_doc_path():
    return "doc/classes"
