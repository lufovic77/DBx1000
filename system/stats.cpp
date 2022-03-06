#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "global.h"
#include "helper.h"
#include "stats.h"
#include "mem_alloc.h"

#include <inttypes.h>
#include <iomanip>
#define BILLION 1000000000UL


#ifndef PRIu64
#define PRIu64 "ld"
#endif

Stats_thd::Stats_thd(uint64_t i)
{
	init(i);
	clear();
}

void Stats_thd::init(uint64_t thd_id) {
	clear();
	all_debug1 = (uint64_t *)
		_mm_malloc(sizeof(uint64_t) * MAX_TXN_PER_PART, 64);
	all_debug2 = (uint64_t *)
		_mm_malloc(sizeof(uint64_t) * MAX_TXN_PER_PART, 64);
}

void Stats_thd::clear() {
	txn_cnt = 0;
	abort_cnt = 0;
	run_time = 0;
	time_man = 0;
	debug1 = 0;
	debug2 = 0;
	debug3 = 0;
	debug4 = 0;
	debug5 = 0;
	time_index = 0;
	time_abort = 0;
	time_cleanup = 0;
	time_wait = 0;
	time_ts_alloc = 0;
	latency = 0;
	time_query = 0;

	memset(_int_stats, 0, sizeof(uint64_t) * NUM_INT_STATS);
	for (uint32_t i = 0; i < NUM_FLOAT_STATS; i++)
		_float_stats[i] = 0;
}

void Stats_thd::copy_from(Stats_thd *stats_thd)
{
	memcpy(_float_stats, stats_thd->_float_stats, sizeof(double) * NUM_FLOAT_STATS);
	memcpy(_int_stats, stats_thd->_int_stats, sizeof(double) * NUM_INT_STATS);
}

void Stats_tmp::init() {
	clear();
}

void Stats_tmp::clear() {	
	time_man = 0;
	time_index = 0;
	time_wait = 0;
}

void Stats::init() {
	if (!STATS_ENABLE) 
		return;
	_stats = (Stats_thd**) 
			_mm_malloc(sizeof(Stats_thd*) * g_thread_cnt, 64);
	tmp_stats = (Stats_tmp**) 
			_mm_malloc(sizeof(Stats_tmp*) * g_thread_cnt, 64);
	dl_detect_time = 0;
	dl_wait_time = 0;
	deadlock = 0;
	cycle_detect = 0;
}

void Stats::init(uint64_t thread_id) {
	if (!STATS_ENABLE) 
		return;
	_stats[thread_id] = (Stats_thd *) 
		_mm_malloc(sizeof(Stats_thd), 64);
	tmp_stats[thread_id] = (Stats_tmp *)
		_mm_malloc(sizeof(Stats_tmp), 64);

	_stats[thread_id]->init(thread_id);
	tmp_stats[thread_id]->init();
}

void Stats::clear(uint64_t tid) {
	if (STATS_ENABLE) {
		_stats[tid]->clear();
		tmp_stats[tid]->clear();

		dl_detect_time = 0;
		dl_wait_time = 0;
		cycle_detect = 0;
		deadlock = 0;
	}
}


void Stats::output(std::ostream *os)
{
	std::ostream &out = *os;

	uint64_t total_num_commits = 0;
	double total_run_time = 0;
	uint64_t total_logging_run_time_int = 0;
	double max_run_time = 0;
	uint64_t max_logging_time_int = 0;

	double PerThreadAvgThroughput = 0;

	for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
	{
		total_num_commits += _stats[tid]->_int_stats[STAT_num_commits];
		_stats[tid]->_float_stats[STAT_run_time] /= CPU_FREQ;
		// because we are using the raw rdtsc
		total_run_time += _stats[tid]->_float_stats[STAT_run_time];
		total_logging_run_time_int += (double)_stats[tid]->_int_stats[STAT_time_logging_thread];
		if (_stats[tid]->_float_stats[STAT_run_time] > max_run_time)
			max_run_time = _stats[tid]->_float_stats[STAT_run_time];
		if (_stats[tid]->_int_stats[STAT_time_logging_thread] > max_logging_time_int)
			max_logging_time_int = _stats[tid]->_int_stats[STAT_time_logging_thread];
		if (_stats[tid]->_float_stats[STAT_run_time] > 0)
			PerThreadAvgThroughput += _stats[tid]->_int_stats[STAT_num_commits] / _stats[tid]->_float_stats[STAT_run_time];
	}

	double total_logging_run_time = (double)total_logging_run_time_int / CPU_FREQ;
	double max_logging_time = (double)max_logging_time_int / CPU_FREQ;

	//assert(total_num_commits > 0);
	out << "=Worker Thread=" << endl;

	double Throughput = BILLION * total_num_commits / 
		MAX(total_run_time / g_thread_cnt, total_logging_run_time / g_num_logger);

#if LOG_ALGORITHM == LOG_SERIAL
	if (g_log_recover)
	{
		Throughput = BILLION * _stats[0]->_int_stats[STAT_num_commits] / 
			MAX(_stats[0]->_float_stats[STAT_run_time], _stats[0]->_int_stats[STAT_time_logging_thread] / CPU_FREQ);
	}
#elif LOG_ALGORITHM == LOG_TAURUS
	// high-contention mode
	if (g_log_recover && g_zipf_theta > CONTENTION_THRESHOLD  && !PER_WORKER_RECOVERY)
	{
		Throughput = BILLION * _stats[0]->_int_stats[STAT_num_commits] / 
			MAX(_stats[0]->_float_stats[STAT_run_time], _stats[0]->_int_stats[STAT_time_logging_thread] / CPU_FREQ);
	}
#elif LOG_ALGORITHM == LOG_PLOVER
	// we need to remove those empty logs
	uint64_t fully_working_worker_num = 0;
	uint64_t fully_working_worker_commit = 0;
	double total_fully_working_worker_run_time = 0;
	for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
	{
		if (_stats[tid]->_float_stats[STAT_run_time] > max_run_time * OUTPUT_AVG_RATIO)
		{
			fully_working_worker_num++;
			fully_working_worker_commit += _stats[tid]->_int_stats[STAT_num_commits];
			total_fully_working_worker_run_time += _stats[tid]->_float_stats[STAT_run_time];
		}
	}
	Throughput = BILLION * fully_working_worker_commit / total_fully_working_worker_run_time * g_thread_cnt;
	if (fully_working_worker_num < g_thread_cnt * OUTPUT_AVG_RATIO)
		std::cerr << "Warning: Not many threads cost close to max running time. Running time not uniform." << endl;
#endif
	out << "    " << setw(30) << left << "Throughput:"
		<< Throughput << endl;

	double MaxThr = BILLION * total_num_commits / MAX(max_run_time, max_logging_time);
	out << "    " << setw(30) << left << "MaxThr:"
		<< MaxThr << endl;

	out << "    " << setw(30) << left << "PerThdThr:"
		<< BILLION * PerThreadAvgThroughput << endl;

	double AvgRatio = Throughput / MaxThr; // should be larger than 1
	if (AvgRatio > 1 / OUTPUT_AVG_RATIO)
		std::cerr << "Warning: Throughput and Max Throughput deviate. Running time not uniform." << endl;

	double log_bytes_total = 0.;
	// print floating point stats
	for (uint32_t i = 0; i < NUM_FLOAT_STATS; i++)
	{
		double total = 0;
		for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
		{
			total += _stats[tid]->_float_stats[i];
		}
		//if (i == STAT_latency)
		//	total /= total_num_commits;
		string suffix = "";
		out << "    " << setw(30) << left << statsFloatName[i] + suffix + ':' << total / BILLION;
		out << " (";
		for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
		{
			out << _stats[tid]->_float_stats[i] / BILLION << ',';
		}
		out << ')' << endl;
		if (i == STAT_log_bytes)
			log_bytes_total = total;
	}

	out << endl;

#if COLLECT_LATENCY
	double avg_latency = 0;
	for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
		avg_latency += _stats[tid]->_float_stats[STAT_txn_latency];
	avg_latency /= total_num_commits;

	out << "    " << setw(30) << left << "average_latency:" << avg_latency / BILLION << endl;
	// print latency distribution
	out << "    " << setw(30) << left << "90%_latency:"
		<< _aggregate_latency[(uint64_t)(total_num_commits * 0.90)] / BILLION << endl;
	out << "    " << setw(30) << left << "95%_latency:"
		<< _aggregate_latency[(uint64_t)(total_num_commits * 0.95)] / BILLION << endl;
	out << "    " << setw(30) << left << "99%_latency:"
		<< _aggregate_latency[(uint64_t)(total_num_commits * 0.99)] / BILLION << endl;
	out << "    " << setw(30) << left << "max_latency:"
		<< _aggregate_latency[total_num_commits - 1] / BILLION << endl;

	out << endl;
#endif
	// print integer stats
	double time_io_total = 1., time_io_max = 0.;
	for (uint32_t i = 0; i < NUM_INT_STATS; i++)
	{
		double total = 0;
		double non_zero_total = 0.;
		double max_item = 0;
		int non_zero_indices = 0;
		for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
		{
			total += _stats[tid]->_int_stats[i];
			if (_stats[tid]->_int_stats[i] > 0)
			{
				non_zero_total += _stats[tid]->_int_stats[i];
				non_zero_indices++;
			}
			if (max_item < _stats[tid]->_int_stats[i])
				max_item = _stats[tid]->_int_stats[i];
		}
		if (statsIntName[i].substr(0, 4) == "time")
		{
			double nonzero_avg = (double)non_zero_total / CPU_FREQ / BILLION / non_zero_indices;
			out << "    " << setw(30) << left << statsIntName[i] + ':' << (double)total / CPU_FREQ / BILLION;
			cout << " " << nonzero_avg;
			cout << " " << nonzero_avg / (total_run_time / CPU_FREQ / BILLION / g_thread_cnt) * 100.0 << "%";
			cout << " " << (double)total / CPU_FREQ / total_num_commits;
			out << " (";
			for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
				out << (double)_stats[tid]->_int_stats[i] / CPU_FREQ / BILLION << ',';
			out << ')' << endl;
		}
		else
		{
			out << "    " << setw(30) << left << statsIntName[i] + ':' << total;
			out << " (";
			for (uint32_t tid = 0; tid < _total_thread_cnt; tid++)
				out << _stats[tid]->_int_stats[i] << ',';
			out << ')' << endl;
		}
		if (i == STAT_time_io)
		{
			time_io_total = (double)total / CPU_FREQ / BILLION;
			time_io_max = max_item / CPU_FREQ / BILLION;
		}
	}

	if (LOG_ALGORITHM == LOG_BATCH && g_log_recover)
	{
		out << "Projected Disk Bandwidth Utilized - avg " << log_bytes_total / time_io_total * g_thread_cnt << " real " << log_bytes_total / time_io_max << endl;
	}
	else
	{
		out << "Projected Disk Bandwidth Utilized - avg " << log_bytes_total / time_io_total * g_num_logger << " real " << log_bytes_total / time_io_max << endl;
	}
}

void Stats::add_debug(uint64_t thd_id, uint64_t value, uint32_t select) {
	if (g_prt_lat_distr && warmup_finish) {
		uint64_t tnum = _stats[thd_id]->txn_cnt;
		if (select == 1)
			_stats[thd_id]->all_debug1[tnum] = value;
		else if (select == 2)
			_stats[thd_id]->all_debug2[tnum] = value;
	}
}

void Stats::commit(uint64_t thd_id) {
	if (STATS_ENABLE) {
		_stats[thd_id]->time_man += tmp_stats[thd_id]->time_man;
		_stats[thd_id]->time_index += tmp_stats[thd_id]->time_index;
		_stats[thd_id]->time_wait += tmp_stats[thd_id]->time_wait;
		tmp_stats[thd_id]->init();
	}
}

void Stats::abort(uint64_t thd_id) {	
	if (STATS_ENABLE) 
		tmp_stats[thd_id]->init();
}

void Stats::print() {
	
	uint64_t total_txn_cnt = 0;
	uint64_t total_abort_cnt = 0;
	double total_run_time = 0;
	double total_time_man = 0;
	double total_debug1 = 0;
	double total_debug2 = 0;
	double total_debug3 = 0;
	double total_debug4 = 0;
	double total_debug5 = 0;
	double total_time_index = 0;
	double total_time_abort = 0;
	double total_time_cleanup = 0;
	double total_time_wait = 0;
	double total_time_ts_alloc = 0;
	double total_latency = 0;
	double total_time_query = 0;
	for (uint64_t tid = 0; tid < g_thread_cnt; tid ++) {
		total_txn_cnt += _stats[tid]->txn_cnt;
		total_abort_cnt += _stats[tid]->abort_cnt;
		total_run_time += _stats[tid]->run_time;
		total_time_man += _stats[tid]->time_man;
		total_debug1 += _stats[tid]->debug1;
		total_debug2 += _stats[tid]->debug2;
		total_debug3 += _stats[tid]->debug3;
		total_debug4 += _stats[tid]->debug4;
		total_debug5 += _stats[tid]->debug5;
		total_time_index += _stats[tid]->time_index;
		total_time_abort += _stats[tid]->time_abort;
		total_time_cleanup += _stats[tid]->time_cleanup;
		total_time_wait += _stats[tid]->time_wait;
		total_time_ts_alloc += _stats[tid]->time_ts_alloc;
		total_latency += _stats[tid]->latency;
		total_time_query += _stats[tid]->time_query;
		
		printf("[tid=%ld] txn_cnt=%ld,abort_cnt=%ld\n", 
			tid,
			_stats[tid]->txn_cnt,
			_stats[tid]->abort_cnt
		);
	}
	FILE * outf;
	if (output_file != NULL) {
		outf = fopen(output_file, "w");
		fprintf(outf, "[summary] txn_cnt=%ld, abort_cnt=%ld"
			", run_time=%f, time_wait=%f, time_ts_alloc=%f"
			", time_man=%f, time_index=%f, time_abort=%f, time_cleanup=%f, latency=%f"
			", deadlock_cnt=%ld, cycle_detect=%ld, dl_detect_time=%f, dl_wait_time=%f"
			", time_query=%f, debug1=%f, debug2=%f, debug3=%f, debug4=%f, debug5=%f\n",
			total_txn_cnt, 
			total_abort_cnt,
			total_run_time / BILLION,
			total_time_wait / BILLION,
			total_time_ts_alloc / BILLION,
			(total_time_man - total_time_wait) / BILLION,
			total_time_index / BILLION,
			total_time_abort / BILLION,
			total_time_cleanup / BILLION,
			total_latency / BILLION / total_txn_cnt,
			deadlock,
			cycle_detect,
			dl_detect_time / BILLION,
			dl_wait_time / BILLION,
			total_time_query / BILLION,
			total_debug1, // / BILLION,
			total_debug2, // / BILLION,
			total_debug3, // / BILLION,
			total_debug4, // / BILLION,
			total_debug5 / BILLION
		);
		fclose(outf);
	}
	printf("[summary] txn_cnt=%ld, abort_cnt=%ld"
		", run_time=%f, time_wait=%f, time_ts_alloc=%f"
		", time_man=%f, time_index=%f, time_abort=%f, time_cleanup=%f, latency=%f"
		", deadlock_cnt=%ld, cycle_detect=%ld, dl_detect_time=%f, dl_wait_time=%f"
		", time_query=%f, debug1=%f, debug2=%f, debug3=%f, debug4=%f, debug5=%f\n", 
		total_txn_cnt, 
		total_abort_cnt,
		total_run_time / BILLION,
		total_time_wait / BILLION,
		total_time_ts_alloc / BILLION,
		(total_time_man - total_time_wait) / BILLION,
		total_time_index / BILLION,
		total_time_abort / BILLION,
		total_time_cleanup / BILLION,
		total_latency / BILLION / total_txn_cnt,
		deadlock,
		cycle_detect,
		dl_detect_time / BILLION,
		dl_wait_time / BILLION,
		total_time_query / BILLION,
		total_debug1 / BILLION,
		total_debug2, // / BILLION,
		total_debug3, // / BILLION,
		total_debug4, // / BILLION,
		total_debug5  // / BILLION 
	);
	if (g_prt_lat_distr)
		print_lat_distr();
}

void Stats::print_lat_distr() {
	FILE * outf;
	if (output_file != NULL) {
		outf = fopen(output_file, "a");
		for (UInt32 tid = 0; tid < g_thread_cnt; tid ++) {
			fprintf(outf, "[all_debug1 thd=%d] ", tid);
			for (uint32_t tnum = 0; tnum < _stats[tid]->txn_cnt; tnum ++) 
				fprintf(outf, "%ld,", _stats[tid]->all_debug1[tnum]);
			fprintf(outf, "\n[all_debug2 thd=%d] ", tid);
			for (uint32_t tnum = 0; tnum < _stats[tid]->txn_cnt; tnum ++) 
				fprintf(outf, "%ld,", _stats[tid]->all_debug2[tnum]);
			fprintf(outf, "\n");
		}
		fclose(outf);
	} 
}
