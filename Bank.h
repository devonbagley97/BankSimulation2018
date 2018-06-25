/*
 * Bank.h
 *
 *  Created on: Nov 2, 2017
 *      Author: exm8563
 */

#ifndef BANK_H_
#define BANK_H_
#include <pthread.h>
/******** Simulation parameters ********/
//	Timing definitions
#define SIM_TICK (1)	// Specifies base tick so it can be scaled if need be
#define SIM_TICK_DURATION (50000000) // Real-time duration of simulation clock tick, in ns
#define SIM_TICK_PER_SEC (1000000000 / SIM_TICK_DURATION) // Number of ticks in a realtime second
#define SEC_30 (1*SIM_TICK)
#define MINUTE (2*SIM_TICK)
// Bank-specific definitions
#define MIN_CUSTOMER_INTERVAL (1*MINUTE)
#define MAX_CUSTOMER_INTERVAL (4*MINUTE)
#define MIN_TRANSACTION_TIME  (SEC_30)
#define MAX_TRANSACTION_TIME (8*MINUTE)
#define BANK_OPENING_TIME (9*60*MINUTE)    // Open at 9:00 (AM)
#define BANK_CLOSING_TIME (16*60*MINUTE)	// close at 16:00 (4PM)
#define BANK_OPEN_DURATION (BANK_CLOSING_TIME - BANK_OPENING_TIME)

#define NUM_TELLERS (3)
#define MAX_CUSTOMERS (10)

/******** Struct Definitions ********/
// Basic Customer struct
typedef struct customer_t{
	int queue_entry_time;
	int transaction_start_time;
} customer;

// queue of customers and associated functions for queue operations
typedef struct queue_t{
	struct customer_t customers[MAX_CUSTOMERS];
	int depth;
	int front;
	int rear;
	pthread_mutex_t mutex;
	sem_t empty;
	sem_t full;
} queue;

// the attributes of the tellers
typedef struct teller_data_t {
	int teller_id;
	int wait_start;
	struct queue_t* queue_ptr;
	struct metrics_t* metrics_ptr;
} teller_data;

// all the collected metrics
typedef struct metrics_t{
	int total_customers;
	int total_teller_wait;
	int total_customer_wait;
	int total_transaction_time;
	int maximum_teller_wait;
	int maximum_customer_wait;
	int maximum_depth;
	int maximum_transaction_time;
	int average_customer_wait;
	int average_teller_wait;
	int average_transaction_time;
	pthread_mutex_t mutex;
} metrics;

// Timing related prototypes
int get_sim_time(void);
void sim_wait_duration(int duration);

// Queue Function Prototypes
bool isFull(struct queue_t* line);
void enter_queue(struct queue_t* line, struct customer_t cur_customer);
struct customer_t* leave_queue(struct queue_t* line);

// Metrics function prototypes
void calc_average_customer_wait(struct metrics_t* metrics);
void calc_average_teller_wait(struct metrics_t* metrics);
void calc_average_transaction_time(struct metrics_t* metrics);
void update_total_customers(struct metrics_t* metrics);
void update_teller_wait(struct metrics_t* metrics, int teller_wait);
void update_customer_wait(struct metrics_t* metrics, int customer_wait);
void update_teller_wait(struct metrics_t* metrics, int teller_wait);
void update_customer_wait(struct metrics_t* metrics, int customer_wait);
void update_transaction_time(struct metrics_t* metrics, int transaction_time);
void update_maximum_depth(struct metrics_t* metrics, int cur_depth);

#endif /* BANK_H_ */
