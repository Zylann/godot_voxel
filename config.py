

def can_build(env, platform):
  return True


def configure(env):
	pass


def get_doc_classes():
  return [
    "VoxelBuffer",
    "VoxelMap",

    "Voxel",
    "VoxelLibrary",

    "VoxelTerrain",
    "VoxelLodTerrain",

    "VoxelStream",
    "VoxelStreamTest",
    "VoxelStreamImage",
    "VoxelStreamNoise",
    "VoxelStreamFile",
    "VoxelStreamBlockFiles",
    "VoxelStreamRegionFiles",

    "VoxelBoxMover",
    "VoxelIsoSurfaceTool",

    "VoxelMesher",
    "VoxelMesherBlocky",
    "VoxelMesherTransvoxel",
    "VoxelMesherDMC"
  ]


def get_doc_path():
  return "doc/classes"
