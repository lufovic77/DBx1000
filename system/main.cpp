#include "global.h"
#include "helper.h"
#include "ycsb.h"
#include "tpcc.h"
#include "thread.h"
#include "manager.h"
#include "mem_alloc.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"

#include "logging_thread.h"
#include "log.h"
#include "log_alg_list.h"
#include "locktable.h"
#include "log_pending_table.h"
#include "log_recover_table.h"
#include "free_queue.h"
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "numa.h"
void * f(void *);
void *f_log(void *);

thread_t ** m_thds;

LoggingThread **logging_thds;

// defined in parser.cpp
void parser(int argc, char * argv[]);

void handler(int sig)
{
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);

	raise(SIGABRT); // cause a core dump.
					//exit(1);
}

int main(int argc, char* argv[])
{	//signal(SIGBUS, handler);   // install our handler
	uint64_t mainstart = get_sys_clock();
	double mainstart_wallclock = get_wall_time();

	numa_set_strict(true); // panic if numa_alloc_onnode fails

	SHOW_DEFINE(NUM_CORES_PER_SLOT);
	SHOW_DEFINE(NUMA_NODE_NUM);
	SHOW_DEFINE(HYPER_THREADING_FACTOR);

#if UPDATE_SIMD
	printf("Using SIMD\n");
	SHOW_DEFINE(MM_MAX);
	SHOW_DEFINE(SIMD_PREFIX);
	SHOW_DEFINE(MAX_LOGGER_NUM_SIMD);
#endif

	if (BIG_HASH_TABLE_MODE == true)
		cout << "Running in big-hash-table mode." << endl;

	string dir;
	char hostname[256];
	gethostname(hostname, 256);
	if (strncmp(hostname, "draco", 5) == 0)
		dir = "./";
	if (strncmp(hostname, "yx", 2) == 0)
	{
		g_max_txns_per_thread = 100;
		cout << "[!] Detected desktop. Entering low disk-usage mode... " << endl;
	}


	parser(argc, argv);
	
	mem_allocator.init(g_part_cnt, MEM_SIZE / g_part_cnt); 
	stats = new Stats();
	stats->init();
	glob_manager = (Manager *) _mm_malloc(sizeof(Manager), 64);
	glob_manager->init();
	if (g_cc_alg == DL_DETECT) 
		dl_detector.init();
	printf("mem_allocator initialized!\n");
	workload * m_wl;
	switch (WORKLOAD) {
		case YCSB :
			m_wl = new ycsb_wl; break;
		case TPCC :
			m_wl = new tpcc_wl; break;
		
		default:
			assert(false);
	}
	m_wl->init();
	printf("workload initialized!\n");
	
	uint64_t thd_cnt = g_thread_cnt;
	pthread_t p_thds[thd_cnt - 1];
	m_thds = new thread_t * [thd_cnt];
	for (uint32_t i = 0; i < thd_cnt; i++)
		m_thds[i] = (thread_t *) _mm_malloc(sizeof(thread_t), 64);
	// query_queue should be the last one to be initialized!!!
	// because it collects txn latency
	query_queue = (Query_queue *) _mm_malloc(sizeof(Query_queue), 64);
	if (WORKLOAD != TEST)
		query_queue->init(m_wl);
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );
	printf("query_queue initialized!\n");
#if CC_ALG == HSTORE
	part_lock_man.init();
#elif CC_ALG == OCC
	occ_man.init();
#elif CC_ALG == VLL
	vll_man.init();
#endif

	for (uint32_t i = 0; i < thd_cnt; i++) 
		m_thds[i]->init(i, m_wl);

	if (WARMUP > 0){
		printf("WARMUP start!\n");
		for (uint32_t i = 0; i < thd_cnt - 1; i++) {
			uint64_t vid = i;
			pthread_create(&p_thds[i], NULL, f, (void *)vid);
		}
		f((void *)(thd_cnt - 1));
		for (uint32_t i = 0; i < thd_cnt - 1; i++)
			pthread_join(p_thds[i], NULL);
		printf("WARMUP finished!\n");
	}
	warmup_finish = true;
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );
#ifndef NOGRAPHITE
	CarbonBarrierInit(&enable_barrier, g_thread_cnt);
#endif
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );

	// spawn and run txns again.
	int64_t starttime = get_server_clock();
	for (uint32_t i = 0; i < thd_cnt - 1; i++) {
		uint64_t vid = i;
		pthread_create(&p_thds[i], NULL, f, (void *)vid);
	}
	f((void *)(thd_cnt - 1));
	for (uint32_t i = 0; i < thd_cnt - 1; i++) 
		pthread_join(p_thds[i], NULL);
	int64_t endtime = get_server_clock();
	


	
	return 0;
}

void * f(void * id) {
	uint64_t tid = (uint64_t)id;
	m_thds[tid]->run();
	return NULL;
}

void *f_log(void *id)
{
#if LOG_ALGORITHM != LOG_NO
	uint64_t tid = (uint64_t)id;
	logging_thds[(tid + g_thread_cnt) % g_num_logger]->run();
#endif
	return NULL;
}