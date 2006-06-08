#include "lib/self_test.h"

#include "lib/lib.h"
#include "lib/posix.h"
#include "lib/lockfree.h"
#include "lib/timer.h"

// make sure the data structures work at all; doesn't test thread-safety.
class TestLockfreeBasic : public CxxTest::TestSuite 
{
public:
	void test_basic_single_threaded()
	{
		void* user_data;

		const uint ENTRIES = 50;
		// should be more than max # retired nodes to test release..() code
		uintptr_t key = 0x1000;
		uint sig = 10;

		LFList list;
		TS_ASSERT_OK(lfl_init(&list));

		LFHash hash;
		TS_ASSERT_OK(lfh_init(&hash, 8));

		// add some entries; store "signatures" (ascending int values)
		for(uint i = 0; i < ENTRIES; i++)
		{
			int was_inserted;

			user_data = lfl_insert(&list, (void *)(key+i), sizeof(int), &was_inserted);
			TS_ASSERT(user_data != 0 && was_inserted);
			*(uint*)user_data = sig+i;

			user_data = lfh_insert(&hash, key+i, sizeof(int), &was_inserted);
			TS_ASSERT(user_data != 0 && was_inserted);
			*(uint*)user_data = sig+i;
		}

		// make sure all "signatures" are present in list
		for(uint i = 0; i < ENTRIES; i++)
		{
			user_data = lfl_find(&list, (void *)(key+i));
			TS_ASSERT(user_data != 0);
			TS_ASSERT_EQUALS(*(uint*)user_data, sig+i);

			user_data = lfh_find(&hash, key+i);
			TS_ASSERT(user_data != 0);
			TS_ASSERT_EQUALS(*(uint*)user_data, sig+i);
		}

		lfl_free(&list);
		lfh_free(&hash);
	}
};


// known to fail on P4 due to mem reordering and lack of membars.
class TestMultithread : public CxxTest::TestSuite 
{
	// poor man's synchronization "barrier"
	bool is_complete;
	intptr_t num_active_threads;

	LFList list;
	LFHash hash;

	typedef std::set<uintptr_t> KeySet; 
	typedef KeySet::const_iterator KeySetIt;
	KeySet keys;
	pthread_mutex_t mutex;	// protects <keys>

	struct ThreadFuncParam
	{
		TestMultithread* this_;
		uintptr_t thread_number;

		ThreadFuncParam(TestMultithread* this__, uintptr_t thread_number_)
			: this_(this__), thread_number(thread_number_) {}
	};

	static void* thread_func(void* arg)
	{
		debug_set_thread_name("LF_test");

		ThreadFuncParam* param = (ThreadFuncParam*)arg;
		TestMultithread* this_        = param->this_;
		const uintptr_t thread_number = param->thread_number;

		atomic_add(&this_->num_active_threads, 1);

		// chosen randomly every iteration (int_value % 4)
		enum TestAction
		{
			TA_FIND   = 0,
			TA_INSERT = 1,
			TA_ERASE  = 2,
			TA_SLEEP  = 3
		};
		static const char* const action_strings[] =
		{
			"find", "insert", "erase", "sleep"
		};

		while(!this_->is_complete)
		{
			void* user_data;

			const int action            = rand(0, 4);
			const uintptr_t key         = rand(0, 100);
			const int sleep_duration_ms = rand(0, 100);
			debug_printf("thread %d: %s\n", thread_number, action_strings[action]);

			//
			pthread_mutex_lock(&this_->mutex);
			const bool was_in_set = this_->keys.find(key) != this_->keys.end();
			if(action == TA_INSERT)
				this_->keys.insert(key);
			else if(action == TA_ERASE)
				this_->keys.erase(key);
			pthread_mutex_unlock(&this_->mutex);

			switch(action)
			{
			case TA_FIND:
			{
				user_data = lfl_find(&this_->list, key);
				TS_ASSERT(was_in_set == (user_data != 0));
				if(user_data)
					TS_ASSERT_EQUALS(*(uintptr_t*)user_data, ~key);

				user_data = lfh_find(&this_->hash, key);
				// typical failure site if lockfree data structure has bugs.
				TS_ASSERT(was_in_set == (user_data != 0));
				if(user_data)
					TS_ASSERT_EQUALS(*(uintptr_t*)user_data, ~key);
			}
			break;

			case TA_INSERT:
			{
				int was_inserted;

				user_data = lfl_insert(&this_->list, key, sizeof(uintptr_t), &was_inserted);
				TS_ASSERT(user_data != 0);	// only triggers if out of memory
				*(uintptr_t*)user_data = ~key;	// checked above
				TS_ASSERT(was_in_set == !was_inserted);

				user_data = lfh_insert(&this_->hash, key, sizeof(uintptr_t), &was_inserted);
				TS_ASSERT(user_data != 0);	// only triggers if out of memory
				*(uintptr_t*)user_data = ~key;	// checked above
				TS_ASSERT(was_in_set == !was_inserted);
			}
			break;

			case TA_ERASE:
			{
				int err;

				err = lfl_erase(&this_->list, key);
				TS_ASSERT(was_in_set == (err == INFO_OK));

				err = lfh_erase(&this_->hash, key);
				TS_ASSERT(was_in_set == (err == INFO_OK));
			}
			break;

			case TA_SLEEP:
				usleep(sleep_duration_ms*1000);
				break;

			default:
				TS_FAIL(L"invalid TA_* action");
				break;
			}	// switch
		}	// while !is_complete

		atomic_add(&this_->num_active_threads, -1);
		TS_ASSERT(this_->num_active_threads >= 0);

		delete param;

		return 0;
	}

public:
	TestMultithread()
		: is_complete(false), num_active_threads(0),
		  list(), hash(),
		  mutex(0) {}

	void disabled_due_to_failure_on_p4_test_multithread()
	{
		// this test is randomized; we need deterministic results.
		srand(1);

		static const double TEST_LENGTH = 30.;	// [seconds]
		const double end_time = get_time() + TEST_LENGTH;
		is_complete = false;

		TS_ASSERT_OK(lfl_init(&list));
		TS_ASSERT_OK(lfh_init(&hash, 128));
		TS_ASSERT_OK(pthread_mutex_init(&mutex, 0));

		// spin off test threads (many, to force preemption)
		const uint NUM_THREADS = 16;
		for(uintptr_t i = 0; i < NUM_THREADS; i++)
		{
			ThreadFuncParam* param = new ThreadFuncParam(this, i);
			pthread_create(0, 0, thread_func, param);
		}

		// wait until time interval elapsed (if we get that far, all is well).
		while(get_time() < end_time)
			usleep(10*1000);

		// signal and wait for all threads to complete (poor man's barrier -
		// those aren't currently implemented in wpthread).
		is_complete = true;
		while(num_active_threads > 0)
			usleep(5*1000);

		lfl_free(&list);
		lfh_free(&hash);
		TS_ASSERT_OK(pthread_mutex_destroy(&mutex));
	}
};
