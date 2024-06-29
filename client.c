#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "main.h"

int main(){
int sock_dg;
socklen_t len;

printf("this is client\n");
struct sockaddr_in serv_addr;
char *filename = "test.txt";
char *mode = "netascii";
int mode_len = strlen(mode);
int file_len = strlen(filename);
char buffer[BUFFER_SIZE];
if ((sock_dg = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
perror("unable to create socket\n");
exit(EXIT_FAILURE);
}

memset(&serv_addr, 0 , sizeof(serv_addr));
serv_addr.sin_family = AF_INET;

serv_addr.sin_port = htons(PORT);

inet_pton(AF_INET, "127.0.0.1",&(serv_addr.sin_addr));

/*Declare the File descritor*/
FILE *fp = fopen("received.txt", "w");
/*Send RRQ to server*/
buffer[0] = 0;
buffer[1] = RRQ;
strncpy(buffer + 2, filename, file_len);
buffer[file_len + 2] = 0;
strncpy(buffer + file_len + 3, mode,mode_len);
buffer[file_len + mode_len + 3] = 0;


if(sendto(sock_dg, (const char *) buffer,file_len + mode_len + 3 , 0, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
perror("Error while sending RRQ to server \n");
exit(EXIT_FAILURE);
}
printf("Sent RRQ message to server\n");

int recv_bytes, block;
char recv_buffer[BUFFER_SIZE];
memset(buffer, 0, BUFFER_SIZE);
do {
if ((recv_bytes = recvfrom(sock_dg, (char *)recv_buffer, BUFFER_SIZE,0, NULL, NULL)) < 0){
perror("unable to receive DATA \n");
exit(EXIT_FAILURE);
}
printf("Received %d bytes from server \n", recv_bytes);
if(recv_buffer[1] == DATA){
	block = (recv_buffer[2] << 8 | recv_buffer[3]);
	printf("Block number received %d \n", block);
	fwrite(recv_buffer + 4, recv_bytes, 1, fp);
	//Send ACK
	buffer[0] = 0;
	buffer[1] = ACK;
	buffer[2] = (block >> 8) && 0xFF;
	buffer[3] = (block) && 0xFF;
	if(sendto(sock_dg, (const char *) buffer,4, 0, 		(const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Error while sending ACK \n");
		exit(EXIT_FAILURE);
	}
	printf("Sent ACK for block : %d \n", block);
	if(recv_bytes < BUFFER_SIZE){
		fclose(fp);
		break;
	}
	memset(recv_buffer, 0, BUFFER_SIZE);	
}

}while (recv_bytes >= BUFFER_SIZE);

return 0;
}
