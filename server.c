#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "main.h"

void handle_RRQ(int sockfd, char *msg, struct sockaddr_in client_addr, socklen_t len){
char ack_buffer[BUFFER_SIZE];
char *filename = msg + 2;
int n, block = 1;

	
	//strncpy(filename, msg+2, strlen(msg+2));
	printf("Filename requested is %s\n", filename );

	FILE *fp = fopen(filename, "r");

	if (fp  == NULL){
//send error file not found to client
		char err[BUFFER_SIZE];
		char *err_msg = "File not found";
		int err_msg_len = strlen(err_msg);
		err[0] = 0;
		err[1] = ERROR;
		err[2] = 0;
		err[3] = FILENOTFOUND;
		strncpy(err + 4, err_msg, err_msg_len);
		err[err_msg_len + 4] = 0;
		if(sendto(sockfd, (const void *)err, err_msg_len + 5, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
		perror("unable to send reply to client\n");
		exit(EXIT_FAILURE);
		}
	return;
	}
	block = 1;
	memset(ack_buffer, 0, BUFFER_SIZE);
	while (( n = fread(msg + 4, 1, BLOCK_SIZE, fp)) > 0){
	
		msg[0] = 0;
		msg[1] = DATA;
		msg[2] = (block >> 8)&&0xFF;
		msg[3] = block && 0xFF;
		if(sendto(sockfd, (const void *)msg, n + 4, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
	perror("unable to send reply to client\n");
	exit(EXIT_FAILURE);
		}
		printf("Sent the block %d\n ", block);
		int bytes = recvfrom(sockfd, (char *)ack_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
		if ((bytes  > 0 ) && (ack_buffer[1] == ACK) && (ack_buffer[2] << 8 | ack_buffer[3]) == block){
			printf("received ACK for block %d \n" , block);
			block++;
			
		}
		
		
	}
	fclose(fp);
	
}
void handle_WRQ(){}
void handle_ERR(){}
void handle_ACK(){}

int main(int argc, char **argv){
int sock_dg;
socklen_t len;
char *server = "server";
char *client = "client";

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
		handle_WRQ();
		break;

	}

    } 
close(sock_dg);
return 0;
}
