#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <sys/neutrino.h>

#include "Bank.h"


// ********* queue functions **********
bool isFull(struct queue_t* line){
	return line->depth == MAX_CUSTOMERS;
};

void enter_queue(struct queue_t* line, struct customer_t cur_customer){
	sem_wait(&line->empty);
	pthread_mutex_lock(&(line->mutex));
	if(!isFull(line)){
		if(line->rear == MAX_CUSTOMERS){
			line->rear = 0;
		}
		line->customers[line->rear++] = cur_customer;
		line->depth++;
	}
	pthread_mutex_unlock(&(line->mutex));
	sem_post(&line->full);
};

struct customer_t* leave_queue(struct queue_t* line){
	sem_wait(&line->full);
	pthread_mutex_lock(&(line->mutex));
	struct customer_t* next_customer = &(line->customers[line->front++]);

	// if the index is the end of our memory, wrap around
	if(line->front == MAX_CUSTOMERS){
		line->front = 0;
	}
	line->depth--;
	pthread_mutex_unlock(&(line->mutex));
	sem_post(&line->empty);

	return next_customer;
};

// ********************** End of queue functions *************

// *********************** Metric functions ******************

void calc_average_customer_wait(struct metrics_t* metrics){
	int average = (metrics->total_customer_wait / metrics->total_customers);
	metrics->average_customer_wait = average;
}

void calc_average_teller_wait(struct metrics_t* metrics){
	int average = (metrics->total_teller_wait / metrics->total_customers);
	metrics->average_teller_wait = average;
}

void calc_average_transaction_time(struct metrics_t* metrics){
	int average = (metrics->total_transaction_time / metrics->total_customers);
	metrics->average_transaction_time = average;
}

void update_total_customers(struct metrics_t* metrics){
	metrics->total_customers++;
}

void update_teller_wait(struct metrics_t* metrics, int teller_wait){
	metrics->total_teller_wait += teller_wait;
	if(metrics->maximum_teller_wait <= teller_wait){
		metrics->maximum_teller_wait = teller_wait;
	}
}

void update_customer_wait(struct metrics_t* metrics, int customer_wait){
	metrics->total_customer_wait += customer_wait;
	if(metrics->maximum_customer_wait <= customer_wait){
		metrics->maximum_customer_wait = customer_wait;
	}
}

void update_transaction_time(struct metrics_t* metrics, int transaction_time){
	metrics->total_transaction_time += transaction_time;
	if(metrics->maximum_transaction_time <= transaction_time){
		metrics->maximum_transaction_time = transaction_time;
	}
}

void update_maximum_depth(struct metrics_t* metrics, int cur_depth){
	if(metrics->maximum_depth <= cur_depth){
		metrics->maximum_depth = cur_depth;
	}
}

// ********************* End of Metrics ***********

// ********************* Teller function **********

void *teller_transaction(void *arg){
	teller_data *teller_log = (teller_data *)arg;
	customer* cur_customer;
	int transaction_clock;

	do {
		teller_log->wait_start = get_sim_time();
		//grab a customer when ready
		cur_customer = leave_queue(teller_log->queue_ptr);
		cur_customer->transaction_start_time = get_sim_time();
		//printf("Teller %d now serving #%d\n",teller_log->teller_id,teller_log->metrics_ptr->total_customers);
		transaction_clock = (random() % (MAX_TRANSACTION_TIME - MIN_TRANSACTION_TIME)) + MIN_TRANSACTION_TIME;

		//update metrics with mutex
		pthread_mutex_lock(&teller_log->metrics_ptr->mutex);
		update_total_customers(teller_log->metrics_ptr);
		update_customer_wait(teller_log->metrics_ptr, cur_customer->transaction_start_time - cur_customer->queue_entry_time);
		pthread_mutex_unlock(&teller_log->metrics_ptr->mutex);

		//Simulate the transaction happening
		sim_wait_duration(transaction_clock);

		//update metrics after transaction
		pthread_mutex_lock(&teller_log->metrics_ptr->mutex);
		update_teller_wait(teller_log->metrics_ptr, cur_customer->transaction_start_time - teller_log->wait_start);
		update_transaction_time(teller_log->metrics_ptr, transaction_clock);
		pthread_mutex_unlock(&teller_log->metrics_ptr->mutex);
	} while (get_sim_time() < BANK_CLOSING_TIME || teller_log->queue_ptr->depth > 0);

	return NULL; // get Momentix to stop complaining
}

// ********** Timing functions **********
// These operate in terms of system ticks; it is assumed that
// the appropriate macros are used to pass in times

void sim_wait_duration(int duration){
	/* non-blocking sleep, loops until duration elapses on realtime clock
	 *
	 * duration - time to wait, in system ticks
	*/
	int targettime;

	targettime = get_sim_time() + duration;
	while (get_sim_time() < targettime);
}


int get_sim_time(void){
	/* returns the system time, normalized to the ticks of the simulation clock  */
	struct timespec systime;
	clock_gettime(CLOCK_REALTIME, &systime);

	return (systime.tv_nsec / SIM_TICK_DURATION) + ((int)systime.tv_sec * SIM_TICK_PER_SEC);
}

void sim_time_tostring(int time, char* str){
	sprintf(str, "%02d:%02d:%02d", time / (60*MINUTE), (time / (MINUTE)) % (60),30*(time % MINUTE));
}

// *********** main program ************
int main(int argc, char *argv[]) {
	int i;
	int customer_entry_clock = 0;
	struct timespec opening_time;
	char time_string[10];

	//struct timespec init_time, bank_time;

	static struct queue_t queue;
	queue.depth = 0;
	queue.front = 0;
	queue.rear = 0;
	pthread_mutex_init(&queue.mutex, NULL);
	sem_init(&queue.full,0,0);
	sem_init(&queue.empty,0,MAX_CUSTOMERS);


	static struct metrics_t metrics;
	metrics.average_customer_wait = 0;
	metrics.average_teller_wait = 0;
	metrics.average_transaction_time = 0;
	metrics.maximum_customer_wait = 0;
	metrics.maximum_depth = 0;
	metrics.maximum_teller_wait = 0;
	metrics.maximum_transaction_time = 0;
	metrics.total_customer_wait = 0;
	metrics.total_teller_wait = 0;
	metrics.total_transaction_time = 0;
	metrics.total_customers = 0;
	pthread_mutex_init(&metrics.mutex, NULL);

	// customers in the queue will actually be clones of this one
	struct customer_t new_customer;



	// initialize teller attributes
	struct teller_data_t teller_logs[NUM_TELLERS]; // our thread's attributes
	for(i=0;i<NUM_TELLERS;i++){
		teller_logs[i].metrics_ptr = &metrics;
		teller_logs[i].queue_ptr = &queue;
		teller_logs[i].teller_id = i;
		teller_logs[i].wait_start = 0;
	}

	// System clock is set to the bank's opening time
	opening_time.tv_sec = (BANK_OPENING_TIME / SIM_TICK_PER_SEC);
	opening_time.tv_nsec = (long int)((BANK_OPENING_TIME % SIM_TICK_PER_SEC) * SIM_TICK_DURATION);
	clock_settime(CLOCK_REALTIME, &opening_time);

	// Start the teller threads
	pthread_t tellers[NUM_TELLERS]; // our threads
	for(i=0;i<NUM_TELLERS;i++){
		pthread_create(&tellers[i], NULL, teller_transaction, &teller_logs[i]);
	}

	while (get_sim_time() < BANK_CLOSING_TIME ){
		// generate a new random customer interval
		customer_entry_clock = (random() % (MAX_CUSTOMER_INTERVAL - MIN_CUSTOMER_INTERVAL)) + MIN_CUSTOMER_INTERVAL;
		new_customer.queue_entry_time = get_sim_time();

		// put somebody in the queue (passing customer by value, on purpose!)
		enter_queue(&queue, new_customer);
		pthread_mutex_lock(&metrics.mutex);
		update_maximum_depth(&metrics,queue.depth);
		pthread_mutex_unlock(&metrics.mutex);

		// wait until customer interval expires
		sim_wait_duration(customer_entry_clock);

		sim_time_tostring(get_sim_time(), time_string);
		//printf("Arrival at %s, %d in queue\n",time_string,queue.depth);
	}
	// metrics wait to print until all customers exit
	while(queue.depth > 0);
	// end of day, print metrics
	pthread_mutex_lock(&metrics.mutex);
	calc_average_customer_wait(&metrics);
	calc_average_teller_wait(&metrics);
	calc_average_transaction_time(&metrics);
	sim_time_tostring(BANK_CLOSING_TIME, time_string);
	printf( "\nBank closed at %s\n\n"
			"Total customers:              %d\n",
			time_string, metrics.total_customers);
	sim_time_tostring(metrics.average_customer_wait, time_string);
	printf(	"Average customer wait time:   %s\n", time_string);
	sim_time_tostring(metrics.average_transaction_time, time_string);
	printf(	"Average transaction time:     %s\n", time_string);
	sim_time_tostring(metrics.average_teller_wait, time_string);
	printf( "Average teller wait time:     %s\n\n", time_string);
	sim_time_tostring(metrics.maximum_customer_wait, time_string);
	printf( "Maximum customer wait time:   %s\n", time_string);
	sim_time_tostring(metrics.maximum_teller_wait, time_string);
	printf( "Maximum teller wait time:     %s\n", time_string);
	sim_time_tostring(metrics.maximum_transaction_time, time_string);
	printf( "Maximum transaction time:     %s\n", time_string);
	printf( "Maximum queue depth:          %d\n",metrics.maximum_depth);
	pthread_mutex_unlock(&metrics.mutex);

	//tellers will exit after closing time && all customers are gone
	for (i=0;i<NUM_TELLERS;i++){
		// pretend that the queue is non-empty so the tellers can wake up and go home
		sem_post(&queue.full);
		pthread_join(tellers[i],0);
	}

	return EXIT_SUCCESS;
}
