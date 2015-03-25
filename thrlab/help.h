#ifndef _THRLAB_HELP_H_
#define _THRLAB_HELP_H_

#include <pthread.h>
#include <semaphore.h>

/******************************************************************************
 * Initialization & Cleanup
 *****************************************************************************/

/**
 * Initialize the thrlab environment.
 *
 * Must be called before any other thrlab function.
 */
void thrlab_setup (int *argc, char ***argv);

/**
 * Clean up resources used by the thrlab environment.
 *
 * No thrlab function may be called after a cleanup.
 */
void thrlab_cleanup ();

/******************************************************************************
 * Barbershop Information
 *****************************************************************************/

/**
 * Get the number of barbers on duty.
 */
unsigned int thrlab_get_num_barbers ();

/**
 * Get the number of waiting chairs in the building.
 */
unsigned int thrlab_get_num_chairs ();

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

/**
 * Pause the current thread for at least `ms` milliseconds.
 */
void thrlab_sleep (int ms);

/******************************************************************************
 * Customer Management
 *****************************************************************************/

/**
 * Information about a customer.
 */
struct customer
{
	/* the customer's thread */
	pthread_t thread;

	/* name of the customer */
	const char *name;

	/* a unique customer identifier */
	unsigned int id;

	/* length of the customer's hair in millimetres */
	unsigned int hair_length;

	/* desired hair length in millimetres */
	unsigned int hair_goal;

	/* A mutex that you are free to use */
	sem_t mutex;
};

/**
 * Blocks the current thread, calling `callback` in a new thread each time a
 * new customer arrives, until it's time to close down the shop.
 */
void thrlab_wait_for_customers
	( void (*callback) (struct customer *, void *)
	, void *ud
	);

/**
 * Accept the customer and place him in the waiting room.
 */
void thrlab_accept_customer (struct customer *customer);

/**
 * Reject the customer; the waiting room's full.
 */
void thrlab_reject_customer (struct customer *customer);

/**
 * Prepare the customer for a nice haircut in room `room`.
 */
void thrlab_prepare_customer (struct customer *customer, unsigned int room);

/**
 * Dismiss the customer from room `room`; their hair has been cut.
 */
void thrlab_dismiss_customer (struct customer *customer, unsigned int room);

#endif
