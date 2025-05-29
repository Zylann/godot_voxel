extends Node

func _ready() -> void:
	if VoxelEngine.has_method("run_tests"):
		VoxelEngine.call_deferred("run_tests", {})
	else:
		push_error("Tests not available")
