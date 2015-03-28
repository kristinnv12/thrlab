#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

    sem_t chair; // Semphore for waiting chairs
    sem_t mutex;
    sem_t barber; // Semphore for the barber

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


void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

void my_error_msg(char *function, char *msg)
{
    fprintf(stdout, "%s - %s: %s\n", function, msg, strerror(errno));
    exit(EXIT_FAILURE);
}

void my_sem_post(sem_t *semi, char *errormsg)
{
    if (sem_post(semi) == -1)
    {
        my_error_msg("sem_post", errormsg);
    }
}

void my_sem_wait(sem_t *semi, char *errormsg)
{
    if (sem_wait(semi) == -1)
    {
        my_error_msg("semwait", errormsg);
    }
}

void my_sem_init(sem_t *semi, int pshared, unsigned int value, char *errormsg)
{
    if (sem_init(semi, pshared, value) == -1)
    {
        my_error_msg("sem_init", errormsg);
    }
}

void my_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg, char *errormsg)
{
    if (pthread_create(thread, attr, start_routine, arg) != 0)
    {
        unix_error(errormsg);
    }

}

void my_pthread_detach(pthread_t thread, char *errormsg)
{
    if (pthread_detach(thread) != 0)
    {
        unix_error(errormsg);
    }

}

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer = 0; /* TODO: Fetch a customer from a chair */

    /* Main barber loop */
    while (true)
    {
        /* TODO: Here you must add you semaphores and locking logic */
        my_sem_wait(&chairs->barber, "chairs->barber");

        // Use this to lock everything so no one will edit shared variables
        my_sem_wait(&chairs->mutex, "chairs->mutex");

        //sp->buf[(++sp->rear)%(sp->n)] = item; /* Insert the item */
        customer = chairs->customer[(++chairs->front) % chairs->max]; // Get the front customer
        thrlab_prepare_customer(customer, barber->room);

        // We have edited everything and now threads can run
        my_sem_post(&chairs->mutex, "chairs->mutex");

        my_sem_post(&chairs->chair, "chairs->chair");

        thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
        thrlab_dismiss_customer(customer, barber->room);

        my_sem_post(&customer->mutex, "customer->mutex");

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
    chairs->front = chairs->rear = 0; // The ring buffer setup

    chairs->max = thrlab_get_num_chairs();


    my_sem_init(&chairs->mutex, 0, 1, "chairs->mutex");

    // Here is our initial available chairs, that is we allow chairs->max to be the maximum allowed
    // customers to wait. Beyond that the customers shall be rejected
    my_sem_init(&chairs->chair, 0, chairs->max, "chairs->chair");

    my_sem_init(&chairs->barber, 0, 0, "chairs->barber"); // Barber semaphore

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

        my_pthread_create(&simulator->barberThread[i], 0, barber_work, barber, "Failed to create barberThread");

        my_pthread_detach(simulator->barberThread[i], "Failed detaching barberThread");

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

    my_sem_init(&customer->mutex, 0, 0, "customer->mutex");

    /* This below is old solution
    // Here we check if we can follow the customer to the chair
    sem_wait(&chairs->mutex); // Lock the thread so our int value is not change by others
    int value;
    sem_getvalue(&chairs->chair, &value); // Get the available chairs
    //printf("Available chairs are: %d\n", value);

    if (value != 0) // If 1 or more chairs are available we can allow customers to sit down
    */

    // We try to take one seat and the sem_trywait should return 0 if
    // success else -1
    if (sem_trywait(&chairs->chair) == 0)
    {
        /* Part of an old solution
        // Thread will take a chair and everyone else have to wait
        //sem_wait(&chairs->chair); // Now we take 1 seat.

        //sem_post(&chairs->mutex); // Unlock so we can allow others to sit if chairs are available
        */

        // The customer waits here for the semophore to allow him to continue
        my_sem_wait(&chairs->mutex, "chairs->mutex");

        // Accept the new customer
        thrlab_accept_customer(customer);

        // Put him in the chair. Since the threads share the same variable
        // we must take care of it so they dont read/write at the same time
        //printf("Waiting chairs: %d\n", (chairs->rear ) % (chairs->max));
        //sp->buf[(++sp->rear)%(sp->n)] = item; /* Insert the item */
        chairs->customer[(++chairs->rear) % chairs->max] = customer; // Put our arravied customer to his seat

        my_sem_post(&chairs->mutex, "chairs->mutex");
        my_sem_post(&chairs->barber, "chairs->barber"); // Allow the barber to cut if he wants

        my_sem_wait(&customer->mutex, "customer->mutex");// No the customer waits for the cut

    }
    else
    {
        // Part of an old solution
        //sem_post(&chairs->mutex); // Unlock the "value"

        thrlab_reject_customer(customer); // Reject the customer because there is no seat available
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
