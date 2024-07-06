#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "main.h"
#include <time.h>
void handle_WRQ(int sockfd, struct sockaddr_in serv_addr ,socklen_t len, FILE *fp, char *filename){

/*Init variables*/
int recv_bytes, block = 1;
char recv_buffer[BUFFER_SIZE], buffer[BUFFER_SIZE];
memset(buffer, 0, BUFFER_SIZE);
memset(recv_buffer, 0, BUFFER_SIZE);

fd_set readfds;
struct timeval tv;
char *mode = "netascii";
int mode_len = strlen(mode);
int file_len = strlen(filename);

FD_ZERO(&readfds);
FD_SET(sockfd, &readfds);

tv.tv_sec = 2;
tv.tv_usec = 0;

/*Preparing  RRQ or WRQ request to server*/
buffer[0] = 0;
buffer[1] = WRQ;
strncpy(buffer + 2, filename, file_len);
buffer[file_len + 2] = 0;
strncpy(buffer + file_len + 3, mode,mode_len);
buffer[file_len + mode_len + 3] = 0;

/*Sending WRQ to the server*/
if(sendto(sockfd, (const char *) buffer,file_len + mode_len + 3 , 0, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
perror("Error while sending request to server \n");
exit(EXIT_FAILURE);
}
printf("Sent request message to server\n");

/*Wait for the first ACK with block=0;*/
while (1) {
	int s = select(sockfd+1, &readfds, NULL, NULL, &tv);

	if(s > 0){
		recv_bytes = recvfrom(sockfd, (char *)recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, &len);
		if(recv_buffer[1] == ACK){
			printf("Received initial ACK from Server %d \n", recv_bytes);
		
			block = (recv_buffer[3] << 8 | recv_buffer[4]);
		
			if(block == 0){
				break;
			}
		}
		
	}

}

/*Once initial ACK from server received. Proceed to read from file and send over data*/
block++;
int n;
while ((n = fread(buffer + 4, 1, BLOCK_SIZE, fp)) > 0){
	buffer[0] = 0;
	buffer[1] = DATA;
	buffer[2] = (block >> 8 ) & 0xFF;
	buffer[3] = (block & 0xFF);
	if(sendto(sockfd, (const char *) buffer,n + 4, 0,	(const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Error while sending request to server \n");
		exit(EXIT_FAILURE);
	}
	int s = select(sockfd + 1, &readfds, NULL, NULL, &tv);
	if (s > 0){
		recv_bytes = recvfrom(sockfd, (char *)recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, &len);
		if (recv_buffer[1] == ACK && (recv_buffer[3] << 8 | recv_buffer[4]) == block){
			printf("ACK received for block %d \n", block);	
		}
	}else{
		printf("Timed out waiting for server ACK for block %d\n", block);
		return ;
	}

}

fclose(fp);

}
void handle_RRQ(int sockfd, struct sockaddr_in serv_addr ,socklen_t len, FILE *fp, char *filename){

/*Init variables*/

int recv_bytes, block = 0;
char recv_buffer[BUFFER_SIZE], buffer[BUFFER_SIZE];
memset(buffer, 0, BUFFER_SIZE);
memset(recv_buffer, 0, BUFFER_SIZE);

fd_set readfds;
struct timeval tv;
char *mode = "netascii";
int mode_len = strlen(mode);
int file_len = strlen(filename);


/*Preparing  RRQ or WRQ request to server*/
buffer[0] = 0;
buffer[1] = RRQ;
strncpy(buffer + 2, filename, file_len);
buffer[file_len + 2] = 0;
strncpy(buffer + file_len + 3, mode,mode_len);
buffer[file_len + mode_len + 3] = 0;


if(sendto(sockfd, (const char *) buffer,file_len + mode_len + 3 , 0, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
perror("Error while sending request to server \n");
exit(EXIT_FAILURE);
}
printf("Sent request message to server\n");

FD_ZERO(&readfds);

FD_SET(sockfd, &readfds);

tv.tv_usec = 0;
tv.tv_sec = 2;


do {
int s = select(sockfd+1, &readfds, NULL, NULL, &tv);
if (s > 0){
recv_bytes = recvfrom(sockfd, (char *)recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, &len);

printf("Received %d bytes from server \n", recv_bytes);
if(recv_buffer[1] == DATA){
	block = (recv_buffer[2] << 8 | recv_buffer[3]);
	printf("Block number received %d \n", block);
	fwrite(recv_buffer + 4, recv_bytes, 1, fp);
	//Send ACK
	buffer[0] = 0;
	buffer[1] = ACK;
	buffer[2] = (block >> 8) & 0xFF;
	buffer[3] = block & 0xFF;
	if(sendto(sockfd, (const char *) buffer,4, 0, 		(const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Error while sending ACK \n");
		exit(EXIT_FAILURE);
	}
	printf("Sent ACK for block : %d \n", block);
	if(recv_bytes < BUFFER_SIZE){
		break;
	}
		
}else if (recv_buffer[1] == ERROR){
	char *err_msg = recv_buffer + 4;
	int err_code = (recv_buffer[2] << 8 | recv_buffer[3]);
	printf("ERROR received from the server - Code : %d, Reason : %s", err_code, err_msg);
	break;
}
} else {
printf("Timed out waiting for server info");
}
}while (1);

fclose(fp);
FD_ZERO(&readfds);
}
int main(int argc, char **argv){
int sock_dg;
socklen_t len;

printf("this is client\n");
struct sockaddr_in serv_addr;

/*Declare the File descritor*/
FILE *fp = NULL;
char *read_file = "test.txt";
char *write_file = "send.txt";
if (strcmp(argv[1], "get") == 0){
	
	
	fp = fopen(read_file, "w");
	if (fp == NULL){
		printf("Unable to create file %s\n", read_file);
		return 0;
		}
		
	
} else if(strcmp(argv[1], "put") == 0){
	
	fp = fopen(write_file, "r");
	if (fp == NULL){
		printf("No file name %s present \n", write_file);
		return 0;
		}
	
}


if ((sock_dg = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
perror("unable to create socket\n");
exit(EXIT_FAILURE);
}

memset(&serv_addr, 0 , sizeof(serv_addr));
serv_addr.sin_family = AF_INET;

serv_addr.sin_port = htons(PORT);

inet_pton(AF_INET, "127.0.0.1",&(serv_addr.sin_addr));




if (strcmp(argv[1],"get") == 0){
	handle_RRQ(sock_dg, serv_addr,sizeof(serv_addr) ,fp, read_file);
}else if (strcmp(argv[1], "put") == 0){
	handle_WRQ(sock_dg, serv_addr, sizeof(serv_addr), fp, write_file);
}

return 0;
}
