#include "global.h"
#include "helper.h"

#include <iostream>
#include <algorithm>
#include <iterator>
#include <thread>
void print_usage() {
	printf("[usage]:\n");
	printf("\t-pINT       ; PART_CNT\n");
	printf("\t-vINT       ; VIRTUAL_PART_CNT\n");
	printf("\t-tINT       ; THREAD_CNT\n");
	printf("\t-qINT       ; QUERY_INTVL\n");
	printf("\t-dINT       ; PRT_LAT_DISTR\n");
	printf("\t-aINT       ; PART_ALLOC (0 or 1)\n");
	printf("\t-mINT       ; MEM_PAD (0 or 1)\n");
	printf("\t-GaINT      ; ABORT_PENALTY (in ms)\n");
	printf("\t-GcINT      ; CENTRAL_MAN\n");
	printf("\t-GtINT      ; TS_ALLOC\n");
	printf("\t-GkINT      ; KEY_ORDER\n");
	printf("\t-GnINT      ; NO_DL\n");
	printf("\t-GoINT      ; TIMEOUT\n");
	printf("\t-GlINT      ; DL_LOOP_DETECT\n");
	
	printf("\t-GbINT      ; TS_BATCH_ALLOC\n");
	printf("\t-GuINT      ; TS_BATCH_NUM\n");
	
	printf("\t-o STRING   ; output file\n\n");
	printf("  [YCSB]:\n");
	printf("\t-cINT       ; PART_PER_TXN\n");
	printf("\t-eINT       ; PERC_MULTI_PART\n");
	printf("\t-rFLOAT     ; READ_PERC\n");
	printf("\t-wFLOAT     ; WRITE_PERC\n");
	printf("\t-zFLOAT     ; ZIPF_THETA\n");
	printf("\t-sINT       ; SYNTH_TABLE_SIZE\n");
	printf("\t-RINT       ; REQ_PER_QUERY\n");
	printf("\t-fINT       ; FIELD_PER_TUPLE\n");
	printf("  [TPCC]:\n");
	printf("\t-nINT       ; NUM_WH\n");
	printf("\t-TpFLOAT    ; PERC_PAYMENT\n");
	printf("\t-TuINT      ; WH_UPDATE\n");
	printf("  [TEST]:\n");
	printf("\t-Ar         ; Test READ_WRITE\n");
	printf("\t-Ac         ; Test CONFLIT\n");
}

void parser(int argc, char * argv[]) {
	g_params["abort_buffer_enable"] = ABORT_BUFFER_ENABLE? "true" : "false";
	g_params["write_copy_form"] = WRITE_COPY_FORM;
	g_params["validation_lock"] = VALIDATION_LOCK;
	g_params["pre_abort"] = PRE_ABORT;
	g_params["atomic_timestamp"] = ATOMIC_TIMESTAMP;

	for (int i = 1; i < argc; i++) {
		assert(argv[i][0] == '-');
		if (argv[i][1] == 'a')
			g_part_alloc = atoi( &argv[i][2] );
		else if (argv[i][1] == 'm')
			g_mem_pad = atoi( &argv[i][2] );
		else if (argv[i][1] == 'q')
			g_query_intvl = atoi( &argv[i][2] );
		else if (argv[i][1] == 'c')
			g_part_per_txn = atoi( &argv[i][2] );
		else if (argv[i][1] == 'e')
			g_perc_multi_part = atof( &argv[i][2] );
		else if (argv[i][1] == 'r') 
			g_read_perc = atof( &argv[i][2] );
		else if (argv[i][1] == 'w') 
			g_write_perc = atof( &argv[i][2] );
		else if (argv[i][1] == 'z')
			g_zipf_theta = atof( &argv[i][2] );
		else if (argv[i][1] == 'd')
			g_prt_lat_distr = atoi( &argv[i][2] );
		else if (argv[i][1] == 'p')
			g_part_cnt = atoi( &argv[i][2] );
		else if (argv[i][1] == 'v')
			g_virtual_part_cnt = atoi( &argv[i][2] );
		else if (argv[i][1] == 't')
			g_thread_cnt = atoi( &argv[i][2] );
		else if (argv[i][1] == 's')
			g_synth_table_size = atoi( &argv[i][2] );
		else if (argv[i][1] == 'R') 
			g_req_per_query = atoi( &argv[i][2] );
		else if (argv[i][1] == 'f')
			g_field_per_tuple = atoi( &argv[i][2] );
		else if (argv[i][1] == 'n')
			g_num_wh = atoi( &argv[i][2] );
		else if (argv[i][1] == 'G') {
			if (argv[i][2] == 'a')
				g_abort_penalty = atoi( &argv[i][3] );
			else if (argv[i][2] == 'c')
				g_central_man = atoi( &argv[i][3] );
			else if (argv[i][2] == 't')
				g_ts_alloc = atoi( &argv[i][3] );
			else if (argv[i][2] == 'k')
				g_key_order = atoi( &argv[i][3] );
			else if (argv[i][2] == 'n')
				g_no_dl = atoi( &argv[i][3] );
			else if (argv[i][2] == 'o')
				g_timeout = atol( &argv[i][3] );
			else if (argv[i][2] == 'l')
				g_dl_loop_detect = atoi( &argv[i][3] );
			else if (argv[i][2] == 'b')
				g_ts_batch_alloc = atoi( &argv[i][3] );
			else if (argv[i][2] == 'u')
				g_ts_batch_num = atoi( &argv[i][3] );
		} else if (argv[i][1] == 'T') {
			if (argv[i][2] == 'p')
				g_perc_payment = atof( &argv[i][3] );
			if (argv[i][2] == 'u')
				g_wh_update = atoi( &argv[i][3] );
		} else if (argv[i][1] == 'A') {
			if (argv[i][2] == 'r')
				g_test_case = READ_WRITE;
			if (argv[i][2] == 'c')
				g_test_case = CONFLICT;
		}
		// Logging
		else if (argv[i][1] == 'L'){
			if (argv[i][2] == 'b')
			{
				g_log_buffer_size = strtoull( &argv[i][3], NULL, 10);
				//g_read_blocksize = (uint64_t)(g_log_buffer_size * g_recover_buffer_perc);
			}
			else if(argv[i][2] == 'e') {
				g_max_num_epoch = atoi(&argv[i][3]);
			}
			else if (argv[i][2] == 'r') {
				char c = argv[i][3];
				assert(c == '0' || c == '1');
				g_log_recover = (c == '1')? true : false;
			}
			else if (argv[i][2] == 'R') {
				// RAMDISK
				char c = argv[i][3];
				assert(c == '0' || c == '1');
				g_ramdisk = (c == '1')? true : false;
			}
			else if (argv[i][2] == 'a') 
			{
				if (argv[i][3] == 'd')
					g_rlv_delta = atoi( &argv[i][4] );
				else if (argv[i][3] == 'l')
					g_loggingthread_rlv_freq = atoi( &argv[i][4] );
				else
					assert(false);
			}
			else if (argv[i][2] == 'n') 
				g_num_logger = atoi( &argv[i][3] );
			else if (argv[i][2] == 'D')
				g_num_disk = atoi( &argv[i][3] );
			else if (argv[i][2] == 'B')
				g_flush_blocksize = atoi(&argv[i][3]);
			else if (argv[i][2] == 'K')
			{
				// recommend putting -LK at the end.
				g_read_blocksize = atoi(&argv[i][3]);
			}
			else if (argv[i][2] == 'f') { 
				char c = argv[i][3];
				assert(c == '0' || c == '1');
				g_no_flush = (c == '1')? true : false;
			} else if (argv[i][2] == 'k')
				g_log_parallel_num_buckets = atoi( &argv[i][3] );
			else if (argv[i][2] == 'j') 
				g_epoch_period = atof( &argv[i][3] );
			else if (argv[i][2] == 'p') 
				g_num_pools = atoi( &argv[i][3] );
			else if (argv[i][2] == 'i')
				g_locktable_init_slots = atoi( &argv[i][3]);
			else if (argv[i][2] == 'c')
				g_log_chunk_size = atoi( &argv[i][3] );
			else if (argv[i][2] == 't')
				g_flush_interval = atoi( &argv[i][3] ); 
			else if (argv[i][2] == 's')
			{
				printf("Warning: -Ls (g_recover_buffer_perc) is deprecated.\n");
				assert(false);
				g_recover_buffer_perc = atof( &argv[i][3] ); 
				//g_read_blocksize = (uint64_t)(g_log_buffer_size * g_recover_buffer_perc);
			}
			else if (argv[i][2] == 'z')
				g_poolsize_wait = atof( &argv[i][3] ); 
			else if (argv[i][2] == 'w')
				g_scan_window = atof( &argv[i][3] );				
			else if (argv[i][2] == 'd')
			{
				g_max_log_entry_size = atoi( &argv[i][3]);
			}
			else assert(false);
		}
		else if (argv[i][1] == 'o') {
			i++;
			output_file = argv[i];
		}
		else if (argv[i][1] == 'h') {
			print_usage();
			exit(0);
		} 
		else if (argv[i][1] == '-') {
			string line(&argv[i][2]);
			size_t pos = line.find("="); 
			assert(pos != string::npos);
			string name = line.substr(0, pos);
			string value = line.substr(pos + 1, line.length());
			assert(g_params.find(name) != g_params.end());
			g_params[name] = value;
		}
		else
			assert(false);
	}
	if (g_thread_cnt < g_init_parallelism)
		g_init_parallelism = g_thread_cnt;
}
