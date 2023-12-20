#include "test_threaded_task_runner.h"
#include "../../util/godot/classes/os.h"
#include "../../util/godot/classes/time.h"
#include "../../util/io/log.h"
#include "../../util/math/vector3i.h"
#include "../../util/memory.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/threaded_task_runner.h"
#include "../testing.h"

#include <fstream>
#include <unordered_map>

namespace zylann::tests {

void test_threaded_task_runner_misc() {
	static const uint32_t task_duration_usec = 100'000;

	struct TaskCounter {
		std::atomic_uint32_t max_count = { 0 };
		std::atomic_uint32_t current_count = { 0 };
		std::atomic_uint32_t completed_count = { 0 };

		void reset() {
			max_count = 0;
			current_count = 0;
			completed_count = 0;
		}
	};

	class TestTask : public IThreadedTask {
	public:
		std::shared_ptr<TaskCounter> counter;
		bool completed = false;

		TestTask(std::shared_ptr<TaskCounter> p_counter) : counter(p_counter) {}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();
			ZN_ASSERT(counter != nullptr);

			++counter->current_count;

			// Update maximum count
			// https://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
			unsigned int current_count = counter->current_count;
			unsigned int prev_max = counter->max_count;
			while (prev_max < current_count && !counter->max_count.compare_exchange_weak(prev_max, current_count)) {
				current_count = counter->current_count;
			}

			Thread::sleep_usec(task_duration_usec);

			--counter->current_count;
			++counter->completed_count;
			completed = true;
		}

		void apply_result() override {
			ZN_TEST_ASSERT(completed);
		}
	};

	struct L {
		static void dequeue_tasks(ThreadedTaskRunner &runner) {
			runner.dequeue_completed_tasks([](IThreadedTask *task) {
				ZN_ASSERT(task != nullptr);
				task->apply_result();
				ZN_DELETE(task);
			});
		}
	};

	const unsigned int test_thread_count = 4;
	const unsigned int hw_concurrency = Thread::get_hardware_concurrency();
	if (hw_concurrency < test_thread_count) {
		ZN_PRINT_WARNING(format(
				"Hardware concurrency is {}, smaller than test requirement {}", test_thread_count, hw_concurrency));
	}

	std::shared_ptr<TaskCounter> parallel_counter = make_unique_instance<TaskCounter>();
	std::shared_ptr<TaskCounter> serial_counter = make_unique_instance<TaskCounter>();

	ThreadedTaskRunner runner;
	runner.set_thread_count(test_thread_count);
	runner.set_name("Test");

	// Parallel tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		TestTask *task = ZN_NEW(TestTask(parallel_counter));
		runner.enqueue(task, false);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZN_TEST_ASSERT(parallel_counter->completed_count == 16);
	ZN_TEST_ASSERT(parallel_counter->max_count <= test_thread_count);
	ZN_TEST_ASSERT(parallel_counter->current_count == 0);

	// Serial tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		TestTask *task = ZN_NEW(TestTask(serial_counter));
		runner.enqueue(task, true);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZN_TEST_ASSERT(serial_counter->completed_count == 16);
	ZN_TEST_ASSERT(serial_counter->max_count == 1);
	ZN_TEST_ASSERT(serial_counter->current_count == 0);

	// Interleaved

	parallel_counter->reset();
	serial_counter->reset();

	for (unsigned int i = 0; i < 32; ++i) {
		if ((i & 1) == 0) {
			TestTask *task = ZN_NEW(TestTask(parallel_counter));
			runner.enqueue(task, false);
		} else {
			TestTask *task = ZN_NEW(TestTask(serial_counter));
			runner.enqueue(task, true);
		}
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZN_TEST_ASSERT(parallel_counter->completed_count == 16);
	ZN_TEST_ASSERT(parallel_counter->max_count <= test_thread_count);
	ZN_TEST_ASSERT(parallel_counter->current_count == 0);
	ZN_TEST_ASSERT(serial_counter->completed_count == 16);
	ZN_TEST_ASSERT(serial_counter->max_count == 1);
	ZN_TEST_ASSERT(serial_counter->current_count == 0);
}

void test_threaded_task_runner_debug_names() {
	class NamedTestTask1 : public IThreadedTask {
	public:
		unsigned int sleep_amount_usec;

		NamedTestTask1(int p_sleep_amount_usec) : sleep_amount_usec(p_sleep_amount_usec) {}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();
			Thread::sleep_usec(sleep_amount_usec);
		}

		const char *get_debug_name() const override {
			return "NamedTestTask1";
		}
	};

	class NamedTestTask2 : public IThreadedTask {
	public:
		unsigned int sleep_amount_usec;

		NamedTestTask2(int p_sleep_amount_usec) : sleep_amount_usec(p_sleep_amount_usec) {}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();
			Thread::sleep_usec(sleep_amount_usec);
		}

		const char *get_debug_name() const override {
			return "NamedTestTask2";
		}
	};

	struct L {
		static void dequeue_tasks(ThreadedTaskRunner &runner, unsigned int &r_in_flight_count) {
			runner.dequeue_completed_tasks([&r_in_flight_count](IThreadedTask *task) {
				ZN_ASSERT(task != nullptr);
				task->apply_result();
				ZN_DELETE(task);
				ZN_ASSERT(r_in_flight_count > 0);
				--r_in_flight_count;
			});
		}
	};

	const unsigned int test_thread_count = 4;
	const unsigned int hw_concurrency = Thread::get_hardware_concurrency();
	if (hw_concurrency < test_thread_count) {
		ZN_PRINT_WARNING(format(
				"Hardware concurrency is {}, smaller than test requirement {}", test_thread_count, hw_concurrency));
	}

	ThreadedTaskRunner runner;
	runner.set_thread_count(test_thread_count);
	runner.set_name("Test");

	const uint64_t time_before = Time::get_singleton()->get_ticks_msec();

	unsigned int in_flight_count = 0;

	Dictionary name_counts;

	while (Time::get_singleton()->get_ticks_msec() - time_before < 5000) {
		ZN_PROFILE_SCOPE();

		// Saturate task queue so a bunch should be running while we query their names
		while (in_flight_count < 5000) {
			for (unsigned int i = 0; i < 1000; ++i) {
				if ((i % 3) != 0) {
					NamedTestTask1 *task = ZN_NEW(NamedTestTask1(100 + i % 11));
					runner.enqueue(task, false);
				} else {
					NamedTestTask2 *task = ZN_NEW(NamedTestTask2(60 + i % 7));
					runner.enqueue(task, true);
				}
				++in_flight_count;
			}
		}

		// Get debug task names
		FixedArray<const char *, ThreadedTaskRunner::MAX_THREADS> active_task_names;
		fill(active_task_names, (const char *)nullptr);
		for (unsigned int i = 0; i < test_thread_count; ++i) {
			active_task_names[i] = runner.get_thread_debug_task_name(i);
		}

		// Put task names into an array
		Array task_names;
		task_names.resize(test_thread_count);
		for (unsigned int i = 0; i < active_task_names.size(); ++i) {
			const char *name = active_task_names[i];
			if (name != nullptr) {
				task_names[i] = name;
			}
		}

		// Count names
		for (int i = 0; i < task_names.size(); ++i) {
			Variant name = task_names[i];
			Variant count = name_counts[name];
			if (count.get_type() != Variant::INT) {
				count = 1;
			} else {
				count = int64_t(count) + 1;
			}
			name_counts[name] = count;
		}

		L::dequeue_tasks(runner, in_flight_count);

		OS::get_singleton()->delay_usec(10'000);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner, in_flight_count);

	// Print how many times each name came up.
	// Doing this to check if the test runs as expected and to prevent compiler optimization on getting the names
	String s;
	Array keys = name_counts.keys();
	for (int i = 0; i < keys.size(); ++i) {
		String name = keys[i];
		Variant count = name_counts[name];
		s += String("{0}: {1}; ").format(varray(name, count));
	}
	print_line(s);
}

void test_task_priority_values() {
	ZN_TEST_ASSERT(TaskPriority(0, 0, 0, 0) < TaskPriority(1, 0, 0, 0));
	ZN_TEST_ASSERT(TaskPriority(0, 0, 0, 0) < TaskPriority(0, 0, 0, 1));
	ZN_TEST_ASSERT(TaskPriority(10, 0, 0, 0) < TaskPriority(0, 10, 0, 0));
	ZN_TEST_ASSERT(TaskPriority(10, 10, 0, 0) < TaskPriority(10, 10, 10, 0));
}

// Simulates doing work in every chunk of a grid, where each task will want to access neighbors of each block. If any
// neighbor fails to get locked, the task is postponed.
void test_threaded_task_postponing() {
	// There isn't really a test check in this function, for now we run it to detect if it crashes and that all tasks
	// eventually run once.

	//#define VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS

	struct Block {
		std::atomic_bool is_locked;
	};

	struct Map {
		// Doesn't have to be a map but I chose it anyways since that's how the actual voxel map is stored
		std::unordered_map<Vector3i, Block> blocks;
	};

	struct Event {
		enum Type { BLOCK_BEGIN, BLOCK_END };
		Type type;
		Vector3i bpos;
		uint64_t time_us;
	};

	// To log what actually happened and visualize it
	struct EventList {
		std::vector<Event> events;
		Mutex mutex;

		inline void push(Event event) {
			event.time_us = Time::get_singleton()->get_ticks_usec();
			MutexLock lock(mutex);
			events.push_back(event);
		}
	};

	class Task1 : public IThreadedTask {
	public:
		unsigned int sleep_amount_usec;
		Vector3i bpos0;
		Map &map;
		EventList &events;

		Task1(int p_sleep_amount_usec, Map &p_map, Vector3i p_bpos, EventList &p_events) :
				sleep_amount_usec(p_sleep_amount_usec), map(p_map), bpos0(p_bpos), events(p_events) {}

		bool try_lock_area(std::vector<Block *> &locked_blocks) {
			Vector3i delta;
			for (delta.z = -1; delta.z < 2; ++delta.z) {
				for (delta.x = -1; delta.x < 2; ++delta.x) {
					for (delta.y = -1; delta.y < 2; ++delta.y) {
						const Vector3i bpos = bpos0 + delta;
						auto it = map.blocks.find(bpos);
						if (it == map.blocks.end()) {
							continue;
						}
						Block &block = it->second;
						bool expected = false;
						if (block.is_locked.compare_exchange_strong(expected, true) == false) {
							// Could not lock, will have to cancel
							for (Block *b : locked_blocks) {
								b->is_locked = false;
							}
							locked_blocks.clear();
							return false;
						} else {
							// Successful lock
							locked_blocks.push_back(&block);
						}
					}
				}
			}
			return true;
		}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();

			static thread_local std::vector<Block *> locked_blocks;

			if (!try_lock_area(locked_blocks)) {
				ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
				return;
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_BEGIN, bpos0 });
#endif

			{
				ZN_PROFILE_SCOPE_NAMED("Work");
				Thread::sleep_usec(sleep_amount_usec);
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_END, bpos0 });
#endif

			for (Block *block : locked_blocks) {
				block->is_locked = false;
			}
			locked_blocks.clear();
		}

		const char *get_debug_name() const override {
			return "Task1";
		}
	};

	struct L {
		static void dequeue_tasks(ThreadedTaskRunner &runner, unsigned int &r_in_flight_count) {
			runner.dequeue_completed_tasks([&r_in_flight_count](IThreadedTask *task) {
				ZN_ASSERT(task != nullptr);
				task->apply_result();
				ZN_DELETE(task);
				ZN_ASSERT(r_in_flight_count > 0);
				--r_in_flight_count;
			});
		}
	};

	const unsigned int test_thread_count = 4;
	const unsigned int hw_concurrency = Thread::get_hardware_concurrency();
	if (hw_concurrency < test_thread_count) {
		ZN_PRINT_WARNING(format(
				"Hardware concurrency is {}, smaller than test requirement {}", test_thread_count, hw_concurrency));
	}

	ThreadedTaskRunner runner;
	runner.set_thread_count(test_thread_count);
	runner.set_name("Test");

	unsigned int in_flight_count = 0;

	// Generate map
	Map map;
	const int map_size = 16;
	Vector3i bpos;
	for (bpos.z = 0; bpos.z < map_size; ++bpos.z) {
		for (bpos.x = 0; bpos.x < map_size; ++bpos.x) {
			for (bpos.y = 0; bpos.y < map_size; ++bpos.y) {
				Block &block = map.blocks[bpos];
				block.is_locked = false;
			}
		}
	}

	EventList events;
	RandomPCG rng;

	// Schedule tasks that will want to access overlapping blocks
	for (bpos.z = 0; bpos.z < map_size; ++bpos.z) {
		for (bpos.x = 0; bpos.x < map_size; ++bpos.x) {
			for (bpos.y = 0; bpos.y < map_size; ++bpos.y) {
				Task1 *task = ZN_NEW(Task1(1000 + rng.rand(2000), map, bpos, events));
				runner.enqueue(task, false);
				++in_flight_count;
			}
		}
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner, in_flight_count);

	ZN_TEST_ASSERT(in_flight_count == 0);

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
	// Dump events
	std::ofstream ofs("ddd_block_tasks_test.json", std::ios::binary);
	if (ofs.good()) {
		ofs << "{\n";
		ofs << "\t\"events\": [\n";
		unsigned int i = 0;
		for (const Event &e : events.events) {
			if (i > 0) {
				ofs << ",\n";
			}
			ofs << "\t\t[" << e.type << ", " << e.time_us << ", " << e.bpos.x << ", " << e.bpos.y << ", " << e.bpos.z
				<< "]";
			++i;
		}
		ofs << "\n\t]\n}\n";
	}
#endif
}

} // namespace zylann::tests
