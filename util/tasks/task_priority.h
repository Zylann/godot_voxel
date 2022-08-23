#ifndef ZN_TASK_PRIORITY_H
#define ZN_TASK_PRIORITY_H

#include <cstdint>

namespace zylann {

// Represents the priorirty of a task, which can be compared quickly to another.
struct TaskPriority {
	static const uint8_t BAND_MAX = 255;

	union {
		struct {
			uint8_t band0; // Higher means higher priority (to state the obvious)
			uint8_t band1; // Takes precedence over band0
			uint8_t band2; // Takes precedence over band1
			uint8_t band3; // Takes precedence over band2
		};
		uint32_t whole;
	};

	TaskPriority() : whole(0) {}

	TaskPriority(uint8_t p_band0, uint8_t p_band1, uint8_t p_band2, uint8_t p_band3) :
			band0(p_band0), band1(p_band1), band2(p_band2), band3(p_band3) {}

	// Returns `true` if the left-hand priority is lower than the right-hand one.
	// Means the right-hand task should run first.
	inline bool operator<(const TaskPriority &other) const {
		return whole < other.whole;
	}

	// Returns `true` if the left-hand priority is higher than the right-hand one.
	// Means the left-hand task should run first.
	inline bool operator>(const TaskPriority &other) const {
		return whole > other.whole;
	}

	inline bool operator==(const TaskPriority &other) const {
		return whole == other.whole;
	}

	static inline TaskPriority min() {
		TaskPriority p;
		p.whole = 0;
		return p;
	}

	static inline TaskPriority max() {
		TaskPriority p;
		p.whole = 0xffffffff;
		return p;
	}
};

} // namespace zylann

#endif // ZN_TASK_PRIORITY_H
