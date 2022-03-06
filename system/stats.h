#pragma once 


enum StatsFloat {
	#define DEF_STAT(x) STAT_##x,
	#include "stats_float_enum.def"
	#undef DEF_STAT
	NUM_FLOAT_STATS
};

enum StatsInt {
	#define DEF_STAT(x) STAT_##x,
	#include "stats_int_enum.def"
	#undef DEF_STAT
	NUM_INT_STATS
};

class Stats_thd {
public:
	void init(uint64_t thd_id);
	void clear();

	Stats_thd(uint64_t i);
	void copy_from(Stats_thd * stats_thd);
	
	//double _float_stats[NUM_FLOAT_STATS];
	//uint64_t _int_stats[NUM_INT_STATS];
	double * _float_stats;
	uint64_t * _int_stats;

	char _pad2[CL_SIZE];
	uint64_t txn_cnt;
	uint64_t abort_cnt;
	double run_time;
	double time_man;
	double time_index;
	double time_wait;
	double time_abort;
	double time_cleanup;
	uint64_t time_ts_alloc;
	double time_query;
	uint64_t wait_cnt;
	uint64_t debug1;
	uint64_t debug2;
	uint64_t debug3;
	uint64_t debug4;
	uint64_t debug5;
	
	uint64_t latency;
	uint64_t * all_debug1;
	uint64_t * all_debug2;
	char _pad[CL_SIZE];
};

class Stats_tmp {
public:
	void init();
	void clear();
	double time_man;
	double time_index;
	double time_wait;
	char _pad[CL_SIZE - sizeof(double)*3];
};

class Stats {
public:

	Stats();
	// PER THREAD statistics
	Stats_thd ** _stats;
	// stats are first written to tmp_stats, if the txn successfully commits, 
	// copy the values in tmp_stats to _stats
	Stats_tmp ** tmp_stats;
	
	// GLOBAL statistics
	double dl_detect_time;
	double dl_wait_time;
	uint64_t cycle_detect;
	uint64_t deadlock;	


	// output thread	
	uint64_t bytes_sent;
	uint64_t bytes_recv;

	void init();
	void init(uint64_t thread_id);
	void clear(uint64_t tid);
	void add_debug(uint64_t thd_id, uint64_t value, uint32_t select);
	void commit(uint64_t thd_id);
	void abort(uint64_t thd_id);
	void print();
	void print_lat_distr();

	void output(std::ostream * os); 
	
	std::string statsFloatName[NUM_FLOAT_STATS] = {
		// worker thread
		#define DEF_STAT(x) #x,
		#include "stats_float_enum.def"
		#undef DEF_STAT
	};

	std::string statsIntName[NUM_INT_STATS] = {
		#define DEF_STAT(x) #x,
		#include "stats_int_enum.def"
		#undef DEF_STAT
	};
private:
	uint32_t _total_thread_cnt;
	//vector<double> _aggregate_latency;
	//vector<Stats *> _checkpoints;
    //uint32_t        _num_cp;

};
