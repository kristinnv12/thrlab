#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOUCE 200112L
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "help.h"

#define ARRSIZE(x) (sizeof (x) / sizeof (*(x)))

enum customer_status
{
	CUSTOMER_PENDING,
	CUSTOMER_WAITING,
	CUSTOMER_CUTTING,
	CUSTOMER_DONE,
	CUSTOMER_REJECTED
};

struct arguments
{
	size_t barbers;
	size_t chairs;
	size_t customers;
	size_t rate;
};

static struct {
	pthread_mutex_t mtx;
	struct timespec start;

	/* constants */
	size_t visitors;
	size_t barbers;
	size_t chairs;
	size_t rate;

	/* current statistics */
	size_t num_cutting;
	size_t num_waiting;
	size_t num_pending;

	/* complaints */
	size_t complaint_reject_avail; /* rejected with seats available */
	size_t complaint_reject_wait; /* a waiting customer was shown the door */
	size_t complaint_reject_cut; /* customer rejected mid-cut */
	size_t complaint_reject_done; /* rejected entry after a haircut */
	size_t complaint_reject_again; /* rejected more than once */
	size_t complaint_accept_full; /* accepted while seats were unavailable */
	size_t complaint_accept_wait; /* accepted while waiting */
	size_t complaint_accept_cut; /* accepted while being cut */
	size_t complaint_accept_done; /* accepted when done */
	size_t complaint_accept_reject; /* accepted after rejection */
	size_t complaint_prepare_pending; /* prepared without being let in first */
	size_t complaint_prepare_busy; /* prepared while barber's busy */
	size_t complaint_prepare_again; /* prepared more than once */
	size_t complaint_prepare_done; /* prepared after leaving */
	size_t complaint_prepare_reject; /* prepared after being rejected */
	size_t complaint_prepare_self; /* customer had to cut their own hair */
	size_t complaint_dismiss_pending; /* dismissed outside */
	size_t complaint_dismiss_wait; /* dismissed while waiting */
	size_t complaint_dismiss_done; /* dismissed again */
	size_t complaint_dismiss_reject; /* dismissed after rejection */
	size_t complaint_dismiss_room; /* told to dismiss, but wrong room */
	size_t complaint_dismiss_self; /* told to show themselves to the door */
	size_t complaint_dismiss_early; /* customer thread died before dismissal */
	size_t complaint_cut_fast; /* barber in a hurry, too fast */
	size_t complaint_cut_slow; /* barber too slow */

	size_t customer_count;
	struct customer **customers;
	enum customer_status *statuses;
	struct timespec *times; /* time when prepared */

	struct customer **occupancy; /* room occupancy */
} *thrlab = NULL;

static const char *barber_names[] =
	{ "HAL9000"
	, "Terminator"
	, "Wall-E"
	, "Johnny-5"
	, "Dot-Matrix"
	, "C-3PO"
	, "R2D2"
	, "Optimus-Prime"
	, "Bishop"
	, "Call"
	, "Robot"
	, "Robocop"
	, "Andrew"
	, "Bender"
	, "T-1000"
	, "GERTY-3000"
	, "Data"
	, "Sonny"
	, "Astro-Boy"
	, "Baymax"
	, "TARS"
	, "CASE"
	, "Cylon"
	, "C.H.E.E.S.E."
	, "Funnybot"
	, "GIR"
	, "D.A.V.E."
	, "Six"
	, "Replicator"
	, "MegaMan"
	, "ED209"
	, "B.E.N."
	, "SID-6.7"
	, "21-B"
	, "Fembot"
	, "Sentinel"
	, "Gigalo-Joe"
	, "DroidEkas"
	, "D.A.R.Y.L."
	, "Pris"
	, "Omnidroid"
	, "Teddy"
	, "EVE"
	, "Mechagodzilla"
	, "Steprod-Wive"
	, "Marvin"
	, "Ash"
	, "Gort"
	, "Roy-Batty"
	};

static const char *customer_names[] =
	{ "Abigail"
	, "Adam"
	, "Adrian"
	, "Alan"
	, "Alexander"
	, "Alexandra"
	, "Alison"
	, "Amanda"
	, "Amelia"
	, "Amy"
	, "Andrea"
	, "Andrew"
	, "Angela"
	, "Anna"
	, "Anne"
	, "Anthony"
	, "Audrey"
	, "Austin"
	, "Ava"
	, "Bella"
	, "Benjamin"
	, "Bernadette"
	, "Blake"
	, "Boris"
	, "Brandon"
	, "Brian"
	, "Cameron"
	, "Carl"
	, "Carol"
	, "Caroline"
	, "Carolyn"
	, "Charles"
	, "Chloe"
	, "Christian"
	, "Christopher"
	, "Claire"
	, "Colin"
	, "Connor"
	, "Dan"
	, "David"
	, "Deirdre"
	, "Diana"
	, "Diane"
	, "Dominic"
	, "Donna"
	, "Dorothy"
	, "Dylan"
	, "Edward"
	, "Elizabeth"
	, "Ella"
	, "Emily"
	, "Emma"
	, "Eric"
	, "Evan"
	, "Faith"
	, "Felicity"
	, "Fiona"
	, "Frank"
	, "Gabrielle"
	, "Gavin"
	, "Gordon"
	, "Grace"
	, "Hannah"
	, "Harry"
	, "Heather"
	, "Ian"
	, "Irene"
	, "Isaac"
	, "Jack"
	, "Jacob"
	, "Jake"
	, "James"
	, "Jan"
	, "Jane"
	, "Jasmine"
	, "Jason"
	, "Jennifer"
	, "Jessica"
	, "Joan"
	, "Joanne"
	, "Joe"
	, "John"
	, "Jonathan"
	, "Joseph"
	, "Joshua"
	, "Julia"
	, "Julian"
	, "Justin"
	, "Karen"
	, "Katherine"
	, "Keith"
	, "Kevin"
	, "Kimberly"
	, "Kylie"
	, "Lauren"
	, "Leah"
	, "Leonard"
	, "Liam"
	, "Lillian"
	, "Lily"
	, "Lisa"
	, "Lucas"
	, "Luke"
	, "Madeleine"
	, "Maria"
	, "Mary"
	, "Matt"
	, "Max"
	, "Megan"
	, "Melanie"
	, "Michael"
	, "Michelle"
	, "Molly"
	, "Natalie"
	, "Nathan"
	, "Neil"
	, "Nicholas"
	, "Nicola"
	, "Oliver"
	, "Olivia"
	, "Owen"
	, "Paul"
	, "Penelope"
	, "Peter"
	, "Phil"
	, "Piers"
	, "Pippa"
	, "Rachel"
	, "Rebecca"
	, "Richard"
	, "Robert"
	, "Rose"
	, "Ruth"
	, "Ryan"
	, "Sally"
	, "Sam"
	, "Samantha"
	, "Sarah"
	, "Sean"
	, "Sebastian"
	, "Simon"
	, "Sonia"
	, "Sophie"
	, "Stephanie"
	, "Stephen"
	, "Steven"
	, "Stewart"
	, "Sue"
	, "Theresa"
	, "Thomas"
	, "Tim"
	, "Tracey"
	, "Trevor"
	, "Una"
	, "Vanessa"
	, "Victor"
	, "Victoria"
	, "Virginia"
	, "Wanda"
	, "Warren"
	, "Wendy"
	, "William"
	, "Yvonne"
	, "Zoe"
	};

/**
 * This is a terrible function!
 */
static uint32_t my_arc4random_uniform (uint32_t upper_bound)
{
	assert (upper_bound > 0);

	uint32_t r = 0;

	for (size_t i = 0; i < 31; ++i)
	{
		r ^= random () << i;
	}

	return r % upper_bound;
}

const char *argp_program_version = "thrlab-1.0";
const char *argp_program_bug_address = "freysteinn@ru.is";

size_t my_strtonum (const char *str, size_t min, size_t max, const char **err)
{
	assert (str);
	assert (min <= max);
	assert (err);

	char *endptr;

	errno = 0;
	size_t num = strtoul (str, &endptr, 10);

	if (errno)
		*err = "Not an integer";
	else if (num < min)
		*err = "Integer too small";
	else if (num > max)
		*err = "Integer too large";
	else
		*err = NULL;

	return num;
}

static error_t argparse_opt
	( int key
	, char *arg
	, struct argp_state *state
	)
{
	struct arguments *arguments = state->input;
	const char *err;

	switch (key)
	{
		case 'b':
			arguments->barbers = my_strtonum
				( arg
				, 1
				, ARRSIZE (barber_names)
				, &err
				);
			if (err) argp_usage (state);
			break;
		case 'c':
			arguments->customers = my_strtonum (arg, 1, 1000, &err);
			if (err) argp_usage (state);
			break;
		case 'r':
			arguments->rate = my_strtonum (arg, 1, 10000, &err);
			if (err) argp_usage (state);
			break;
		case 'w':
			arguments->chairs = my_strtonum (arg, 1, 1000, &err);
			if (err) argp_usage (state);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct arguments argparse (int *argc, char ***argv)
{
	static const char *doc = "thrlab -- a mutex-less epoll";

	const struct argp argp =
		{ .options = (struct argp_option [])
			{ (struct argp_option)
				{ .name = "barbers"
				, .key = 'b'
				, .arg = "NUM"
				, .flags = 0
				, .doc = "Number of barbers employed [default = 3]"
				, .group = 0
				}
			, (struct argp_option)
				{ .name = "chairs"
				, .key = 'w'
				, .arg = "NUM"
				, .flags = 0
				, .doc = "Number of waiting chairs [default = 2]"
				, .group = 0
				}
			, (struct argp_option)
				{ .name = "customers"
				, .key = 'c'
				, .arg = "NUM"
				, .flags = 0
				, .doc = "Number of customers for the day [default = 10]"
				, .group = 0
				}
			, (struct argp_option)
				{ .name = "rate"
				, .key = 'r'
				, .arg = "MS"
				, .flags = 0
				, .doc = "Average time between new customers, in milliseconds"
				         " [default = 1000]"
				, .group = 0
				}
			, (struct argp_option)
				{ .name = NULL
				}
			}
		, .parser = argparse_opt
		, .args_doc = NULL
		, .doc = doc
		};

	/* default values */
	struct arguments arguments = (struct arguments)
		{ .barbers = 3
		, .chairs = 2
		, .customers = 10
		, .rate = 1000
		};

	argp_parse (&argp, *argc, *argv, 0, NULL, &arguments);

	return arguments;
}

static double timespec_diff (struct timespec now, struct timespec before)
{
	double dt
		= (now.tv_sec + now.tv_nsec / 1000000000.0)
		- (before.tv_sec + before.tv_nsec / 1000000000.0);

	return dt;
}

static void time_printf (const char *format, ...)
{
	assert (thrlab);
	assert (format);

	va_list ap;
	va_start (ap, format);

	struct timespec current;

	int status = clock_gettime (CLOCK_MONOTONIC, &current);
	assert (status == 0);

	printf ("%9.3f: ", timespec_diff (current, thrlab->start));
	vprintf (format, ap);

	va_end (ap);
}

static unsigned int add_customer
	( struct customer *customer
	, enum customer_status status
	)
{
	assert (thrlab);
	assert (customer);
	assert (thrlab->customer_count < thrlab->visitors);

	thrlab->customers[thrlab->customer_count] = customer;
	thrlab->statuses[thrlab->customer_count] = status;

	return thrlab->customer_count++;
}

static double customer_cutting_time (struct customer *customer)
{
	assert (thrlab);
	assert (customer);

	return 0.005 * (customer->hair_length - customer->hair_goal);
}

static void sleep_until_customer ()
{
	assert (thrlab);

	/* Add some funky pseudo-randomness */
	thrlab_sleep (my_arc4random_uniform (thrlab->rate * 2));
}

static const char *random_name ()
{
	size_t id = my_arc4random_uniform (ARRSIZE (customer_names));

	return customer_names[id];
}

static void check_complaints ()
{
	size_t complaints
		= thrlab->num_cutting
		+ thrlab->num_waiting
		+ thrlab->num_pending
		+ thrlab->complaint_reject_avail
		+ thrlab->complaint_reject_wait
		+ thrlab->complaint_reject_cut
		+ thrlab->complaint_reject_done
		+ thrlab->complaint_reject_again
		+ thrlab->complaint_accept_full
		+ thrlab->complaint_accept_wait
		+ thrlab->complaint_accept_cut
		+ thrlab->complaint_accept_done
		+ thrlab->complaint_accept_reject
		+ thrlab->complaint_prepare_pending
		+ thrlab->complaint_prepare_busy
		+ thrlab->complaint_prepare_again
		+ thrlab->complaint_prepare_done
		+ thrlab->complaint_prepare_reject
		+ thrlab->complaint_prepare_self
		+ thrlab->complaint_dismiss_pending
		+ thrlab->complaint_dismiss_wait
		+ thrlab->complaint_dismiss_done
		+ thrlab->complaint_dismiss_reject
		+ thrlab->complaint_dismiss_room
		+ thrlab->complaint_dismiss_self
		+ thrlab->complaint_dismiss_early
		+ thrlab->complaint_cut_fast
		+ thrlab->complaint_cut_slow;

	if (complaints == 0)
		return;

	printf
		( "\nOH NO%s! %s submitted %s!\n"
		, (complaints > 1) ? "ES" : ""
		, (complaints > 1) ? "Some customers" : "A customer"
		, (complaints > 1) ? "a number of complaints" : "a complaint"
		);

	size_t forgotten = thrlab->num_cutting + thrlab->num_waiting;

	if (forgotten)
	{
		printf
			("  - %zu customer%s locked inside!"
			, forgotten
			, (forgotten > 1) ? "s were" : " was"
			);

		if (thrlab->num_cutting)
		{
			printf
				( ", of which %zu %s still being cut!\n"
				, thrlab->num_cutting
				, (thrlab->num_cutting > 1) ? "were" : "was"
				);
		}
		else
		{
			printf ("!\n");
		}
	}

	if (thrlab->num_pending)
	{
		printf
			( "  - %zu %s never greeted at the door!\n"
			, thrlab->num_pending
			, (thrlab->num_pending > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_reject_avail)
	{
		printf
			( "  - %zu %s shown the door, but witnessed that seats were"
			  " available! (If this is the only complaint, then this might be OK)\n"
			, thrlab->complaint_reject_avail
			, (thrlab->complaint_reject_avail > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_reject_wait)
	{
		printf
			( "  - %zu %s shown the door whilst waiting for a barber!\n"
			, thrlab->complaint_reject_wait
			, (thrlab->complaint_reject_wait > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_reject_cut)
	{
		printf
			( "  - %zu %s shown the door in the middle of a haircut!\n"
			, thrlab->complaint_reject_cut
			, (thrlab->complaint_reject_cut > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_reject_done)
	{
		printf
			( "  - %zu %s told to come come back to be shown the door,"
			  " after having gotten their haircut!\n"
			, thrlab->complaint_reject_done
			, (thrlab->complaint_reject_done > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_reject_again)
	{
		printf
			( "  - %zu %s shown the door, repeatedly!\n"
			, thrlab->complaint_reject_again
			, (thrlab->complaint_reject_again > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_accept_full)
	{
		printf
			( "  - %zu couldn't find a chair and had to wait in the hallway!\n"
			, thrlab->complaint_accept_full
			);
	}

	if (thrlab->complaint_accept_wait)
	{
		printf
			( "  - %zu %s told to come inside and wait, whilst already"
			  " waiting!\n"
			, thrlab->complaint_accept_wait
			, (thrlab->complaint_accept_wait > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_accept_cut)
	{
		printf
			( "  - %zu %s told to come inside and wait, whilst their"
			  " barber was alredy cutting their hair!\n"
			, thrlab->complaint_accept_cut
			, (thrlab->complaint_accept_cut > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_accept_done)
	{
		printf
			( "  - %zu %s told to come inside and wait, already having"
			  " received their haircuts!\n"
			, thrlab->complaint_accept_done
			, (thrlab->complaint_accept_done > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_accept_reject)
	{
		printf
			( "  - %zu %s told to come inside and wait, after having been"
			  " rejected earlier!\n"
			, thrlab->complaint_accept_reject
			, (thrlab->complaint_accept_reject > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_prepare_pending)
	{
		printf
			( "  - %zu had to have their haircut outside!\n"
			, thrlab->complaint_prepare_pending
			);
	}

	if (thrlab->complaint_prepare_busy)
	{
		printf
			( "  - %zu %s told their barber was ready, but it turned out they"
			  " were otherwise occupied!\n"
			, thrlab->complaint_prepare_busy
			, (thrlab->complaint_prepare_busy > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_prepare_again)
	{
		printf
			( "  - %zu %s told \"their\" barber was ready, but a barber was"
			  " already cutting their hairs!\n"
			, thrlab->complaint_prepare_again
			, (thrlab->complaint_prepare_again > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_prepare_done)
	{
		printf
			( "  - %zu %s told their barber was ready, but had already just"
			  " gotten their haircuts!\n"
			, thrlab->complaint_prepare_done
			, (thrlab->complaint_prepare_done > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_prepare_reject)
	{
		printf
			( "  - %zu %s told their barber was ready, after having previously"
			  " been rejected!\n"
			, thrlab->complaint_prepare_reject
			, (thrlab->complaint_prepare_reject > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_prepare_self)
	{
		printf
			( "  - %zu had to cut their own hair!\n"
			, thrlab->complaint_prepare_self
			);
	}

	if (thrlab->complaint_dismiss_pending)
	{
		printf
			( "  - %zu %s told they had already received their haircuts when"
			  " they even hadn't entered the building yet!\n"
			, thrlab->complaint_dismiss_pending
			, (thrlab->complaint_dismiss_pending > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_dismiss_wait)
	{
		printf
			( "  - %zu %s told they had already received their haircuts whilst"
			  " waiting in the waiting room!\n"
			, thrlab->complaint_dismiss_wait
			, (thrlab->complaint_dismiss_wait > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_dismiss_done)
	{
		printf
			( "  - %zu %s asked to leave the barber's room, repeatedly!\n"
			, thrlab->complaint_dismiss_done
			, (thrlab->complaint_dismiss_done > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_dismiss_reject)
	{
		printf
			( "  - %zu %s told that they had already received their haircuts,"
			  " but were previously rejected entry!\n"
			, thrlab->complaint_dismiss_reject
			, (thrlab->complaint_dismiss_reject > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_dismiss_room)
	{
		printf
			( "  - A barber saw a false positive customer on %zu occasion%s!\n"
			, thrlab->complaint_dismiss_room
			, (thrlab->complaint_dismiss_room > 1) ? "s" : ""
			);
	}

	if (thrlab->complaint_dismiss_self)
	{
		printf
			( "  - %zu had to show themselves to the door!\n"
			, thrlab->complaint_dismiss_self
			);
	}

	if (thrlab->complaint_dismiss_early)
	{
		printf
			( "  - %zu lost their %s while undergoing a haircut!\n"
			, thrlab->complaint_dismiss_early
			, (thrlab->complaint_dismiss_early > 1) ? "lives" : "life"
			);
	}

	if (thrlab->complaint_cut_fast)
	{
		printf
			( "  - %zu %s cut way too fast! You better call Saul, neither you"
			  " nor the public are ready to witness this.\n"
			, thrlab->complaint_cut_fast
			, (thrlab->complaint_cut_fast > 1) ? "were" : "was"
			);
	}

	if (thrlab->complaint_cut_slow)
	{
		printf
			( "  - %zu hair was cut too slowly! (This will happen with many threads and is fine)\n"
			, thrlab->complaint_cut_slow
			);
	}
}

/******************************************************************************
 * Initialization & Cleanup
 *****************************************************************************/

void thrlab_setup (int *argc, char ***argv)
{
	assert (argc);
	assert (*argc > 0);
	assert (argv);
	assert (*argv);

	int status;

	struct arguments arguments = argparse (argc, argv);

	/* NOTE: this is the worst possible way to get good random numbers!
	 * Never do this at home! Use arc4random where supported instead.
	 */
	srandom (time (NULL));

	thrlab = malloc (sizeof (*thrlab));
	if (thrlab == NULL) goto error_thrlab;

	thrlab->visitors = arguments.customers;
	thrlab->barbers = arguments.barbers;
	thrlab->chairs = arguments.chairs;
	thrlab->rate = arguments.rate;

	thrlab->num_cutting = 0;
	thrlab->num_waiting = 0;
	thrlab->num_pending = 0;

	thrlab->complaint_reject_avail = 0;
	thrlab->complaint_reject_wait = 0;
	thrlab->complaint_reject_cut = 0;
	thrlab->complaint_reject_done = 0;
	thrlab->complaint_reject_again = 0;
	thrlab->complaint_accept_full = 0;
	thrlab->complaint_accept_wait = 0;
	thrlab->complaint_accept_cut = 0;
	thrlab->complaint_accept_done = 0;
	thrlab->complaint_accept_reject = 0;
	thrlab->complaint_prepare_pending = 0;
	thrlab->complaint_prepare_busy = 0;
	thrlab->complaint_prepare_again = 0;
	thrlab->complaint_prepare_done = 0;
	thrlab->complaint_prepare_reject = 0;
	thrlab->complaint_prepare_self = 0;
	thrlab->complaint_dismiss_pending = 0;
	thrlab->complaint_dismiss_wait = 0;
	thrlab->complaint_dismiss_done = 0;
	thrlab->complaint_dismiss_reject = 0;
	thrlab->complaint_dismiss_room = 0;
	thrlab->complaint_dismiss_self = 0;
	thrlab->complaint_dismiss_early = 0;
	thrlab->complaint_cut_fast = 0;
	thrlab->complaint_cut_slow = 0;

	thrlab->customer_count = 0;
	thrlab->customers = malloc (thrlab->visitors * sizeof (*thrlab->customers));
	if (thrlab->customers == NULL) goto error_customers;

	thrlab->statuses = malloc (thrlab->visitors * sizeof (*thrlab->statuses));
	if (thrlab->statuses == NULL) goto error_statuses;

	thrlab->times = malloc (thrlab->visitors * sizeof (*thrlab->times));
	if (thrlab->times == NULL) goto error_times;

	thrlab->occupancy = malloc
		( thrlab->barbers * sizeof (*thrlab->occupancy)
		);
	if (thrlab->occupancy == NULL) goto error_occupancy;

	for (size_t i = 0; i < thrlab->barbers; ++i)
		thrlab->occupancy[i] = NULL;

	status = pthread_mutex_init (&thrlab->mtx, NULL);
	if (status != 0) goto error_mtx;

	status = clock_gettime (CLOCK_MONOTONIC, &thrlab->start);
	assert (status == 0);

	printf
		( "%s%s"
		, "POSIX Barbershop open! All welcome!\n"
		, "===================================\n"
		);

	return;

error_mtx:
	free (thrlab->statuses);

error_occupancy:
	free (thrlab->times);

error_times:
	free (thrlab->statuses);

error_statuses:
	free (thrlab->customers);

error_customers:
	free (thrlab);

error_thrlab:
	exit (EXIT_FAILURE);
}

void thrlab_cleanup ()
{
	assert (thrlab);

	for (size_t i = 0; i < thrlab->customer_count; ++i)
	{
		assert (thrlab->customers[i]);

		int status = pthread_join (thrlab->customers[i]->thread, NULL);
		assert (status == 0);

//		pthread_detach(thrlab->customers[i]->thread);
		
		free (thrlab->customers[i]);
	}

	free (thrlab->customers);
	free (thrlab->statuses);
	free (thrlab->times);
	free (thrlab->occupancy);

	int status = pthread_mutex_destroy (&thrlab->mtx);
	if (status != 0) goto error_mtx;

	printf
		( "%s%s"
		, "============================\n"
		, "POSIX Barbershop closed! Good bye!\n"
		);

	check_complaints ();

	free (thrlab);
	thrlab = NULL;

	return;

error_mtx:
	exit (EXIT_FAILURE);
}

/******************************************************************************
 * Barbershop Information
 *****************************************************************************/

unsigned int thrlab_get_num_barbers ()
{
	assert (thrlab);

	return thrlab->barbers;
}

unsigned int thrlab_get_num_chairs ()
{
	assert (thrlab);

	return thrlab->chairs;
}

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

void thrlab_sleep (int ms)
{
	assert (thrlab);
	assert (ms >= 0);

	struct timespec ts = (struct timespec)
		{ .tv_sec = ms / 1000
		, .tv_nsec = ms % 1000 * 1000000
		};

	while (ts.tv_sec > 0 || ts.tv_nsec > 0)
	{
		int status = nanosleep (&ts, &ts);

		if (status == -1)
		{
			assert (errno == EINTR);
			continue;
		}

		break;
	}
}

/******************************************************************************
 * Customer Management
 *****************************************************************************/

struct my_ud
{
	void (*callback) (struct customer *, void *);
	struct customer *customer;
	void *ud;
};

static void *my_callback (void *ud)
{
	assert (ud);

	int status;
	struct my_ud m = *(struct my_ud *) ud;

	free (ud);

	m.callback (m.customer, m.ud);

	status = pthread_mutex_lock (&thrlab->mtx);
	assert (status == 0);

	assert (m.customer->id < thrlab->visitors);

	if (thrlab->statuses[m.customer->id] == CUSTOMER_CUTTING)
		++thrlab->complaint_dismiss_early;

	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);

	return NULL;
}

void thrlab_wait_for_customers
	( void (*callback) (struct customer *, void *)
	, void *ud
	)
{
	assert (thrlab);
	assert (callback);

	int status;
	struct customer *customer;

	for (size_t i = 0; i < thrlab->visitors; ++i)
	{
		sleep_until_customer ();

		customer = malloc (sizeof (struct customer));
		if (customer == NULL) goto error_customer;

		customer->name = random_name ();
		customer->id = 0;
		customer->hair_length = my_arc4random_uniform (100) + 100;
		customer->hair_goal = my_arc4random_uniform (25) + 50;

		status = pthread_mutex_lock (&thrlab->mtx);
		assert (status == 0);

		customer->id = add_customer (customer, CUSTOMER_PENDING);

		time_printf
			( "%s (#%u) arrives at the door.\n"
			, customer->name
			, customer->id
			);

		struct my_ud *m = malloc (sizeof (struct my_ud));
		if (m == NULL) exit (EXIT_FAILURE);

		m->callback = callback;
		m->customer = customer;
		m->ud = ud;

		status = pthread_create (&customer->thread, NULL, my_callback, m);
		if (status != 0) goto error_thread;

		++thrlab->num_pending;

		status = pthread_mutex_unlock (&thrlab->mtx);
		assert (status == 0);
	}

	return;

error_thread:
	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);

	free (customer);

error_customer:
	exit (EXIT_FAILURE);
}

void thrlab_accept_customer (struct customer *customer)
{
	assert (thrlab);
	assert (customer);
	assert (customer->id < thrlab->visitors);

	int status;

	status = pthread_mutex_lock (&thrlab->mtx);
	assert (status == 0);

	if (thrlab->statuses[customer->id] == CUSTOMER_PENDING)
	{
		time_printf ("%s (#%u) waits.\n", customer->name, customer->id);
	}
	else
	{
		time_printf ("%s (#%u) is confused!\n", customer->name, customer->id);
	}

	switch (thrlab->statuses[customer->id])
	{
		case CUSTOMER_PENDING:
			if (thrlab->chairs <= thrlab->num_waiting)
				++thrlab->complaint_accept_full;

			thrlab->statuses[customer->id] = CUSTOMER_WAITING;
			++thrlab->num_waiting;
			--thrlab->num_pending;

			break;
		case CUSTOMER_WAITING:
			++thrlab->complaint_accept_wait;
			break;
		case CUSTOMER_CUTTING:
			++thrlab->complaint_accept_cut;
			break;
		case CUSTOMER_DONE:
			++thrlab->complaint_accept_done;
			break;
		case CUSTOMER_REJECTED:
			++thrlab->complaint_accept_reject;
			break;
	}

	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);
}

void thrlab_reject_customer (struct customer *customer)
{
	assert (thrlab);
	assert (customer);
	assert (customer->id < thrlab->visitors);

	int status;

	status = pthread_mutex_lock (&thrlab->mtx);
	assert (status == 0);

	if (thrlab->statuses[customer->id] == CUSTOMER_PENDING)
	{
		time_printf
			( "%s (#%u) was turned away!\n"
			, customer->name
			, customer->id
			);
	}
	else
	{
		time_printf ("%s (#%u) is confused!\n", customer->name, customer->id);
	}


	switch (thrlab->statuses[customer->id])
	{
		case CUSTOMER_PENDING:
			if (thrlab->chairs > thrlab->num_waiting)
				++thrlab->complaint_reject_avail;

			thrlab->statuses[customer->id] = CUSTOMER_REJECTED;
			--thrlab->num_pending;

			break;
		case CUSTOMER_WAITING:
			++thrlab->complaint_reject_wait;
			break;
		case CUSTOMER_CUTTING:
			++thrlab->complaint_reject_cut;
			break;
		case CUSTOMER_DONE:
			++thrlab->complaint_reject_done;
			break;
		case CUSTOMER_REJECTED:
			++thrlab->complaint_reject_again;
			break;
	}

	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);
}

void thrlab_prepare_customer (struct customer *customer, unsigned int room)
{
	assert (thrlab);
	assert (customer);
	assert (customer->id < thrlab->visitors);
	assert (room < thrlab->barbers);

	int status;

	status = pthread_mutex_lock (&thrlab->mtx);
	assert (status == 0);

	if (thrlab->occupancy[room] && thrlab->occupancy[room] != customer)
	{
		time_printf
			( "%s'%s (#%u) confused! %s is busy cutting someone else!\n"
			, customer->name
			, (customer->name[strlen (customer->name) - 1] == 's') ? "" : "s"
			, customer->id
			, barber_names[room]
			);

		++thrlab->complaint_prepare_busy;

		goto done;
	}
	else if (thrlab->statuses[customer->id] == CUSTOMER_WAITING)
	{
		if (pthread_equal (pthread_self (), customer->thread) == 0)
		{
			time_printf
				( "%s begins giving %s (#%u) a haircut in room %u\n"
				, barber_names[room]
				, customer->name
				, customer->id
				, room
				);
		}
		else
		{
			time_printf
				( "%s orders %s (#%u) to cut their own hair!\n"
				, barber_names[room]
				, customer->name
				, customer->id
				);

			++thrlab->complaint_prepare_self;
		}
	}
	else
	{
		time_printf
			( "%s and %s (#%u) are confused!\n"
			, barber_names[room]
			, customer->name
			, customer->id
			);
	}

	switch (thrlab->statuses[customer->id])
	{
		case CUSTOMER_PENDING:
			++thrlab->complaint_prepare_pending;
			break;
		case CUSTOMER_WAITING:
			if (thrlab->occupancy[room])
			{
				++thrlab->complaint_prepare_busy;
			}
			else
			{
				thrlab->occupancy[room] = customer;
				thrlab->statuses[customer->id] = CUSTOMER_CUTTING;
				++thrlab->num_cutting;
				--thrlab->num_waiting;

				status = clock_gettime
					( CLOCK_MONOTONIC
					, &thrlab->times[customer->id]
					);
				assert (status == 0);
			}
			break;
		case CUSTOMER_CUTTING:
			++thrlab->complaint_prepare_again;
			break;
		case CUSTOMER_DONE:
			++thrlab->complaint_prepare_done;
			break;
		case CUSTOMER_REJECTED:
			++thrlab->complaint_prepare_reject;
			break;
	}

done:
	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);
}

void thrlab_dismiss_customer (struct customer *customer, unsigned int room)
{
	assert (thrlab);
	assert (customer);
	assert (customer->id < thrlab->visitors);
	assert (room < thrlab->barbers);

	int status;

	status = pthread_mutex_lock (&thrlab->mtx);
	assert (status == 0);

	if (thrlab->occupancy[room] != customer)
	{
		time_printf
			( "%s'%s confused! %s (#%u) wasn't found in their room!\n"
			, barber_names[room]
			, (barber_names[room][strlen (barber_names[room]) - 1] == 's') ? "" : "s"
			, customer->name
			, customer->id
			);

		++thrlab->complaint_dismiss_room;

		goto done;
	}
	else if (thrlab->statuses[customer->id] == CUSTOMER_CUTTING)
	{
		if (pthread_equal (pthread_self (), customer->thread) == 0)
		{
			time_printf
				( "%s finishes cutting %s'%s (#%u) hair.\n"
				, barber_names[room]
				, customer->name
				, (customer->name[strlen (customer->name) - 1] == 's') ? "" : "s"
				, customer->id
				);
		}
		else
		{
			time_printf
				( "%s orders %s (#%u) to show themselves to the door after"
				  " their haircut!\n"
				, barber_names[room]
				, customer->name
				, customer->id
				);

			++thrlab->complaint_dismiss_self;
		}
	}
	else
	{
		time_printf
			( "%s and %s (#%u) are confused!\n"
			, barber_names[room]
			, customer->name
			, customer->id
			);
	}

	switch (thrlab->statuses[customer->id])
	{
		case CUSTOMER_PENDING:
			++thrlab->complaint_dismiss_pending;
			break;
		case CUSTOMER_WAITING:
			++thrlab->complaint_dismiss_wait;
			break;
		case CUSTOMER_CUTTING:;
			struct timespec current;
			status = clock_gettime (CLOCK_MONOTONIC, &current);

			double t = customer_cutting_time (customer);
			double dt = timespec_diff (current, thrlab->times[customer->id]);

			if (dt < t)
				++thrlab->complaint_cut_fast;

			if (dt >= 2*t)
				++thrlab->complaint_cut_slow;

			thrlab->occupancy[room] = NULL;
			thrlab->statuses[customer->id] = CUSTOMER_DONE;
			--thrlab->num_cutting;
			break;
		case CUSTOMER_DONE:
			++thrlab->complaint_dismiss_done;
			break;
		case CUSTOMER_REJECTED:
			++thrlab->complaint_dismiss_reject;
			break;
	}

done:
	status = pthread_mutex_unlock (&thrlab->mtx);
	assert (status == 0);
}
