//ADD YOUR HEADER COMMENT
////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2013,2019-2020
// Posting or sharing this file is prohibited, including any changes/additions.
//
////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
//
///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Main File:        csim.c
// This File:        csim.c
// Semester:         CS 354 Spring 2020
//
// Author:           Zach Huemann
// Email:            zhuemann@wisc.eddu
// CS Login:         huemann
//
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//
// Online sources:   avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of 
//                   of any information you find.
//////////////////////////// 80 columns wide /////////////////////////////////// 

/*
 * csim.c:  
 * A cache simulator that can replay traces (from Valgrind) and output
 * statistics for the number of hits, misses, and evictions.
 * The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most 1 cache miss plus a possible eviction.
 *  2. Instruction loads (I) are ignored.
 *  3. Data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus a possible eviction.
 */  

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/******************************************************************************/
/* DO NOT MODIFY THESE VARIABLES **********************************************/

//Globals set by command line args.
int b = 0; //number of block (b) bits
int s = 0; //number of set (s) bits
int E = 0; //number of lines per set

//Globals derived from command line args.
int B; //block size in bytes: B = 2^b
int S; //number of sets: S = 2^s

//Global counters to track cache statistics in access_data().
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;

//Global to control trace output
int verbosity = 0; //print trace if set
/******************************************************************************/
  
  
//Type mem_addr_t: Use when dealing with addresses or address masks.
typedef unsigned long long int mem_addr_t;

//Type cache_line_t: Use when dealing with cache lines.
typedef struct cache_line {                    
    char valid;
    mem_addr_t tag;
    //Add a data member as needed by your implementation for LRU tracking.
    //hit_count keeps track of the last time this cache_line had a hit
    int hit_count;
} cache_line_t;

//Type cache_set_t: Use when dealing with cache sets
//Note: Each set is a pointer to a heap array of one or more cache lines.
typedef cache_line_t* cache_set_t;
//Type cache_t: Use when dealing with the cache.
//Note: A cache is a pointer to a heap array of one or more sets.
typedef cache_set_t* cache_t;

// Create the cache (i.e., pointer var) we're simulating.
cache_t cache;  

/* 
 * init_cache:
 * Allocates the data structure for a cache with S sets and E lines per set.
 * Initializes all valid bits and tags with 0s.
 */                    
void init_cache() {    

    //update the values of S and B based on s and b inputs
    S = pow(2,s);
    B = pow(2,b);

    //creat the new cache with size S*E	
    //cache_t newCache = (cache_t)malloc(S*E*sizeof(cache_line_t*)); 
    cache_line_t** newcache = (cache_line_t**)malloc(S*sizeof(cache_line_t));
    //malloc returnedd null so exit the program with a message
    if (newcache == NULL) {
	printf("error, malloc did not allocate correctly");
	exit(1);
    }
    //loops through s and creates s number of sets
    for (int i = 0; i < S; i++) {
	//create sets
	newcache[i] = (cache_line_t*)malloc(E*sizeof(cache_line_t));
	//malloc returnedd null so exit the program with a message
	if (newcache[i] == NULL) {
            printf("error, malloc did not allocate correctly");
            exit(1);
        }
    }

    //initializs the value of each element in the cache
    for (int i = 0; i < S; i++) {
	for (int j = 0; j < E; j++) {
            newcache[i][j].tag = 0;
	    newcache[i][j].valid = 0;
	    newcache[i][j].hit_count = 0;
	}
    }

    cache = newcache;
    return;
}
  

/* 
 * free_cache:
 * Frees all heap allocated memory used by the cache.
 */                    
void free_cache() {
    //frees up the heap that was allocated for each set in initialization
    for( int i = 0; i < S; i++) {
        free(cache[i]);
    }
    //rees up the cache array
    free(cache);   
    return; 
}
   
   
/* 
 * access_data:
 * Simulates data access at given "addr" memory address in the cache.
 *
 * If already in cache, increment hit_cnt
 * If not in cache, cache it (set tag), increment miss_cnt
 * If a line is evicted, increment evict_cnt
 */                    
void access_data(mem_addr_t addr) { 
    //calulates how tag teh tag should be
    int tag_size = (64 - s - b);
    //left shifts the addrress to isolate the tag bits
    mem_addr_t tag = addr >>(s+b);
    //right shifts the addr to remove the tag bits and shits them back
    //so you essentially mask out the tag. then you also shift right 
    //by the b bits. This will leave you with just the s bits
    mem_addr_t sBits = addr << tag_size;
    sBits = sBits >> tag_size;
    sBits = sBits >> b;

    //keeps tract of if there has been a hit or not
    bool hasHit = false; 
    //tracks how many times we have accessed the memeory
    int highestCounter = 0;
    //keeps tract of the index that should be evicted if it has to be
    int to_be_evicted = 0;
    // an initial value for times the mememory has been accessed
    int lowest_counter = cache[sBits][0].hit_count;

    //loops through the set and finds the highest and counter
    for (int j = 0; j < E; j++) {
	//finds the highest counter 
        if (cache[sBits][j].hit_count > highestCounter) {
            highestCounter = cache[sBits][j].hit_count;
	}
	//finds the lowest counter and keeps track of that index
	if (cache[sBits][j].hit_count <= lowest_counter) {
	   lowest_counter = cache[sBits][j].hit_count;
	   to_be_evicted = j; //index that will be evicted
	}
    }	

    //loops through the cache lines in the specific cache set
    //if the tag is a match and is valid its a hit. 
    for (int j = 0; j < E; j++) {
        if (cache[sBits][j].tag == tag) {
            if (cache[sBits][j].valid == 1) {
	        hit_cnt++;
		hasHit = true;
		cache[sBits][j].hit_count = highestCounter + 1;
		cache[sBits][j].valid = 1;
		cache[sBits][j].tag = tag;
		return;
	    }
	}

    }	
    
    bool found_empty_line = false;
    //this checks for cold misses and fills up the cache if its free
    if (hasHit == false) {
	miss_cnt++;
	//loops through looking for a free spot in the cache
	for (int j = 0; j < E; j++) {
	    //checks to see if there is an empty cache line open
	    if (cache[sBits][j].valid == 0) {
		//updates the cache
		cache[sBits][j].valid = 1;
		cache[sBits][j].tag = tag;
		cache[sBits][j].hit_count = highestCounter + 1;
		found_empty_line = true;
		return;
	    }
	}
    }

    //evict least used line	
    if (found_empty_line == false) {
	evict_cnt++;
	//updates cache that was to be evicted with new data
	cache[sBits][to_be_evicted].tag = tag;
        cache[sBits][to_be_evicted].valid = 1;
	cache[sBits][to_be_evicted].hit_count = highestCounter + 1;
    }
    return;
}
  
  
/* 
 * replay_trace:
 * Replays the given trace file against the cache.
 *
 * Reads the input trace file line by line.
 * Extracts the type of each memory access : L/S/M
 * TRANSLATE each "L" as a load i.e. 1 memory access
 * TRANSLATE each "S" as a store i.e. 1 memory access
 * TRANSLATE each "M" as a load followed by a store i.e. 2 memory accesses 
 */                    
void replay_trace(char* trace_fn) {           
    char buf[1000];  
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r"); 

    if (!trace_fp) { 
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);   
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if (verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

            // GIVEN: 1. addr has the address to be accessed
            //        2. buf[1] has type of acccess(S/L/M)
            // call access_data function here depending on type of access
	    
	    //calls access_data once as the instruction would
	    if (buf[1] == 'L' || buf[1] == 'S') {
		access_data(addr);
	    }
	    //call access_data twice as the instruction would
	    if (buf[1] == 'M') {
                access_data(addr);
		access_data(addr);
            }

            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}  
  
  
/*
 * print_usage:
 * Print information on how to use csim to standard output.
 */                    
void print_usage(char* argv[]) {                 
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of s bits for set index.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of b bits for block offsets.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}  
  
  
/*
 * print_summary:
 * Prints a summary of the cache simulation statistics to a file.
 */                    
void print_summary(int hits, int misses, int evictions) {                
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}  
  
  
/*
 * main:
 * Main parses command line args, makes the cache, replays the memory accesses
 * free the cache and print the summary statistics.  
 */                    
int main(int argc, char* argv[]) {                      
    char* trace_file = NULL;
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                print_usage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    //Make sure that all required command line args were specified.
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        print_usage(argv);
        exit(1);
    }
    //if s + e + b is larger than 64 bits our cache sim won't work
    if ( (s + E + b) > 64) {
        printf("arguments not valid (s + E + b) > 64)\n");
        exit(1);
    }

    //Initialize cache.
    init_cache();

    //Replay the memory access trace.
    replay_trace(trace_file);

    //Free memory allocated for cache.
    free_cache();

    //Print the statistics to a file.
    //DO NOT REMOVE: This function must be called for test_csim to work.
    print_summary(hit_cnt, miss_cnt, evict_cnt);
    return 0;   
}  
