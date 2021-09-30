Multiplayer
=============

Multiplayer is a planned feature still in design.

It is currently not possible to implement it efficiently with the script API, so C++ changes are required. Streams are also not suited for such a task because the use case is different. They require synchronous access, and networking is asynchronous.

The plans are to implement a server-authoritative design, where clients don't stream anything by themselves, and rather listen to what the server sends them. The server will only send modified blocks, while the others can be generated locally.
It is important for the `VoxelViewer` system to be functional before this gets implemented, as it guarantees that voxel volumes can be streamed by multiple users at once.

RPCs in Godot use UDP, so sending large amounts of voxels to clients may be severely limited. Instead, it would be an option to use TCP in order to send blocks instead, as well as large edits. Small edits or deterministic edits could keep using UDP.
