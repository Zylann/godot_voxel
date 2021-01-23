
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

    "VoxelBuffer",

    "VoxelNode",
    "VoxelTerrain",
    "VoxelLodTerrain",
    "VoxelViewer",

    "VoxelStream",
    "VoxelStreamFile",
    "VoxelStreamBlockFiles",
    "VoxelStreamRegionFiles",
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
