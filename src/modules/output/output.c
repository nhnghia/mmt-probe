/*
 * output.c
 *
 *  Created on: Dec 18, 2017
 *          by: Huu Nghia
 */
#include <stdarg.h>

#include "../../configure.h"

#include "output.h"

#include "../../lib/memory.h"
#include "../../lib/string_builder.h"
#include "file/file_output.h"
#include "kafka/kafka_output.h"
#include "socket/socket_output.h"
#include "mongodb/mongodb.h"
#include "redis/redis.h"

struct output_struct{
	uint16_t index;
	const char*input_src;
	uint32_t probe_id;
	struct timeval last_report_ts;
	const struct output_conf_struct *config;

	struct output_modules_struct{
		file_output_t *file;
		IF_ENABLE_REDIS(   redis_output_t *redis; )
		IF_ENABLE_KAFKA(   kafka_output_t *kafka; )
		IF_ENABLE_MONGODB( mongodb_output_t *mongodb; )
		IF_ENABLE_SOCKET(  socket_output_t *socket; )
	}modules;
};


//public API
output_t *output_alloc_init( uint16_t output_id, const struct output_conf_struct *config, uint32_t probe_id, const char* input_src ){
	int i;
	if( ! config->is_enable )
		return NULL;

	output_t *ret = mmt_alloc_and_init_zero( sizeof( output_t ));
	ret->config = config;
	ret->index  = output_id;
	ret->input_src = input_src;
	ret->probe_id  = probe_id;

	if( ! ret->config->is_enable )
		return ret;

	if( ret->config->file->is_enable ){
		ret->modules.file = file_output_alloc_init( ret->config->file, output_id );
	}

#ifdef REDIS_MODULE
	if( ret->config->redis->is_enable )
		ret->modules.redis = redis_init( ret->config->redis );
#endif

#ifdef KAFKA_MODULE
	if( ret->config->kafka->is_enable )
		ret->modules.kafka = kafka_output_init( ret->config->kafka );
#endif

#ifdef MONGODB_MODULE
	if( ret->config->mongodb->is_enable )
		ret->modules.mongodb = mongodb_output_alloc_init( ret->config->mongodb, ret->config->cache_max, output_id );
#endif

#ifdef SOCKET_MODULE
	if( ret->config->socket->is_enable )
		ret->modules.socket = socket_output_init( ret->config->socket );
#endif
	return ret;
}


static inline int _write( output_t *output, output_channel_conf_t channels, const char *message ){
	int ret = 0;
	char new_msg[ MAX_LENGTH_REPORT_MESSAGE ];

	//we surround message inside [] to convert it to JSON
	//this is done when output format is JSON or when we need to output to MongoDB
	if( output->config->format == OUTPUT_FORMAT_JSON
#ifdef MONGODB_MODULE
			|| (output->modules.mongodb && IS_ENABLE_OUTPUT_TO( MONGODB, channels ) )
#endif
			){

		//surround message by [ and ]
		new_msg[0] = '[';
		size_t len = strlen( message );
		memcpy( new_msg + 1, message, len );
		new_msg[ len+1 ] = ']';
		new_msg[ len+2 ] = '\0';

		//use new_msg when output format is JSON
		if( output->config->format == OUTPUT_FORMAT_JSON )
			message = new_msg;
	}

	//output to file
	if( IS_ENABLE_OUTPUT_TO( FILE, channels )){
		file_output_write( output->modules.file, message );
		ret ++;
	}

#ifdef KAFKA_MODULE
	//output to Kafka
	if( output->modules.kafka && IS_ENABLE_OUTPUT_TO( KAFKA, channels )){
		ret += kafka_output_send( output->modules.kafka, message );
	}
#endif

#ifdef REDIS_MODULE
	//output to redis
	if( output->modules.redis && IS_ENABLE_OUTPUT_TO( REDIS, channels )){
		ret += redis_send( output->modules.redis, message );
	}
#endif

#ifdef MONGODB_MODULE
	if( output->modules.mongodb && IS_ENABLE_OUTPUT_TO( MONGODB, channels )){
		//here we output new_msg (not message)
		mongodb_output_write( output->modules.mongodb, new_msg );
		ret ++;
	}
#endif

#ifdef SOCKET_MODULE
	if( output->modules.socket && IS_ENABLE_OUTPUT_TO( SOCKET, channels )){
		ret += socket_output_send( output->modules.socket, message );
	}
#endif
	return ret;
}

int output_write_report( output_t *output, output_channel_conf_t channels,
		report_type_t report_type, const struct timeval *ts,
		const char* message_body){

	//global output is disable or no output on this channel
	if( output == NULL
			|| output->config == NULL
			|| ! output->config->is_enable
			|| IS_DISABLE_OUTPUT( channels ) )
		return 0;

	char message[ MAX_LENGTH_REPORT_MESSAGE ];
	int offset = 0;
	STRING_BUILDER_WITH_SEPARATOR( offset, message, MAX_LENGTH_FULL_PATH_FILE_NAME, ",",
			__INT( report_type ),
			__INT( output->probe_id ),
			__STR( output->input_src ),
			__TIME( ts )
	);

	if( message_body != NULL ){
		message[ offset ++ ] = ',';
		size_t len = strlen( message_body );
		memcpy( message+offset, message_body, len+1 ); //copy also '\0' at the end of message_body
	}

	int ret = _write( output, channels, message );
	output->last_report_ts.tv_sec  = ts->tv_sec;
	output->last_report_ts.tv_usec = ts->tv_usec;

	return ret;
}

int output_write_report_with_format( output_t *output, output_channel_conf_t channels,
		report_type_t report_type, const struct timeval *ts,
		const char* format, ...){

	//global output is disable or no output on this channel
	if( output == NULL
			|| output->config == NULL
			|| ! output->config->is_enable
			|| IS_DISABLE_OUTPUT( channels ) )
		return 0;

	char message[ MAX_LENGTH_REPORT_MESSAGE ];
	int offset;

	if( unlikely( format == NULL )){
		return output_write_report( output, channels, report_type, ts, NULL);
	} else {
		va_list args;
		offset = 0;
		va_start( args, format );
		offset += vsnprintf( message + offset, MAX_LENGTH_REPORT_MESSAGE - offset, format, args);
		va_end( args );

		return output_write_report( output, channels, report_type, ts, message );
	}
}

//public API
int output_write( output_t *output, output_channel_conf_t channels, const char *message ){
	//global output is disable or no output on this channel
	if( ! output || ! output->config->is_enable || IS_DISABLE_OUTPUT(channels ))
		return 0;

	return _write( output, channels, message );
}

void output_flush( output_t *output ){
	if( !output )
		return;

	if( output->modules.file )
		file_output_flush( output->modules.file );

#ifdef MONGODB_MODULE
	if( output->modules.mongodb
			&& output->config->mongodb->is_enable )
		mongodb_output_flush_to_database( output->modules.mongodb );
#endif
}

void output_release( output_t * output){
	if( !output ) return;

	file_output_release( output->modules.file );

	IF_ENABLE_MONGODB( mongodb_output_release( output->modules.mongodb ); )
	IF_ENABLE_KAFKA( kafka_output_release( output->modules.kafka ); )
	IF_ENABLE_REDIS( redis_release( output->modules.redis ); )
	IF_ENABLE_SOCKET( socket_output_release( output->modules.socket ); )
	mmt_probe_free( output );
}
