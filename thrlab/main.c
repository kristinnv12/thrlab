#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * User 1: kristinnv12
 * SSN: 0208923829
 * User 2: ragnarp12
 * SSN: 2801872169
 * === End User Information ===
 ********************************************************/

struct chairs
{
    struct customer **customer; /* Array of customers */
    int max;


    /* TODO: Add more variables related to threads */
    /* Hint: Think of the consumer producer thread problem */

    sem_t chair;
    sem_t mutex;
    sem_t barber;

    // Ring buffer - F17 page 8
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
};

struct barber
{
    int room;
    struct simulator *simulator;
};

struct simulator
{
    struct chairs chairs;

    pthread_t *barberThread;
    struct barber **barber;
};

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer = 0; /* TODO: Fetch a customer from a chair */

    /* Main barber loop */
    while (true)
    {
        /* TODO: Here you must add you semaphores and locking logic */
        sem_wait(&chairs->barber);

        // Use this to lock everything so no one will edit shared variables
        sem_wait(&chairs->mutex);

        // Rais the chair count to get the next customer
        chairs->front++;
        customer = chairs->customer[chairs->front % chairs->max]; /* TODO: You must choose the customer */
        thrlab_prepare_customer(customer, barber->room);

        // We have edited everything and now threads can run
        sem_post(&chairs->mutex);
        sem_post(&chairs->chair);

        thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
        thrlab_dismiss_customer(customer, barber->room);

        sem_post(&customer->mutex);
    }
    return NULL;
}

/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
    struct chairs *chairs = &simulator->chairs;
    /* Setup semaphores*/
    chairs->front = chairs->rear = 0;

    chairs->max = thrlab_get_num_chairs();


    sem_init(&chairs->mutex, 0, 1);
    // Here is our initial available chairs, that is we allow chairs->max to be the maximum allowed
    // customers to wait. Beyond that the customers shall be rejected
    sem_init(&chairs->chair, 0, chairs->max);
    sem_init(&chairs->barber, 0, 0);

    /* Create chairs*/
    chairs->customer = malloc(sizeof(struct customer *) * thrlab_get_num_chairs());

    /* Create barber thread data */
    simulator->barberThread = malloc(sizeof(pthread_t) * thrlab_get_num_barbers());
    simulator->barber = malloc(sizeof(struct barber*) * thrlab_get_num_barbers());

    /* Start barber threads */
    struct barber *barber;
    for (unsigned int i = 0; i < thrlab_get_num_barbers(); i++)
    {
        barber = calloc(sizeof(struct barber), 1);
        barber->room = i;
        barber->simulator = simulator;
        simulator->barber[i] = barber;
        pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
        pthread_detach(simulator->barberThread[i]);
    }
}

/**
 * Free all used resources and end the barber threads.
 */
static void cleanup(struct simulator *simulator)
{
    /* Free chairs */
    free(simulator->chairs.customer);

    /* Free barber thread data */
    free(simulator->barber);
    free(simulator->barberThread);
}

/**
 * Called in a new thread each time a customer has arrived.
 */
static void customer_arrived(struct customer *customer, void *arg)
{
    struct simulator *simulator = arg;
    struct chairs *chairs = &simulator->chairs;

    sem_init(&customer->mutex, 0, 0);

    /* TODO: Accept if there is an available chair */


    // Here we check if we can follow the customer to the chair
    sem_wait(&chairs->mutex);
    int value;
    sem_getvalue(&chairs->chair, &value);
    //printf("Available chairs are: %d\n", value);
    if (value != 0)
    {
        sem_post(&chairs->mutex);

        // Thread will take a chair and everyone else have to wait

        sem_wait(&chairs->chair);

        // The customer waits here for the semophore to allow him to continue
        sem_wait(&chairs->mutex);

        // Accept the new customer
        thrlab_accept_customer(customer);

        // Put him in the chair. Since the threads share the same variable
        // we must take care of it so they dont read/write at the same time
        chairs->rear++;
        //printf("Waiting chairs: %d\n", (chairs->rear ) % (chairs->max));
        chairs->customer[chairs->rear % chairs->max] = customer;

        sem_post(&chairs->mutex);
        sem_post(&chairs->barber);

        sem_wait(&customer->mutex);
    }
    else
    {
        sem_post(&chairs->mutex);
        /* TODO: Reject if there are no available chairs */
        thrlab_reject_customer(customer);
    }
}

int main (int argc, char **argv)
{
    struct simulator simulator;

    thrlab_setup(&argc, &argv);
    setup(&simulator);

    thrlab_wait_for_customers(customer_arrived, &simulator);

    thrlab_cleanup();
    cleanup(&simulator);

    return EXIT_SUCCESS;
}
