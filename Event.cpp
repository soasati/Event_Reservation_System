// Question-3 --  Multi-threaded Event-reservation system.
//Group No.: 14


//including the header files
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

//defining the Macros according to the question given
#define MAX_EVENTS 100
#define CAPACITY 500
#define BOOK_MIN 5
#define BOOK_MAX 10
#define MAX_THREADS 20
#define MAX_ACTIVE_QUERIES 5
#define MAX_TIME 1 // mins

pthread_mutex_t event_mutex[MAX_EVENTS]; // one mutex for each event
pthread_mutex_t query_mutex;             // mutex for accessing the shared query table
pthread_cond_t query_cond;               // condition variable for waiting on active queries

struct query {
    int event_num;      // number of the event being queried or modified
    int type;           // type of the query (1=inquire, 2=book, 3=cancel)
    int thread_num;     // number of the thread making the query
};

struct query active_queries[MAX_ACTIVE_QUERIES];    // shared query table
int num_active_queries = 0;                         // number of active queries

int available_seats[MAX_EVENTS];    // number of available seats for each event
pthread_t threads[MAX_THREADS];     // worker threads
int num_threads = 0;                // number of worker threads
pthread_barrier_t barrier;          // barrier for synchronizing thread termination

//function to generate a random event number to perform query on 
int get_random_event() {
    return rand() % MAX_EVENTS;
}

//Function to generate the number of seats to be booked randomly
int get_random_bookings() {
    return rand() % (BOOK_MAX - BOOK_MIN + 1) + BOOK_MIN;
}

//Function to get any random thread that will perform the query
int get_random_thread() {
    return rand() % num_threads;
}

//Function to print the query out of 3 (Inquiry, Book, Cancel)
void print_query(struct query q) {
    printf("Thread %d: ", q.thread_num);
    if(q.type == 1){
        printf("Inquiring %d\n", q.event_num);
    }
    else if(q.type == 2){
        printf("Booking %d seats in %d\n", q.thread_num, q.event_num);
    }
    else if(q.type == 3){
        printf("Cancelling a booking in %d\n", q.event_num);
    }
    else{
        printf("Invalid query type\n");
    }
}

//Function creates worker threads
void* worker_thread(void* arg) {
    int thread_num = *(int*)arg;
    int bookings[MAX_EVENTS];       // private list of bookings made by this thread
    int num_bookings = 0;           // number of bookings made by this thread
    srand(time(NULL) + thread_num); // seed the random number generator
    clock_t start, end;
    start = clock();
    while (true) {
        // wait for a random short interval between queries
        usleep(rand() % 10000);
        // generate a random query
        struct query q;
        q.thread_num = thread_num;
        int event_num = get_random_event();
        q.event_num = event_num;
        int type = rand() % 3 + 1;
        q.type = type;
        // print the query for diagnostic purposes
        print_query(q);
        // wait if there are already MAX active queries
        pthread_mutex_lock(&query_mutex);
        while (num_active_queries >= MAX_ACTIVE_QUERIES) {
            pthread_cond_wait(&query_cond, &query_mutex);
        }
        // check if this query conflicts with any active queries
        bool conflict = false;
        for (int i = 0; i < num_active_queries; i++) {
            struct query active_query = active_queries[i];
            if (active_query.event_num == event_num) {
                if (active_query.type == 2 || type == 2) {
                    conflict = true; // write-write or read-write conflict
                }
            }
        }
        // if there is a conflict, wait on the query condition
        if (conflict) {
            pthread_cond_wait(&query_cond, &query_mutex);
        }
        // add this query to the active query table
        active_queries[num_active_queries++] = q;
        // release the query mutex
        pthread_mutex_unlock(&query_mutex);
        // perform the query
        switch (type) {
            case 1: // inquire
                printf("Thread %d: %d seats available in %d\n", thread_num, available_seats[event_num], event_num);
                break;
            case 2: // book
                if (available_seats[event_num] >= BOOK_MIN) {
                    int num_booked = get_random_bookings();
                    if (num_booked > available_seats[event_num]) {
                        num_booked = available_seats[event_num];
                    }
                    printf("Thread %d: booking %d seats in %d\n", thread_num, num_booked, event_num);
                    //managing  the seats
                    bookings[num_bookings++] = num_booked;
                    available_seats[event_num] -= num_booked;
                } 
                else {
                    printf("Thread %d: %d has no available seats\n", thread_num, event_num);
                }
                break;
            case 3: // cancel
                if (num_bookings > 0) {
                    //generating a random number between 0 to num_bookings for cancellation
                    int index = rand() % num_bookings;
                    int num_cancelled = bookings[index];
                    printf("Thread %d: cancelling %d seats in %d\n", thread_num, num_cancelled, event_num);
                    //managing  the seats
                    bookings[index] = bookings[--num_bookings];
                    available_seats[event_num] += num_cancelled;
                } 
                else {
                    printf("Thread %d: no bookings to cancel in %d\n", thread_num, event_num);
                }
                break;
            default:
                printf("Thread %d: Invalid query type\n", thread_num);
        }
        // remove this query from the active query table
        pthread_mutex_lock(&query_mutex);
        for (int i = 0; i < num_active_queries; i++) {
            if (active_queries[i].thread_num == thread_num) {
                active_queries[i] = active_queries[--num_active_queries];
                break;
            }
        }
        // signal the query condition variable
        pthread_cond_broadcast(&query_cond);
        // release the query mutex
        pthread_mutex_unlock(&query_mutex);
        //check the current time 
        end = clock();
        //add a constant value to reduce overall operations by manually incrementing the execution time
        end +=1000;
        //calculate if the time elapsed has reached the pre-defined time limit
        double now = ((double)end - start)/CLOCKS_PER_SEC;
        // check if the time limit has been reached        
        if (now >= MAX_TIME) {
            break;
        }
    }
    // wait for all threads to finish before terminating
    pthread_barrier_wait(&barrier);
    return NULL;
}

int main() {
    // initialize the event mutexes
    for (int i = 0; i < MAX_EVENTS; i++) {
        pthread_mutex_init(&event_mutex[i], NULL);
        available_seats[i] = CAPACITY;
    }
    // initialize the query mutex and condition variable
    pthread_mutex_init(&query_mutex, NULL);
    pthread_cond_init(&query_cond, NULL);
    // create the worker threads
    for (int i = 0; i < MAX_THREADS; i++) {
        int* thread_num = (int*)malloc(sizeof(int));
        *thread_num = i;
        pthread_create(&threads[i], NULL, worker_thread, thread_num);
        num_threads++;
    }
    // wait for all threads to finish before terminating
    pthread_barrier_init(&barrier, NULL, num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}
