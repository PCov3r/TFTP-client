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
		printf("Error: missing argument\nUsage: ./gettftp address[:port] filename\n");
		exit(EXIT_FAILURE);
	}
	
	ip = strtok(argv[1], delim); // Split first argument into ip and port
	port = strtok(NULL, delim);
	
	if(port == NULL){
		port = DEFAULTPORT; // If no port is given, load default port
	}
	
    sfd = TFTPconnect(&srv_addr, ip, port); // Establish connection to the server using the given arguments
	
	sendRRQ(srv_addr, sfd, argv[2]);
	
	receive_data(srv_addr, sfd, argv[2]); 
	
	free(srv_addr);
	close(sfd);
    
	exit(EXIT_SUCCESS);
}


/* Connect to the TFTP server */

int TFTPconnect(struct addrinfo **srv_addr, char* ip_addr, char* port){
	
	int sfd, sock;
	struct addrinfo hints = {0};
	
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* IPv4 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket: excludes TCP */
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_UDP; /* UDP */
	
	sock = getaddrinfo(ip_addr, port, &hints, srv_addr); /* Obtain server infos */
	if (sock != 0) {
		fprintf(stderr, "Could not obtain server informations: %s\n", gai_strerror(sock));
		exit(EXIT_FAILURE);
    }
    
    sfd = socket((*srv_addr)->ai_family,(*srv_addr)->ai_socktype,(*srv_addr)->ai_protocol); /* Open a socket to the given server */
	if (sfd == -1) {
		fprintf(stderr,"erreur");
		exit(EXIT_FAILURE);
	}
	
	return sfd;
}


/* Make a RRQ */
/* RRQ format:
 2 bytes     string    1 byte     string   1 byte
 ------------------------------------------------
|   01   |  Filename  |   0  |    Mode    |   0  |
 ------------------------------------------------ 
*/


int makeRRQ(char *filename, char** rrq){
	
	char *trame;
	int rrq_length = (9+strlen(filename))*sizeof(char); 
	trame = malloc(rrq_length);
	memset(trame, 0, sizeof(*trame)); 
	
	trame[0]=0;trame[1]=1; //OPcode
	strcpy(trame+2,filename); // Filename
	strcpy(trame+3+strlen(filename),"octet"); // Mode
	
	*rrq = trame;
	
	return rrq_length;
}


/* Send a read request to the server */

void sendRRQ(struct addrinfo *srv_addr, int sfd, char *filename){
	char* rrq;
	int rrq_length;
	
	rrq_length = makeRRQ(filename, &rrq); // Form a RRQ

	if(sendto(sfd, rrq, rrq_length, 0, (struct sockaddr *) srv_addr->ai_addr, srv_addr->ai_addrlen) == -1){ // Send the RRQ
			perror("Unable to send data");
			close(sfd);
			exit(EXIT_FAILURE);
	}
	free(rrq);
}


/* Receive the data from the read request */
/* DATA PACKET format:
 2 bytes     2 bytes      n bytes
 ----------------------------------
|   03   |   Block #  |   Data     |
 ----------------------------------
 */

void receive_data(struct addrinfo *srv_addr, int sfd, char* filename){
	int received_size;
	int written_count;
	FILE *file;
	char*   DATA_packet;
	char ACK_packet[ACK_LENGTH] = {0, 4, 0, 0};
	
	DATA_packet = malloc(DATA_LENGTH*sizeof(char)); 
	file = fopen(filename, "w"); // Open the file in write mode / create if it does not exist
	
	if(file == NULL){
		perror("Could not open specified file:");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	
	/* Receive file packet after packet */
	do{
	
		if((received_size = recvfrom(sfd,DATA_packet,DATA_LENGTH,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive packet into buffer
			perror("Unable to receive data:");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		if(DATA_packet[0]==0 && DATA_packet[1]==5){ // Check for error packet
			fprintf(stderr,"Received error packet with code %d%d\n",DATA_packet[2],DATA_packet[3]);
			fwrite(DATA_packet+4, sizeof(char),received_size-5, stdout);  // Print error packet message
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		written_count = fwrite(DATA_packet+4, sizeof(char), received_size-4,file); // Write the data part from data_packet into file 
		if(written_count != received_size-4){ 
			perror("Could not write data into file:");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		ACK_packet[2]=DATA_packet[2]; // Build ACK packet: add block #
		ACK_packet[3]=DATA_packet[3];
		
		if(sendto(sfd,ACK_packet,ACK_LENGTH,0,srv_addr->ai_addr,srv_addr->ai_addrlen) == -1){ // Send ACK packet
			perror("Could not send ACK:");
		}
		
	}while(received_size==DATA_LENGTH); // While there is still incoming data (ie the DATA packet has full size)
	
	free(DATA_packet);
	fclose(file);
	
}

