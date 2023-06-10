#include "test_threaded_task_runner.h"
#include "../util/godot/classes/os.h"
#include "../util/godot/classes/time.h"
#include "../util/log.h"
#include "../util/memory.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "../util/tasks/threaded_task_runner.h"
#include "testing.h"

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

		void run(ThreadedTaskContext ctx) override {
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
	runner.set_batch_count(1);
	runner.set_name("Test");

	// Parallel tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		runner.enqueue(ZN_NEW(TestTask(parallel_counter)), false);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZN_TEST_ASSERT(parallel_counter->completed_count == 16);
	ZN_TEST_ASSERT(parallel_counter->max_count <= test_thread_count);
	ZN_TEST_ASSERT(parallel_counter->current_count == 0);

	// Serial tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		runner.enqueue(ZN_NEW(TestTask(serial_counter)), true);
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
			runner.enqueue(ZN_NEW(TestTask(parallel_counter)), false);
		} else {
			runner.enqueue(ZN_NEW(TestTask(serial_counter)), true);
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

		void run(ThreadedTaskContext ctx) override {
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

		void run(ThreadedTaskContext ctx) override {
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
	runner.set_batch_count(1);
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
					runner.enqueue(ZN_NEW(NamedTestTask1(100 + i % 11)), false);
				} else {
					runner.enqueue(ZN_NEW(NamedTestTask2(60 + i % 7)), true);
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

} // namespace zylann::tests
