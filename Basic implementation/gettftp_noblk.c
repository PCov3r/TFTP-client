#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define DATA_LENGTH 516
#define ACK_LENGTH 4
#define DEFAULTPORT "69"
const char delim[1] = ":";

int TFTPconnect(struct addrinfo **srv_addr, char* ip_addr, char* port);
int makeRRQ(char *filename, char** rrq);
void sendRRQ(struct addrinfo *srv_addr, int sfd, char *filename);
void receive_data(struct addrinfo *srv_addr, int sfd, char* filename);


int main ( int argc , char * argv[] ) {
	struct addrinfo *srv_addr;
	int sfd;
	char* ip;
	char* port;
	
	if(argc != 3){
		printf("Error: missing argument\nUsage: ./gettftp [address] [filename]\n");
		exit(EXIT_FAILURE);
	}
	
	ip = strtok(argv[1], delim);
	port = strtok(NULL, delim);
	
	if(port == NULL){
		port = DEFAULTPORT;
	}
	
    sfd = TFTPconnect(&srv_addr, ip, port);
	
	sendRRQ(srv_addr, sfd, argv[2]);
	
	receive_data(srv_addr, sfd, argv[2]);
	
	free(srv_addr);
	close(sfd);
    
	exit(EXIT_SUCCESS);
}

int TFTPconnect(struct addrinfo **srv_addr, char* ip_addr, char* port){
	
	int sfd, sock;
	struct addrinfo hints = {0};
	
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket: excludes TCP */
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_UDP;          /* UDP */
	
	sock = getaddrinfo(ip_addr, port, &hints, srv_addr);
	if (sock != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(sock));
		exit(EXIT_FAILURE);
    }
    
    sfd = socket((*srv_addr)->ai_family,(*srv_addr)->ai_socktype,(*srv_addr)->ai_protocol);
	if (sfd == -1) {
		fprintf(stderr,"erreur");
		exit(EXIT_FAILURE);
	}
	
	return sfd;
}


int makeRRQ(char *filename, char** rrq){
	char *trame;
	int rrq_length = (9+strlen(filename))*sizeof(char);
	trame = malloc(rrq_length);
	memset(trame, 0, sizeof(*trame)); 
	
	trame[0]=0;trame[1]=1; //OPcode
	strcpy(trame+2,filename);
	strcpy(trame+3+strlen(filename),"octet");
	
	*rrq = trame;
	
	return rrq_length;
}

void sendRRQ(struct addrinfo *srv_addr, int sfd, char *filename){
	char* rrq;
	int rrq_length;
	
	rrq_length = makeRRQ(filename, &rrq);

	if(sendto(sfd, rrq, rrq_length, 0, (struct sockaddr *) srv_addr->ai_addr, srv_addr->ai_addrlen) == -1){
			perror("CLIENT: sendto");
			exit(EXIT_FAILURE);
	}
	free(rrq);
}


void receive_data(struct addrinfo *srv_addr, int sfd, char* filename){
	int received_size;
	int written_count;
	FILE *file;
	char*   DATA_packet;
	char ACK_packet[ACK_LENGTH] = {0, 4, 0, 0};
	
	DATA_packet = malloc(DATA_LENGTH*sizeof(char)); 
	file = fopen(filename, "w");
	
	if(file == NULL){
		perror("Could not open specified file");
		exit(EXIT_FAILURE);
	}
	
	do{
	
		if((received_size = recvfrom(sfd,DATA_packet,DATA_LENGTH,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive packet into buffer
			perror("Unable to receive data:");
			exit(EXIT_FAILURE);
		}
		
		if(DATA_packet[0]=='0' && DATA_packet[1]=='5'){
			printf("Received error packet\n");
			fwrite(DATA_packet+4, sizeof(char),received_size-5, stdout);  // Print error packet message
			exit(EXIT_FAILURE);
		}
		
		written_count = fwrite(DATA_packet+4,sizeof(char),received_size-4,file);
		if(written_count != received_size-4){ // Write received packet to file
			perror("Could not write data into file");
			exit(EXIT_FAILURE);
		}
		
		ACK_packet[2]=DATA_packet[2]; // Build ACK packet
		ACK_packet[3]=DATA_packet[3];
		
		if(sendto(sfd,ACK_packet,ACK_LENGTH,0,srv_addr->ai_addr,srv_addr->ai_addrlen) == -1){ // Send ACK packet
			perror("Could not send ACK");
		}
		
	}while(received_size==DATA_LENGTH);
	
	free(DATA_packet);
	fclose(file);

}

