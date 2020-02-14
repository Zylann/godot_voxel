# Performance Tips

* Disable VSync and set threading to `Single Safe` in the project settings. Terrain building and editing performs significnatly faster with these settings.

* Excessive noise height will kill your performance, use with care. A height range of 300 can run at 300-400fps on a GTX1060, but a range of 6-10,000 might run at 30-40fps. Hopefully this will be addressed when occlusion culling and other features are available in Godot 4.

* Lower shadow distance on your directional light. I might use 200-400 on a GTX1060, but maybe 20 or disable shadows altogether on an integrated card.

* Use only one directional light. A second cost me a 12% performance hit.

* VoxelLodTerrain has a `collision_lod_count` that limits the creation of collision shapes for distant LODs. Try setting it to 2-3 or higher.

* Optimize and simplify your shaders. In the fps_demo, there are two grass-rock shaders. The second version randomizes the tiling of texture maps so there is no obvious repeating pattern. It only requires two extra texture lookups, but it pretty much halves my frame rate from around 300 to 150 or less.

* Use Linux. On my system, my demo runs with a 30-100% higher frame rate under linux.


---
* [Next Page](08_api-overview.md)
* [Doc Index](01_get-started.md)
