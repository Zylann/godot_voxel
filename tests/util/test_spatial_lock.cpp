#include "test_spatial_lock.h"
#include "../../util/godot/classes/time.h"
#include "../../util/math/conv.h"
#include "../../util/memory.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/threaded_task_runner.h"
#include "../../util/thread/spatial_lock_3d.h"
#include "../testing.h"
#include <fstream>

namespace zylann::tests {

void test_spatial_lock_misc() {
	SpatialLock3D spatial_lock;

	// Lock a box around the origin
	const BoxBounds3i box1 = BoxBounds3i::from_min_max_included(Vector3i(-1, -1, -1), Vector3i(1, 1, 1));
	spatial_lock.lock_read(box1);

	// Unlock it
	spatial_lock.unlock_read(box1);

	// Lock it again
	spatial_lock.lock_read(box1);

	// Spawn a thread to lock more boxes
	Thread thread;
	thread.start(
			[](void *userdata) {
				SpatialLock3D &spatial_lock = *static_cast<SpatialLock3D *>(userdata);

				// Try to lock a box overlapping the one locked by the main thread. It's for read too, it should succeed
				const BoxBounds3i box2 = BoxBounds3i::from_min_max_included(Vector3i(0, 0, 0), Vector3i(3, 4, 5));
				ZN_TEST_ASSERT(spatial_lock.try_lock_read(box2) == true);

				spatial_lock.unlock_read(box2);

				// Try to lock a box overlapping the one locked by the main thread. It's for write, should not succeed
				const BoxBounds3i box3 = BoxBounds3i::from_position(Vector3i(0, 0, 0));
				ZN_TEST_ASSERT(spatial_lock.try_lock_write(box3) == false);

				// Try to lock a box not overlapping the one locked by the main thread. It should succeed.
				const BoxBounds3i box4 = BoxBounds3i::from_position(Vector3i(5, 0, 0));
				ZN_TEST_ASSERT(spatial_lock.try_lock_write(box4) == true);

				spatial_lock.unlock_write(box4);
			},
			&spatial_lock);

	thread.wait_to_finish();

	// Unlock the box around origin
	spatial_lock.unlock_read(box1);

	// Lock a box for write
	const BoxBounds3i box3 = BoxBounds3i::from_position(Vector3i(0, 0, 0));
	ZN_TEST_ASSERT(spatial_lock.try_lock_write(box3) == true);
	spatial_lock.unlock_write(box3);

	ZN_TEST_ASSERT(spatial_lock.get_locked_boxes_count() == 0);
}

void test_spatial_lock_spam() {
	// Spawns many threads that will each lock random boxes in a limited area of a grid of numbers, very frequently.
	// They either read data, in which case they check it doesn't change while they do so,
	// or they write data, in which case they reset numbers and increment them predictably, making sure the result is as
	// expected.

	static const uint64_t THREAD_DURATION_MILLISECONDS = 3000;
	static const int AREA_SIZE = 10;
	static const uint64_t LOCK_DURATION_MICROSECONDS = 20;

	// Simple 3D grid
	struct Map {
	public:
		Map(Vector3i p_size) {
			_size = p_size;
			_cells.resize(Vector3iUtil::get_volume(_size), 0);
		}

		inline Vector3i get_size() const {
			return _size;
		}

		inline bool is_position_valid(Vector3i pos) const {
			return pos.x >= 0 && pos.y >= 0 && pos.z >= 0 && pos.x < _size.x && pos.y < _size.y && pos.z < _size.z;
		}

		inline int &at(Vector3i pos) {
			ZN_ASSERT(is_position_valid(pos));
			return _cells[Vector3iUtil::get_zxy_index(pos, Vector3i(AREA_SIZE, AREA_SIZE, AREA_SIZE))];
		}

		inline int at(Vector3i pos) const {
			ZN_ASSERT(is_position_valid(pos));
			return _cells[Vector3iUtil::get_zxy_index(pos, Vector3i(AREA_SIZE, AREA_SIZE, AREA_SIZE))];
		}

		bool cells_in_box_equal(const Box3i box, int expected_value) {
			return box.all_cells_match([this, expected_value](Vector3i pos) { //
				return at(pos) == expected_value;
			});
		}

	private:
		std::vector<int> _cells;
		Vector3i _size;
	};

	// Data passed to each thread
	struct Context {
		SpatialLock3D *spatial_lock;
		Map *map;
		unsigned int thread_index;
	};

	struct L {
		static void modify_cells(Map &map, Box3i box, RandomPCG &rng, uint64_t microseconds) {
			// Set all cells in the box to a starting value
			const int base = rng.rand(1000);
			box.for_each_cell([&map, base](Vector3i pos) { map.at(pos) = base; });

			const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

			for (int i = 1; /* keep looping at least once */; ++i) {
				// Increment cells in the box
				box.for_each_cell([&map](Vector3i pos) { ++map.at(pos); });
				// Check they have the expected value
				ZN_TEST_ASSERT(map.cells_in_box_equal(box, base + i));

				if (Time::get_singleton()->get_ticks_usec() - time_before >= microseconds) {
					break;
				}
			}
		}

		static void read_cells(const Map &map, Box3i box, uint64_t microseconds, std::vector<int> &reusable_vector) {
			const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

			std::vector<int> &expected_values = reusable_vector;
			expected_values.clear();
			expected_values.reserve(Vector3iUtil::get_volume(box.size));
			box.for_each_cell([&map, &expected_values](Vector3i pos) { //
				expected_values.push_back(map.at(pos));
			});

			for (int i = 0; /* keep looping at least once */; ++i) {
				int j = 0;
				box.for_each_cell([&map, &expected_values, &j](Vector3i pos) { //
					// Cells must not change while we read them.
					ZN_TEST_ASSERT(expected_values[j] == map.at(pos));
					// Note, iteration order is the same as when we cached expected values
					++j;
				});

				if (Time::get_singleton()->get_ticks_usec() - time_before >= microseconds) {
					break;
				}
			}
		}

		static Box3i make_random_box(const Vector3i area_size, RandomPCG &rng) {
			ZN_ASSERT(area_size.x > 0 && area_size.y > 0 && area_size.z > 0);
			return Box3i(Vector3i(rng.rand(area_size.x), rng.rand(area_size.y), rng.rand(area_size.z)),
					Vector3i(1 + rng.rand(4), 1 + rng.rand(4), 1 + rng.rand(4)))
					.clipped(Box3i(Vector3i(), area_size));
		}

		static void thread_func(void *userdata) {
			Context &ctx = *static_cast<Context *>(userdata);
			SpatialLock3D &spatial_lock = *ctx.spatial_lock;
			Map &map = *ctx.map;

			const uint64_t time_before = Time::get_singleton()->get_ticks_msec();

			RandomPCG rng;
			rng.seed(ctx.thread_index + 42);

			std::vector<int> reusable_vector;

			// Keep running for the duration of the test
			while (Time::get_singleton()->get_ticks_msec() - time_before < THREAD_DURATION_MILLISECONDS) {
				for (int i = 0; i < 10; ++i) {
					const Box3i box = make_random_box(map.get_size(), rng);
					const bool read = rng.rand(100) < 90;

					if (read) {
						ZN_PROFILE_SCOPE_NAMED("Read");
						SpatialLock3D::Read srlock(spatial_lock, box);
						{
							ZN_PROFILE_SCOPE_NAMED("Work");
							read_cells(map, box, LOCK_DURATION_MICROSECONDS, reusable_vector);
						}

					} else {
						ZN_PROFILE_SCOPE_NAMED("Write");
						SpatialLock3D::Write swlock(spatial_lock, box);
						{
							ZN_PROFILE_SCOPE_NAMED("Work");
							modify_cells(map, box, rng, LOCK_DURATION_MICROSECONDS);
						}
					}
				}
			}
		}
	};

	Map map(Vector3i(AREA_SIZE, AREA_SIZE, AREA_SIZE));
	SpatialLock3D spatial_lock;
	FixedArray<Thread, 7> threads; // Excluding main thread
	FixedArray<Context, 8> contexts;
	const unsigned int main_thread_index = contexts.size() - 1;

	for (unsigned int thread_index = 0; thread_index < contexts.size(); ++thread_index) {
		contexts[thread_index] = Context{ &spatial_lock, &map, thread_index };
	}

	for (unsigned int thread_index = 0; thread_index < threads.size(); ++thread_index) {
		threads[thread_index].start(L::thread_func, &contexts[thread_index]);
	}

	L::thread_func(&contexts[main_thread_index]);

	for (unsigned int thread_index = 0; thread_index < threads.size(); ++thread_index) {
		threads[thread_index].wait_to_finish();
	}

	ZN_TEST_ASSERT(spatial_lock.get_locked_boxes_count() == 0);
}

// #define VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS

void test_spatial_lock_dependent_map_chunks() {
	// Simulates a bunch of tasks that could be baking light in columns of chunks.
	// Each task may write into its neighbors.
	// This also uses ThreadedTaskRunner and postponing.
	// Not many tests in here, but can be interesting to analyze the dumps.

	static const int MAP_SIZE = 16;

	struct Map {
		SpatialLock3D spatial_lock;
	};

	struct Event {
		enum Type { BLOCK_WRITE_BEGIN, BLOCK_WRITE_END, BLOCK_READ_BEGIN, BLOCK_READ_END };
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

	struct L {
		static void dequeue_tasks(ThreadedTaskRunner &runner, unsigned int &r_in_flight_count) {
			runner.dequeue_completed_tasks([&r_in_flight_count](IThreadedTask *task) {
				ZN_ASSERT(task != nullptr);
				// zylann::println(format("Dequeue {}", task));
				task->apply_result();
				ZN_DELETE(task);
				ZN_ASSERT(r_in_flight_count > 0);
				--r_in_flight_count;
			});
		}

		static TaskPriority get_priority(Vector3i bpos) {
			TaskPriority p;
			const float distance_to_center = math::distance(to_vec3f(bpos), Vector3f(MAP_SIZE / 2));
			p.band0 = 255.f * math::clamp(1.f - distance_to_center / float(2 * MAP_SIZE), 0.f, 1.f);
			return p;
		}
	};

	// Writing task
	class Task1 : public IThreadedTask {
	public:
		unsigned int sleep_amount_usec;
		Vector2i column_pos;
		Map &map;
		EventList &events;

		Task1(int p_sleep_amount_usec, Map &p_map, Vector2i p_column_pos, EventList &p_events) :
				sleep_amount_usec(p_sleep_amount_usec), map(p_map), column_pos(p_column_pos), events(p_events) {}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();

			const BoxBounds3i box( //
					Vector3i(column_pos.x - 1, 0, column_pos.y - 1), //
					Vector3i(column_pos.x + 1, 24, column_pos.y + 1) + Vector3i(1, 1, 1));

			if (!map.spatial_lock.try_lock_write(box)) {
				ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
				return;
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_WRITE_BEGIN, Vector3i(column_pos.x, 0, column_pos.y) });
#endif

			{
				ZN_PROFILE_SCOPE_NAMED("Work");
				Thread::sleep_usec(sleep_amount_usec);
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_WRITE_END, Vector3i(column_pos.x, 0, column_pos.y) });
#endif
			map.spatial_lock.unlock_write(box);
		}

		TaskPriority get_priority() override {
			return L::get_priority(Vector3i(column_pos.x, MAP_SIZE / 2, column_pos.y));
		}

		const char *get_debug_name() const override {
			return "Task1";
		}
	};

	// Reading task
	class Task2 : public IThreadedTask {
	public:
		unsigned int sleep_amount_usec;
		Vector3i bpos0;
		Map &map;
		EventList &events;

		Task2(int p_sleep_amount_usec, Map &p_map, Vector3i p_bpos, EventList &p_events) :
				sleep_amount_usec(p_sleep_amount_usec), map(p_map), bpos0(p_bpos), events(p_events) {}

		void run(ThreadedTaskContext &ctx) override {
			ZN_PROFILE_SCOPE();

			const BoxBounds3i box(bpos0 - Vector3i(1, 1, 1), bpos0 + Vector3i(2, 2, 2));

			if (!map.spatial_lock.try_lock_read(box)) {
				ctx.status = ThreadedTaskContext::STATUS_POSTPONED;
				return;
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_READ_BEGIN, bpos0 });
#endif

			{
				ZN_PROFILE_SCOPE_NAMED("Work");
				Thread::sleep_usec(sleep_amount_usec);
			}

#ifdef VOXEL_TEST_TASK_POSTPONING_DUMP_EVENTS
			events.push(Event{ Event::BLOCK_READ_END, bpos0 });
#endif
			map.spatial_lock.unlock_read(box);
		}

		TaskPriority get_priority() override {
			return L::get_priority(bpos0);
		}

		const char *get_debug_name() const override {
			return "Task2";
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

	Map map;

	EventList events;
	RandomPCG rng;

	// Schedule tasks that will want to access overlapping blocks
	Vector2i column_pos;
	for (column_pos.y = 0; column_pos.y < MAP_SIZE; ++column_pos.y) {
		for (column_pos.x = 0; column_pos.x < MAP_SIZE; ++column_pos.x) {
			Task1 *task = ZN_NEW(Task1(1000 + rng.rand(2000), map, column_pos, events));
			runner.enqueue(task, false);
			++in_flight_count;

			// Add some reading requests
			for (int i = 0; i < 2; ++i) {
				Task2 *task = ZN_NEW(Task2(1000 + rng.rand(2000), map,
						Vector3i(rng.rand(MAP_SIZE), rng.rand(MAP_SIZE), rng.rand(MAP_SIZE)), events));
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
