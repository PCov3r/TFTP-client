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
int makeWRQ(char *filename, char** wrq);
void sendWRQ(struct addrinfo *srv_addr, int sfd, char *filename);
void send_data(struct addrinfo *srv_addr, int sfd, char* filename);


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
	
	sendWRQ(srv_addr, sfd, argv[2]);
	
	send_data(srv_addr, sfd, argv[2]);
	
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


/* Make a WRQ */
/* WRQ format:
 2 bytes     string    1 byte     string   1 byte
 ------------------------------------------------
|   02   |  Filename  |   0  |    Mode    |   0  |
 ------------------------------------------------ 
*/

int makeWRQ(char *filename, char** wrq){
	char *trame;
	int wrq_length = (9+strlen(filename))*sizeof(char);
	trame = malloc(wrq_length);
	memset(trame, 0, sizeof(*trame)); 
	
	trame[0]=0;trame[1]=2; //OPcode
	strcpy(trame+2,filename); // Filename
	strcpy(trame+3+strlen(filename),"octet"); // Mode
	
	*wrq = trame;
	
	return wrq_length;
}


/* Send a write request to the server */

void sendWRQ(struct addrinfo *srv_addr, int sfd, char *filename){
	char* wrq;
	int wrq_length;
	char WRQ_response[DATA_LENGTH];
	int received_size;
	
	wrq_length = makeWRQ(filename, &wrq); // Form a WRQ

	if(sendto(sfd, wrq, wrq_length, 0, (struct sockaddr *) srv_addr->ai_addr, srv_addr->ai_addrlen) == -1){ // Send WRQ
		perror("Unable to send data");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	free(wrq);
	
	if((received_size = recvfrom(sfd,WRQ_response,DATA_LENGTH,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive packet # into buffer
		perror("Unable to receive ack:");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	
	if(WRQ_response[0] == 0 && WRQ_response[1] == 4 && WRQ_response[2] == 0 && WRQ_response[3] == 0){ // Check if WRQ accepted ie the ACK has Block # = 0
		fprintf(stdout, "Ready to send file\n");
	} else {
		fprintf(stderr, "Wrong ack received, something went wrong\n");
		close(sfd);
		exit(EXIT_FAILURE);
	}
}


/* Send the data from the file */
/* DATA PACKET format:
 2 bytes     2 bytes      n bytes
 ----------------------------------
|   03   |   Block #  |   Data     |
 ----------------------------------
 */

void send_data(struct addrinfo *srv_addr, int sfd, char* filename){
	int read_size;
	int received_size;
	int packet_count;
	FILE *file;
	char* DATA_packet;
	char* ACK_packet;
	
	DATA_packet = malloc(DATA_LENGTH*sizeof(char)); 
	ACK_packet = malloc(DATA_LENGTH*sizeof(char)); 
	
	file = fopen(filename, "r"); // Open file in read mode
	
	if(file == NULL){
		perror("Could not open specified file:");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	
	packet_count = 1;
	
	/* Send data packet after packet */
	
	while((read_size=fread(DATA_packet+4, sizeof(char),DATA_LENGTH-4,file))>0){
		
		DATA_packet[0] = 0; DATA_packet[1] = 3; // OP Code
		/* Add packet number */
		DATA_packet[2] = ((packet_count >> 8) & 0xff); // High byte from integer packet number
		DATA_packet[3] = (packet_count & 0xff); // Low byte from integer packet number

		
		if(sendto(sfd,DATA_packet,read_size+4,0,srv_addr->ai_addr,srv_addr->ai_addrlen) == -1){ // Send packet to server
			fprintf(stderr,"Couldn't send data\n");
		}
		
		if((received_size = recvfrom(sfd,ACK_packet,DATA_LENGTH,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive ack packet into buffer
			perror("Unable to receive ack:");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		if(ACK_packet[0]==0 && ACK_packet[1]==5){ // Check for error packet
			fprintf(stderr,"Received error packet with code %d%d\n",ACK_packet[2],ACK_packet[3]);
			fwrite(ACK_packet+4, sizeof(char),received_size-5, stdout);  // Print error packet message
			close(sfd);
			exit(EXIT_FAILURE);
		}

		printf("Successfully sent packet n°%d%d\n",ACK_packet[2],ACK_packet[3]);
		
		packet_count++;
		
	}
	
	free(DATA_packet);
	free(ACK_packet);
	fclose(file);
	
}


