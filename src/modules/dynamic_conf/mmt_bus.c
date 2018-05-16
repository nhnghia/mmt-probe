/*
 * mmt_bus.c
 *
 *  Created on: May 15, 2018
 *          by: Huu Nghia Nguyen
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "mmt_bus.h"
#include "../../lib/tools.h"
#include "../../lib/log.h"

/**
 * The id signal to inform other processes to read data in the bus.
 * This signal must not be used elsewhere.
 */
#define SIGNAL_ID SIGUSR1

struct subscriber{
	pid_t pid;
	void *user_data;
	bus_subscriber_callback_t callback;
};

struct mmt_bus{
	uint8_t nb_subscribers;
	struct subscriber sub_lst[ MMT_BUS_MAX_SUBSCRIBERS ];
	pthread_mutex_t mutex; //mutex to synchronize read/write data among publishers and subscribers

	size_t message_size; //real size of message being used
	char message[ MMT_BUS_MAX_MESSAGE_SIZE ];
	int reply_code;
};

static struct mmt_bus *bus = NULL;

bool mmt_bus_create(){
	//already created
	if( bus != NULL )
		return false;

	//size of memory segment to share
	size_t total_shared_memory_size = sizeof( struct mmt_bus);

	//create a shared memory segment
	bus = mmap(0, total_shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (bus == MAP_FAILED) {
		log_write(LOG_ERR, "Cannot create shared memory for %zu B: %s", total_shared_memory_size, strerror( errno ));
		abort();
	}

	//initialize memory segment
	memset( bus, 0, total_shared_memory_size );

	// initialise mutex so it works properly in shared memory
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&bus->mutex, &attr);

	return true;
}

mmt_bus_code_t mmt_bus_publish( const char*message, size_t message_size, uint16_t *reply_code ){
	int i;
	if( bus == NULL )
		return MMT_BUS_NO_INIT;
	if( message_size > MMT_BUS_MAX_MESSAGE_SIZE )
		return MMT_BUS_OVER_MSG_SIZE;

	bool old_msg_is_consummed = false;

	//cannot block memory?
	if( pthread_mutex_lock( &bus->mutex ) != 0){
		log_write( LOG_ERR, "Cannot lock mmt-bus for publishing: %s", strerror( errno) );
		return MMT_BUS_LOCK_ERROR;
	}

//	DEBUG("Number of subscribers: %d", bus->nb_subscribers );
	//the message must be processed by at leat one subscriber
	if( bus->reply_code != DYN_CONF_CMD_DO_NOTHING ){
		old_msg_is_consummed = true;

		//store data to the shared memory segment
		memcpy( bus->message, message, message_size );
		bus->message_size = message_size;

		//this message is fresh, no one consumes it
		bus->reply_code = DYN_CONF_CMD_DO_NOTHING;
	}

	//unblock
	pthread_mutex_unlock( &bus->mutex );

	if( !old_msg_is_consummed )
		return MSG_BUS_OLD_MSG_NO_CONSUME;

	//notify to all subscribers
	for( i=0; i<MMT_BUS_MAX_SUBSCRIBERS; i++ )
		if( bus->sub_lst[i].pid != 0 ){
//			DEBUG("Wake up process %d", bus->sub_lst[i].pid );
			kill( bus->sub_lst[i].pid, SIGNAL_ID );
		}

	if( reply_code == NULL )
		return MMT_BUS_SUCCESS;

	//waiting for a response from one of the subscribers
	while( true ){

		if( pthread_mutex_lock( &bus->mutex ) != 0){
			log_write( LOG_ERR, "Cannot lock mmt-bus for publishing: %s", strerror( errno) );
			return MMT_BUS_LOCK_ERROR;
		}

		//one subscriber replied
		if( bus->reply_code != DYN_CONF_CMD_DO_NOTHING ){
			*reply_code = bus->reply_code;
			goto _end;
		}

		//unblock
		pthread_mutex_unlock( &bus->mutex );

		usleep( 10000 );
	}

	_end:
	pthread_mutex_unlock( &bus->mutex );

	return MMT_BUS_SUCCESS;
}

static void _signal_handler( int type ){
	int i;
	char msg[ MMT_BUS_MAX_MESSAGE_SIZE ];
	size_t msg_size = 0;

//	DEBUG("Subscriber is waken up");

	pid_t pid = getpid();
	for( i=0; i<MMT_BUS_MAX_SUBSCRIBERS; i++ )
		if( bus->sub_lst[i].pid == pid ){
			if( bus->sub_lst[i].callback != NULL ){
				//TODO: pthread_mutex_lock is not signal-safety
				if( pthread_mutex_lock( &bus->mutex ) == 0){
					msg_size = bus->message_size;
					memcpy( msg, bus->message, msg_size );
					pthread_mutex_unlock( &bus->mutex ); //unblock

					//fire the callback of the subscriber
					int ret = bus->sub_lst[i].callback( msg, msg_size, bus->sub_lst[i].user_data );

					//DEBUG("Reply code: %d", ret );

					//update reply code
					if( ret != DYN_CONF_CMD_DO_NOTHING ){
						if( pthread_mutex_lock( &bus->mutex ) == 0){
							bus->reply_code = ret;
							pthread_mutex_unlock( &bus->mutex );
						}
					}

				}else
					log_write( LOG_ERR, "Cannot lock mmt-bus for reading: %s", strerror( errno) );
				return;
			}
			log_write( LOG_ERR, "Callback off %d-th (pid = %d) is NULL", i, pid );
			return;
		}
}

bool mmt_bus_subscribe( bus_subscriber_callback_t cb, void *user_data ){
	int i;
	pid_t pid = getpid();

	if( pthread_mutex_lock( &bus->mutex ) != 0){
		log_write( LOG_ERR, "Cannot lock mmt-bus for publishing: %s", strerror( errno) );
		return false;
	}

	for( i=0; i<MMT_BUS_MAX_SUBSCRIBERS; i++ ){
		//already exist
		if( bus->sub_lst[i].pid == pid )
			return false;

		//found one slot
		if( bus->sub_lst[i].pid == 0 ){
			//register in the list
			bus->sub_lst[i].pid = pid;
			bus->sub_lst[i].callback = cb;
			bus->sub_lst[i].user_data = user_data;

			bus->nb_subscribers ++;

//			DEBUG("Number of subscribers: %d", bus->nb_subscribers );

			//register a handler to respond to a notification from a publisher
			signal( SIGNAL_ID , _signal_handler );

			pthread_mutex_unlock( &bus->mutex );
			return true;
		}
	}
	pthread_mutex_unlock( &bus->mutex );

	log_write( LOG_ERR, "Number of subscribers is bigger than the limit (%d)", MMT_BUS_MAX_SUBSCRIBERS );
	//no more slot for this
	return false;
}

bool mmt_bus_unsubscribe(){
	int i;
	pid_t pid = getpid();

	if( pthread_mutex_lock( &bus->mutex ) != 0){
		log_write( LOG_ERR, "Cannot lock mmt-bus for publishing: %s", strerror( errno) );
		return false;
	}

	for( i=0; i<MMT_BUS_MAX_SUBSCRIBERS; i++ ){
		//found one process
		if( bus->sub_lst[i].pid == pid ){
			bus->sub_lst[i].pid = 0; //this is enough to unregister
			//unregister signal handler
			signal( SIGNAL_ID, SIG_DFL );

			bus->nb_subscribers --;

			pthread_mutex_unlock( &bus->mutex );
			return true;
		}
	}

	pthread_mutex_unlock( &bus->mutex );
	return false;

}

void mmt_bus_release(){
	if( bus == NULL )
		return;
	munmap( bus, sizeof( struct mmt_bus) );
	bus = NULL;
}
