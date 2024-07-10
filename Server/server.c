#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "main.h"
#include <time.h>
void send_ACK(int sockfd, struct sockaddr_in client_addr, socklen_t len, int block){
	char  * init_ack_buffer = (char *) malloc(ACK_BUFF_SIZE * sizeof(char));
	init_ack_buffer[0] = 0;
	init_ack_buffer[1] = ACK;
	init_ack_buffer[2] = (block >> 8 & 0xFF);
	init_ack_buffer[3] = block & 0xFF;
	if(sendto(sockfd, (const void *)init_ack_buffer, 4, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
		perror("unable to send reply to client\n");
		exit(EXIT_FAILURE);
	}
	printf("Sent ACK for block %d\n", block);
	free(init_ack_buffer);
	
}
void send_error(int sockfd, struct sockaddr_in client_addr, socklen_t len, int errorcode, const char * message ){

		char err[BUFFER_SIZE];
		int err_msg_len = strlen(message);
		err[0] = 0;
		err[1] = ERROR;
		err[2] = 0;
		err[3] = errorcode;
		strncpy(err + 4, message, err_msg_len);
		err[err_msg_len + 4] = 0;
		if(sendto(sockfd, (const void *)err, err_msg_len + 5, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
		perror("unable to send reply to client\n");
		exit(EXIT_FAILURE);
		}

}
void handle_RRQ(int sockfd, char *msg, struct sockaddr_in client_addr, socklen_t len){
char ack_buffer[BUFFER_SIZE];
char *filename = msg + 2;
int n, block = 1, bytes;
struct timeval tv;
fd_set readfds;
int attempt = 0;
	
	//strncpy(filename, msg+2, strlen(msg+2));
	printf("Filename requested is %s\n", filename );

	FILE *fp = fopen(filename, "r");

	if (fp  == NULL){
//send error file not found to client
		send_error(sockfd, client_addr, len, FILENOTFOUND, "File Not Found");
	return;
	}
	block = 1;
	memset(ack_buffer, 0, BUFFER_SIZE);
	while (( n = fread(msg + 4, 1, BLOCK_SIZE, fp)) > 0){
		printf("Value of block is :%d\n", block);
		msg[0] = 0;
		msg[1] = DATA;
		msg[2] = (block >> 8) & 0xFF;
		msg[3] = block & 0xFF;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		while (attempt < 5){
		sendto(sockfd, (const void *)msg, n + 4, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr));
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		printf("Sent the block %d\n ", block);
		
		int s = select(sockfd + 1, &readfds, NULL, NULL, &tv);
		
		if (s > 0){
		bytes =recvfrom(sockfd, (char *)ack_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
		printf("Received %d bytes from client. fields are %x %x %x \n", bytes, ack_buffer[1], ack_buffer[2], ack_buffer[3]);
		if ((bytes  > 0 ) && (ack_buffer[1] == ACK) && (ack_buffer[2] << 8 | ack_buffer[3]) == block){
			printf("received ACK of size %d  bytes for block %d \n" , bytes,block);
			block++;
			break;
			
				}
		
			}
		printf("Did not receive ACK from client for attempt : %d \n", attempt);
		attempt++;
		}
		if (attempt == 5){
			printf("Failed to receive ACK for block %d after %d attempts", block, attempt);
			fclose(fp);
			return;
		}
	}
	fclose(fp);
	
}
void handle_WRQ(int sockfd, char *msg, struct sockaddr_in client_addr, socklen_t len){

char *filename = msg + 2;
int n, block = 0, bytes;
struct timeval tv;
fd_set readfds;
int attempt = 0;

printf("Filename to be written %s", filename);

FILE *fp = fopen(filename, "w");

if (fp == NULL){
	send_error(sockfd, client_addr, len, UNDEFINED,"Error creating file");
	return;
}

/*Once the file is created and permissions set. Now send an ACK to client with BLOCK = 0*/

send_ACK(sockfd, client_addr, len, block);

/*Now get ready to receive the DATA blocks from client*/

memset(msg, 0, sizeof(msg));
FD_ZERO(&readfds);
FD_SET(sockfd, &readfds);

tv.tv_sec = 2;
tv.tv_usec = 0;

do {

	int s = select(sockfd+1, &readfds, NULL, NULL, &tv);

	if(s > 0){
	bytes = recvfrom(sockfd, (char *)msg, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
		
		if(msg[1] == DATA){
			block = (msg[2] << 8 | msg[3]);
			printf("Received block %d from client. \n", block);
			fwrite(msg + 4, 1, BLOCK_SIZE, fp);
			send_ACK(sockfd, client_addr, len, block);
			if(bytes < BUFFER_SIZE)
				{
					break;
				}
			
		}else{
			printf("received unexpected header %d", msg[1]);
		}
	}else{
		//handle scenario where s < 0 after timeout
		printf("Timed out receiving the Block of data \n");
		break;
	}

}while(1);


fclose(fp);

return ;


}


int main(int argc, char **argv){
int sock_dg;
socklen_t len;

	printf("Welcome to TFTP server !!! \n");
	struct sockaddr_in serv_addr, client_addr;
	char msg[BUFFER_SIZE];
	char *serv_reply = "hey hi";
	if ((sock_dg = socket(AF_INET, SOCK_DGRAM,0)) < 0){
		perror("Failed to Create socket");
		exit(EXIT_FAILURE);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	memset(&client_addr, 0, sizeof(client_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	if(bind(sock_dg, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Error trying to bind the socket");
		exit(EXIT_FAILURE);
	}
	while(1){

	printf("Waiting for  RRQ packet from client\n");

	len  = sizeof(client_addr);
	memset(msg,0, BUFFER_SIZE);
	int bytes = recvfrom(sock_dg, (char *)msg, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);

	if (bytes < 0){
		perror("Error receiving message from client\n");
		exit(EXIT_FAILURE);
	}

/*parsing the packet received from client*/

	switch(msg[1]){

	case RRQ :
		handle_RRQ(sock_dg, msg, client_addr, len);
		break;
	case WRQ : 
		printf("packet contents : %d %d %s",msg[0], msg[1], msg + 2);
		handle_WRQ(sock_dg, msg, client_addr, len);
		break;

	}

    } 
close(sock_dg);
return 0;
}
