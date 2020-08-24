
def can_build(env, platform):
  return True


def configure(env):
	pass

def get_icons_path():
  return "editor/icons"


def get_doc_classes():
  return [
    "VoxelBuffer",
    "VoxelMap",

    "Voxel",
    "VoxelLibrary",

    "VoxelTerrain",
    "VoxelLodTerrain",

    "VoxelStream",
    "VoxelStreamFile",
    "VoxelStreamBlockFiles",
    "VoxelStreamRegionFiles",

    "VoxelGenerator",
    "VoxelGeneratorHeightmap",
    "VoxelGeneratorImage",
    "VoxelGeneratorNoise",
    "VoxelGeneratorNoise2D",
    "VoxelGeneratorTest",
    "VoxelGeneratorGraph",

    "VoxelBoxMover",
    "VoxelTool",
    "VoxelToolTerrain",
    "VoxelRaycastResult",

    "VoxelMesher",
    "VoxelMesherBlocky",
    "VoxelMesherTransvoxel",
    "VoxelMesherDMC"
]


def get_doc_path():
  return "doc/classes"
