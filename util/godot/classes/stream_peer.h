#ifndef ZN_GODOT_STREAM_PEER_H
#define ZN_GODOT_STREAM_PEER_H

#if defined(ZN_GODOT)
#include <core/io/stream_peer.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/stream_peer.hpp>
using namespace godot;
#endif

#include "../funcs.h"

namespace zylann {

inline void stream_peer_put_data(StreamPeer &peer, Span<const uint8_t> src) {
#if defined(ZN_GODOT)
	peer.put_data(src.data(), src.size());
#elif defined(ZN_GODOT_EXTENSION)
	PackedByteArray bytes;
	copy_to(bytes, src);
	peer.put_data(bytes);
#endif
}

inline Error stream_peer_get_data(StreamPeer &peer, Span<uint8_t> dst) {
#if defined(ZN_GODOT)
	return peer.get_data(dst.data(), dst.size());
#elif defined(ZN_GODOT_EXTENSION)
	PackedByteArray bytes = peer.get_data(dst.size());
	copy_to(dst, bytes);
	if (dst.size() != bytes.size()) {
		// That's what Godot returns in core
		return ERR_INVALID_PARAMETER;
	}
	return OK;
#endif
}

} // namespace zylann

#endif // ZN_GODOT_STREAM_PEER_H
