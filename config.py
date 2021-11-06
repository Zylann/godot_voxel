
def can_build(env, platform):
  return True


def configure(env):
	pass

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
