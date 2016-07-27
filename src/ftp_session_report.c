#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "mmt_core.h"
#include "tcpip/mmt_tcpip.h"
#include "processing.h"


/*char * str_replace_all_char(char *str,int c1, int c2){
    char *new_str;
    new_str = (char*)malloc(strlen(str)+1);
    memcpy(new_str,str,strlen(str));
    new_str[strlen(str)] = '\0';
    int i;
    for(i=0;i<strlen(str);i++){
        if((int)new_str[i]==c1){
            new_str[i]=(char)c2;
        }
    }
    return new_str;
}*/

void write_data_to_file (const ipacket_t * ipacket,char * path,  char * content, int len) {
    int fd = 0,MAX=200;
    char filename[len];
    int valid =0;

	mmt_probe_context_t * probe_context = get_probe_context_config();
    // Replace path by file name
    char *file_path;
    file_path = str_replace(path,"/","_");

    uint64_t session_id = get_session_id (ipacket->session);
      snprintf(filename,MAX, "%s%"PRIu64"_%s",probe_context->ftp_reconstruct_output_location,session_id ,file_path);
      filename[MAX]='\0';
    if ( (fd = open ( filename , O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW , S_IRWXU | S_IRWXG | S_IRWXO )) < 0 )
    {
      fprintf ( stderr , "\n[e] Error %d writting data to \"%s\": %s" , errno , file_path , strerror( errno ) );
      free(file_path);
      return;
    }

    if(len>0){
    	printf("Going to write to file: %s\n",file_path);
  	  // printf("Data: \n%s\n",content);
  	  printf("Data len: %d\n",len);
  	  valid =write ( fd , content , len );
  	  free(file_path);
  	  file_path = NULL;
    }
    free (path);
    path = NULL;
    if (valid==0)printf("Nothing to write");
    close ( fd );

}

void reconstruct_data(const ipacket_t * ipacket){

 	// Get data type - only reconstruct the FILE data
	uint8_t * data_type = (uint8_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_DATA_TYPE);
	int d_type = -1;
	if(data_type){
		d_type = *data_type;
	}

	// Get file name
	char * file_name = (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_FILE_NAME);

	// Get packet len
	uint32_t * data_len = (uint32_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_PACKET_DATA_LEN);
	int len = 0;
	if(data_len){
		len = *data_len;
	}

	// Get packet payload pointer - after TCP
	char * data_payload = (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,PROTO_PAYLOAD);
	if(len > 0 && file_name && data_payload && d_type==1){
		printf("Going to write data of packet %lu\n",ipacket->packet_id);
		write_data_to_file(ipacket,file_name,data_payload,len);
		// free(data_payload);
	}
	//return 0;
}



void reset_ftp_parameters(const ipacket_t * ipacket,session_struct_t *temp_session ){

	((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_sec=0;
	((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_usec=0;
	((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_sec=0;
	((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_usec=0;
/*	((ftp_session_attr_t*) temp_session->app_data)->response_value=NULL;
	((ftp_session_attr_t*) temp_session->app_data)->file_size=0;
	((ftp_session_attr_t*) temp_session->app_data)->filename=NULL;
	((ftp_session_attr_t*) temp_session->app_data)->response_code=0;
	((ftp_session_attr_t*) temp_session->app_data)->session_password=NULL;
	((ftp_session_attr_t*) temp_session->app_data)->packet_request=NULL;
	*/
}

void ftp_session_connection_type_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
/*	if(ipacket->session == NULL) return;
	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	mmt_probe_context_t * probe_context = get_probe_context_config();
	if (temp_session != NULL) {
		if (temp_session->app_data == NULL) {
			ftp_session_attr_t * ftp_data = (ftp_session_attr_t *) malloc(sizeof (ftp_session_attr_t));
			if (ftp_data != NULL) {
				memset(ftp_data, '\0', sizeof (ftp_session_attr_t));
				temp_session->app_format_id = probe_context->ftp_id;
				temp_session->app_data = (void *) ftp_data;
			} else {
				mmt_log(probe_context, MMT_L_WARNING, MMT_P_MEM_ERROR, "Memory error while creating SSL reporting context");
				//fprintf(stderr, "Out of memory error when creating SSL specific data structure!\n");
				return;
			}
		}

	}
	//if (probe_context->ftp_reconstruct_enable == 1)reconstruct_data(ipacket);

	if (temp_session != NULL && temp_session->app_data != NULL) {
		//if (probe_context->ftp_reconstruct_enable == 1)reconstruct_data(ipacket);

		uint8_t * conn_type = (uint8_t *) attribute->data;
		if (conn_type != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->session_conn_type = *conn_type;

			if (((ftp_session_attr_t*) temp_session->app_data)->session_conn_type == 2){
				uint64_t * control_session_id= (uint64_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_CONT_IP_SESSION_ID);
				if (control_session_id != NULL)((ftp_session_attr_t*) temp_session->app_data)->session_id_control_channel = * control_session_id; //need to identify the control session for corresponding data session
				if (((ftp_session_attr_t*) temp_session->app_data)->data_response_time_seen == 0){
					((ftp_session_attr_t *) temp_session->app_data)->first_response_seen_time = ipacket->p_hdr->ts; //Needed for data transfer time
					uint8_t * data_type = (uint8_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_DATA_TYPE);
					if (data_type != NULL){
						((ftp_session_attr_t*) temp_session->app_data)->data_type  = * data_type;
						if (((ftp_session_attr_t*) temp_session->app_data)->data_type == 1){
							 response time defined here
							 * http://www.igi-global.com/chapter/future-networked-healthcare-systems/131381
							 * FTP Response Time: It is the time elapsed between a client application sending a request to the FTP server and receiving the response packet.
							 * The response time includes the 3 way TCP handshake.
							 *
							((ftp_session_attr_t*) temp_session->app_data)->response_time = TIMEVAL_2_USEC(mmt_time_diff(get_session_init_time(ipacket->session), ipacket->p_hdr->ts));
							((ftp_session_attr_t*) temp_session->app_data)->data_response_time_seen = 1;
							//printf ("ftp_response_time = %lu \n",((ftp_session_attr_t*) temp_session->app_data)->response_time);
						}
					}
				}
			}
		}
	}*/

	/*	uint8_t * direction = (uint8_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_DATA_DIRECTION);
		if (direction != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->direction = *direction;
		}

		char * username = (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_USERNAME);
		if (username != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			if (((ftp_session_attr_t*) temp_session->app_data)->session_username != NULL){

				if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->session_username ,username) !=0 ){
					free (((ftp_session_attr_t*) temp_session->app_data)->session_username );
					((ftp_session_attr_t*) temp_session->app_data)->session_username  = NULL;
					((ftp_session_attr_t*) temp_session->app_data)->session_username = username;
				}else {
					free (username);
				}
			}else {
				((ftp_session_attr_t*) temp_session->app_data)->session_username = username;
			}
		}

		char * password = (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_PASSWORD);
		if (password != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			if (((ftp_session_attr_t*) temp_session->app_data)->session_password != NULL){

				if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->session_password ,password) !=0 ){
					free (((ftp_session_attr_t*) temp_session->app_data)->session_password );
					((ftp_session_attr_t*) temp_session->app_data)->session_password  = NULL;
					((ftp_session_attr_t*) temp_session->app_data)->session_password =password;
				}else {
					free (password);
				}
			}else {
				((ftp_session_attr_t*) temp_session->app_data)->session_password =password;

			}
		}
	char * packet_request= (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_PACKET_REQUEST);
	if (packet_request != NULL && temp_session->app_format_id == probe_context->ftp_id) {
		if (((ftp_session_attr_t*) temp_session->app_data)->packet_request != NULL){

			if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->packet_request ,packet_request) !=0 ){
				free (((ftp_session_attr_t*) temp_session->app_data)->packet_request );
				((ftp_session_attr_t*) temp_session->app_data)->packet_request  = NULL;
				((ftp_session_attr_t*) temp_session->app_data)->packet_request =packet_request;
			}else {
				free (packet_request);
			}
		}else {
			((ftp_session_attr_t*) temp_session->app_data)->packet_request =packet_request;

		}
	}


	uint32_t * file_size = (uint32_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_FILE_SIZE);
	if (file_size != NULL && temp_session->app_format_id == probe_context->ftp_id ) {
		((ftp_session_attr_t*) temp_session->app_data)->file_size = * file_size;

	}
	if (((ftp_session_attr_t*) temp_session->app_data)->session_conn_type == 2){
		char * file_name = (char *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_FILE_NAME);
		//fprintf(stderr,"packet_id =%lu, filename %s, id=%lu \n",ipacket->packet_id,file_name,get_session_id(ipacket->session));
		if (file_name != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			if (((ftp_session_attr_t*) temp_session->app_data)->filename != NULL){
				if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->filename,file_name) !=0 ){
					free (((ftp_session_attr_t*) temp_session->app_data)->filename);
					((ftp_session_attr_t*) temp_session->app_data)->filename = NULL;
					((ftp_session_attr_t*) temp_session->app_data)->filename=file_name;
				}else {
					free (file_name);
				}
			}else {
				((ftp_session_attr_t*) temp_session->app_data)->filename=file_name;

			}

		}
	}
	uint16_t * response_code = (uint16_t *) get_attribute_extracted_data(ipacket,PROTO_FTP,FTP_PACKET_RESPONSE_CODE);
	if (response_code != NULL && temp_session->app_format_id == probe_context->ftp_id) {
		((ftp_session_attr_t*) temp_session->app_data)->response_code= * response_code;
		if(*response_code == 150){
			((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_sec = ipacket->p_hdr->ts.tv_sec;
			((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_usec= ipacket->p_hdr->ts.tv_usec;
		}
	}*/

}

/*
void ftp_data_direction_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		uint8_t * direction = (uint8_t *) attribute->data;
		if (direction != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->direction = *direction;
		}
	}

}

void ftp_user_name_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		char * username = (char *) attribute->data;
		if (username != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->session_username =  username;
		}
	}
}

void ftp_password_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		char * password = (char *) attribute->data;
		if (password != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->session_password =  password;
		}
	}
}

void ftp_packet_request_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		char * packet_request= (char *)attribute->data;
		if (packet_request != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->packet_request =  packet_request;
		}
	}
}*/

void ftp_response_value_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
/*	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		char * response_value = (char *) attribute->data;
		if (response_value != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			if (((ftp_session_attr_t*) temp_session->app_data)->response_value != NULL){
				if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->response_value,response_value) !=0 ){
					free (((ftp_session_attr_t*) temp_session->app_data)->response_value);
					((ftp_session_attr_t*) temp_session->app_data)->response_value = NULL;
					((ftp_session_attr_t*) temp_session->app_data)->response_value=response_value;
				}else {
					free (response_value);
				}
			}else {
				((ftp_session_attr_t*) temp_session->app_data)->response_value=response_value;

			}
		}

	}
	char message[MAX_MESS + 1];
	//char * location;
	int i;
	char ip_src_str[46];
	char ip_dst_str[46];

	//FILE * out_file = (probe_context->data_out_file != NULL) ? probe_context->data_out_file : stdout;
	mmt_session_t * ftp_session = get_session_from_packet(ipacket);
	if(ftp_session == NULL) return;

	struct timeval end_time = get_session_last_activity_time(ipacket->session);


	if (temp_session->ipversion == 4) {
		inet_ntop(AF_INET, (void *) &temp_session->ipclient.ipv4, ip_src_str, INET_ADDRSTRLEN);
		inet_ntop(AF_INET, (void *) &temp_session->ipserver.ipv4, ip_dst_str, INET_ADDRSTRLEN);
	} else {
		inet_ntop(AF_INET6, (void *) &temp_session->ipclient.ipv6, ip_src_str, INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, (void *) &temp_session->ipserver.ipv6, ip_dst_str, INET6_ADDRSTRLEN);
	}

	for(i = 0; i < probe_context->condition_reports_nb; i++) {
		mmt_condition_report_t * condition_report = &probe_context->condition_reports[i];
		if (strcmp(((ftp_session_attr_t*) temp_session->app_data)->response_value,"Transfer complete.")==0 && strcmp(condition_report->condition.condition,"FTP")==0 && probe_context->ftp_reconstruct_enable ==1){

			((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_sec= ipacket->p_hdr->ts.tv_sec;
			((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_usec=ipacket->p_hdr->ts.tv_usec;
			snprintf(message, MAX_MESS,
					"%u,%u,\"%s\",%lu.%lu,%"PRIu64",%"PRIu32",\"%s\",\"%s\",%hu,%hu,%"PRIu8",%"PRIu8",\"%s\",\"%s\",%"PRIu32",\"%s\",%lu.%06lu,%lu.%06lu,%"PRIu64",%u,%"PRIu64"",
					probe_context->ftp_reconstruct_id,probe_context->probe_id_number, probe_context->input_source, end_time.tv_sec, end_time.tv_usec,temp_session->session_id,temp_session->thread_number,
					ip_dst_str, ip_src_str,
					temp_session->serverport, temp_session->clientport,
					((ftp_session_attr_t*) temp_session->app_data)->session_conn_type,
					((ftp_session_attr_t*) temp_session->app_data)->direction,
					((ftp_session_attr_t*) temp_session->app_data)->session_username,
					((ftp_session_attr_t*) temp_session->app_data)->session_password,
					((ftp_session_attr_t*) temp_session->app_data)->file_size,
					((ftp_session_attr_t*) temp_session->app_data)->filename,
					((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_sec,
					((ftp_session_attr_t*) temp_session->app_data)->file_download_finishtime_usec,
					((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_sec,
					((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_usec,
					temp_session ->session_id,
					temp_session ->thread_number,((ftp_session_attr_t*) temp_session->app_data)->session_id_control_channel
			);
			message[ MAX_MESS ] = '\0'; // correct end of string in case of truncated message
			//send_message_to_file ("ftp.download.report", message);
			//if (probe_context->output_to_file_enable==1)send_message_to_file (message);
			//if (probe_context->redis_enable==1)send_message_to_redis ("ftp.download.report", message);

			if (probe_context->output_to_file_enable == 1)send_message_to_file_thread (message, (void *)user_args);
			if (probe_context->redis_enable == 1)send_message_to_redis ("ftp.download.report", message);
			reset_ftp_parameters(ipacket,temp_session);
			break;
		}
	}*/
}
/*void ftp_file_size_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();
	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		uint32_t * file_size = (uint32_t *) attribute->data;
		if (file_size != NULL && temp_session->app_format_id == probe_context->ftp_id ) {
			((ftp_session_attr_t*) temp_session->app_data)->file_size = * file_size;

		}
	}
}*/
/*
void ftp_file_name_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	//int valid;
	//char * name;
	//name= (char*)malloc(sizeof(char)*200);

	if (temp_session != NULL && temp_session->app_data != NULL) {
		char * file_name = (char *) attribute->data;
		//file_name=str_replace_all_char(file_name,'/','_');

		//valid=snprintf(name,MAX, "%lu.%lu_%s",((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_sec,((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_usec,file_name);
		//name[valid]='\0';

		if (file_name != NULL && temp_session->app_format_id == probe_context->ftp_id) {

			((ftp_session_attr_t*) temp_session->app_data)->filename=file_name;
		}
		//if (file_name != NULL)free(file_name);
	}
}
*/


/*void ftp_response_code_handle(const ipacket_t * ipacket, attribute_t * attribute, void * user_args) {
	if(ipacket->session == NULL) return;
	mmt_probe_context_t * probe_context = get_probe_context_config();

	session_struct_t *temp_session = (session_struct_t *) get_user_session_context_from_packet(ipacket);
	if (temp_session != NULL && temp_session->app_data != NULL) {
		uint16_t * response_code = (uint16_t *) attribute->data;
		if (response_code != NULL && temp_session->app_format_id == probe_context->ftp_id) {
			((ftp_session_attr_t*) temp_session->app_data)->response_code= * response_code;
		}
		if(*response_code == 150){
			((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_sec = ipacket->p_hdr->ts.tv_sec;
			((ftp_session_attr_t*) temp_session->app_data)->file_download_starttime_usec= ipacket->p_hdr->ts.tv_usec;
		}

	}
}*/
void print_ftp_app_format(const mmt_session_t * expired_session,void *args) {
/*	int keep_direction = 1;
	session_struct_t * temp_session = get_user_session_context(expired_session);
	char message[MAX_MESS + 1];
	char path[128];
	int i;
	mmt_probe_context_t * probe_context = get_probe_context_config();
	//IP strings
	char ip_src_str[46];
	char ip_dst_str[46];
	if (temp_session->ipversion == 4) {
		inet_ntop(AF_INET, (void *) &temp_session->ipclient.ipv4, ip_src_str, INET_ADDRSTRLEN);
		inet_ntop(AF_INET, (void *) &temp_session->ipserver.ipv4, ip_dst_str, INET_ADDRSTRLEN);
		keep_direction = is_local_net(temp_session->ipclient.ipv4);
	} else {
		inet_ntop(AF_INET6, (void *) &temp_session->ipclient.ipv6, ip_src_str, INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, (void *) &temp_session->ipserver.ipv6, ip_dst_str, INET6_ADDRSTRLEN);
		keep_direction = is_localv6_net(ip_src_str);
	}
	uint32_t rtt_ms = TIMEVAL_2_MSEC(get_session_rtt(expired_session));
	proto_hierarchy_ids_to_str(get_session_protocol_hierarchy(expired_session), path);

	struct timeval init_time = get_session_init_time(expired_session);
	struct timeval end_time = get_session_last_activity_time(expired_session);
	const proto_hierarchy_t * proto_hierarchy = get_session_protocol_hierarchy(expired_session);

	for(i = 0; i < probe_context->condition_reports_nb; i++) {
		mmt_condition_report_t * condition_report = &probe_context->condition_reports[i];
		if (strcmp(condition_report->condition.condition,"FTP")==0 && ((ftp_session_attr_t*) temp_session->app_data)->file_size >1){
			snprintf(message, MAX_MESS,
					"%u,%u,\"%s\",%lu.%06lu,%"PRIu64",%"PRIu32",%lu.%06lu,%u,\"%s\",\"%s\",%hu,%hu,%hu,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%u,%u,\"%s\",%u",
					temp_session->app_format_id, probe_context->probe_id_number, probe_context->input_source, end_time.tv_sec, end_time.tv_usec,
					temp_session->session_id,temp_session->thread_number,
					init_time.tv_sec, init_time.tv_usec,
					(int) temp_session->ipversion,
					ip_dst_str, ip_src_str,
					temp_session->serverport, temp_session->clientport,(unsigned short) temp_session->proto,
					(keep_direction)?get_session_ul_packet_count(expired_session):get_session_dl_packet_count(expired_session),
							(keep_direction)?get_session_dl_packet_count(expired_session):get_session_ul_packet_count(expired_session),
									(keep_direction)?get_session_ul_byte_count(expired_session):get_session_dl_byte_count(expired_session),
											(keep_direction)?get_session_dl_byte_count(expired_session):get_session_ul_byte_count(expired_session),
													rtt_ms, get_session_retransmission_count(expired_session),
													path, proto_hierarchy->proto_path[(proto_hierarchy->len <= 16)?(proto_hierarchy->len - 1):(16 - 1)]
															     ((ftp_session_attr_t*) temp_session->app_data)->session_conn_type,
                    ((ftp_session_attr_t*) temp_session->app_data)->session_username,
                    ((ftp_session_attr_t*) temp_session->app_data)->session_password,
                    ((ftp_session_attr_t*) temp_session->app_data)->file_size,
                    ((ftp_session_attr_t*) temp_session->app_data)->filename
			);
			message[ MAX_MESS ] = '\0'; // correct end of string in case of truncated message
			if (probe_context->output_to_file_enable==1)send_message_to_file_thread (message,(void*)args);
			if (probe_context->redis_enable==1)send_message_to_redis ("ftp.flow.report", message);
		}
	}*/
}
void print_initial_ftp_report(const mmt_session_t * session,session_struct_t * temp_session, char message [MAX_MESS + 1], int valid){
	/*const proto_hierarchy_t * proto_hierarchy = get_session_protocol_hierarchy(session);
	snprintf(&message[valid], MAX_MESS-valid,
			",%u,%u,%"PRIu8",\"%s\",\"%s\",%"PRIu32",\"%s\",%"PRIu8",%"PRIu64",%"PRIu64"",
			temp_session->app_format_id,get_application_class_by_protocol_id(proto_hierarchy->proto_path[(proto_hierarchy->len <= 16)?(proto_hierarchy->len - 1):(16 - 1)]),
			((ftp_session_attr_t*) temp_session->app_data)->session_conn_type,
			(((ftp_session_attr_t*) temp_session->app_data)->session_conn_type == 2)?"null":(((ftp_session_attr_t*) temp_session->app_data)->session_username == NULL)?"null":((ftp_session_attr_t*) temp_session->app_data)->session_username,
			(((ftp_session_attr_t*) temp_session->app_data)->session_conn_type == 2)?"null":(((ftp_session_attr_t*) temp_session->app_data)->session_password == NULL)?"null":((ftp_session_attr_t*) temp_session->app_data)->session_password,
			((ftp_session_attr_t*) temp_session->app_data)->file_size,
			(((ftp_session_attr_t*) temp_session->app_data)->filename == NULL)?"null":((ftp_session_attr_t*) temp_session->app_data)->filename,
			((ftp_session_attr_t*) temp_session->app_data)->direction,
			((ftp_session_attr_t*) temp_session->app_data)->session_id_control_channel,
			((ftp_session_attr_t*) temp_session->app_data)->response_time
	);

	temp_session->session_attr->touched = 1;
	((ftp_session_attr_t*) temp_session->app_data)->ftp_throughput[0]=0;
	((ftp_session_attr_t*) temp_session->app_data)->ftp_throughput[1]=0;*/
}

