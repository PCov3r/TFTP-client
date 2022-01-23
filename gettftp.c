#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define DEFAULT_DATA_LENGTH 516
#define ACK_LENGTH 4
#define DEFAULTPORT "69"
const char delim[1] = ":";

struct timeval snd_timeout = {
    .tv_sec = 30
};
struct timeval rcv_timeout = {
    .tv_sec = 30
};

int TFTPconnect(struct addrinfo **srv_addr, char* ip_addr, char* port);
int makeRRQ(char *filename, char** rrq);
void sendRRQ(struct addrinfo *srv_addr, int sfd, char *filename);
void sendRRQ_blk(struct addrinfo *srv_addr, int sfd, char *filename, char* blk_size, int* data_size);
void receive_data(struct addrinfo *srv_addr, int sfd, char* filename, int data_size);


int main ( int argc , char * argv[] ) {
	int data_size;
	int blk_size;
	struct addrinfo *srv_addr;
	int sfd;
	char* ip;
	char* port;
	char* filename;
	
	if(argc < 3){
		printf("Error: missing argument\nUsage: ./gettftp address[:port] filename [block_size]\n");
		exit(EXIT_FAILURE);
	}
	
	
	ip = strtok(argv[1], delim); // Split first argument into ip and port
	port = strtok(NULL, delim);
	filename = argv[2];
	
	if(port == NULL){
		port = DEFAULTPORT; // If no port is given, load default port
	}
	
    sfd = TFTPconnect(&srv_addr, ip, port); // Establish connection to the server using the given arguments
	
	if(argc > 3){ // Contains block size arg
		blk_size= atoi(argv[3]);
		
		if((blk_size < 8) || (blk_size > 65464)){
			fprintf(stderr, "Error: block size must be between 8 and 65464 bytes\n");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		data_size = blk_size + 4; // DATA packet size is block size + 4
		sendRRQ_blk(srv_addr, sfd, filename, argv[3], &data_size); // RRQ with blksize flag
	} else {
		data_size = DEFAULT_DATA_LENGTH;
		sendRRQ(srv_addr, sfd, filename);
	}
	
	receive_data(srv_addr, sfd, filename, data_size); 
	
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
	/* Add timeout */
	setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &snd_timeout, sizeof(snd_timeout)); 
	setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
	
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


/* Send a read request to the server with block size option */
/* RRQ format with block option:
 +------+---~~---+---+---~~---+---+---~~---+---+---~~---+---+
 |  01  |filename| 0 |  mode  | 0 | blksize| 0 | #octets| 0 |
 +------+---~~---+---+---~~---+---+---~~---+---+---~~---+---+
*/

void sendRRQ_blk(struct addrinfo *srv_addr, int sfd, char *filename, char* blk_size, int* data_size){
	char* rrq;
	int rrq_length;
	char* RRQ_response;
	char ACK_packet[ACK_LENGTH] = {0, 4, 0, 0};
	int received_size;
	
	RRQ_response = malloc(DEFAULT_DATA_LENGTH*sizeof(char)); 
	
	rrq_length = makeRRQ(filename, &rrq); // Form a RRQ
	rrq = realloc(rrq, rrq_length+(strlen(blk_size)+9)*sizeof(char)); // Add memory for blk option
	memset(rrq+rrq_length, 0, (strlen(blk_size)+9)*sizeof(char));
	
	strcpy(rrq+rrq_length,"blksize"); /* blk flag */
	rrq_length += 8*sizeof(char);
	strcpy(rrq+rrq_length,blk_size); /* blk size */
	rrq_length += (strlen(blk_size)+1)*sizeof(char);
	

	if(sendto(sfd, rrq, rrq_length, 0, (struct sockaddr *) srv_addr->ai_addr, srv_addr->ai_addrlen) == -1){ // Send RRQ
			perror("Unable to send data");
			close(sfd);
			exit(EXIT_FAILURE);
	}
	free(rrq);
	
	if((received_size = recvfrom(sfd,RRQ_response,DEFAULT_DATA_LENGTH,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive response packet into buffer
			perror("Unable to receive ack");
			close(sfd);
			exit(EXIT_FAILURE);
	}
	
	if(RRQ_response[0] == 0 && RRQ_response[1] == 6){ // Is OACK packet
		if(strncmp(RRQ_response+3+strlen("blksize"), blk_size, strlen(blk_size))){ // Check if block size option has been accepted
			fprintf(stderr,"Blocksize has been refused by server\n");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		if(sendto(sfd,ACK_packet,ACK_LENGTH,0,srv_addr->ai_addr,srv_addr->ai_addrlen) == -1){ // Send ACK packet to confirm block size
			perror("Could not send ACK");
			close(sfd);
			exit(EXIT_FAILURE);
		}
	}
	else if(RRQ_response[0]==0 && RRQ_response[1]==3){ // Is DATA packet
		fprintf(stderr, "*** ERROR: Block size option may not have been implemented in the server ***\n");
		*data_size = DEFAULT_DATA_LENGTH;
		fprintf(stdout, "Ready to receive file with block size of 512 bytes\n");	
		fprintf(stdout, "Please wait as this may take a few seconds\n\n");	
		/* Flawed implementation: we need to wait for server to send first packet again */
	}
	else if(RRQ_response[0]==0 && RRQ_response[1]==5){ // Check for error packet
		fprintf(stderr,"Received error packet with code %d%d\n",RRQ_response[2],RRQ_response[3]);
		fwrite(RRQ_response+4, sizeof(char),received_size-5, stdout);  // Print error packet message
		close(sfd);
		exit(EXIT_FAILURE);
	}
	else {
		fprintf(stderr, "Blocksize option has been refused or is not implemented on the server\n");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	
	free(RRQ_response);
}
	
	


/* Receive the data from the read request */
/* DATA PACKET format:
 2 bytes     2 bytes      n bytes
 ----------------------------------
| Opcode |   Block #  |   Data     |
 ----------------------------------
 */

void receive_data(struct addrinfo *srv_addr, int sfd, char* filename, int data_size){
	int received_size;
	int written_count;
	FILE *file;
	char* DATA_packet;
	char ACK_packet[ACK_LENGTH] = {0, 4, 0, 0};
	
	DATA_packet = malloc(data_size*sizeof(char)); 
	file = fopen(filename, "w"); // Open the file in write mode / create if it does not exist
	
	if(file == NULL){
		perror("Could not open specified file");
		close(sfd);
		exit(EXIT_FAILURE);
	}
	
	/* Receive file packet after packet */
	do{
	
		if((received_size = recvfrom(sfd,DATA_packet,data_size,0,srv_addr->ai_addr,&srv_addr->ai_addrlen)) == -1){ // Receive packet into buffer
			perror("Unable to receive data");
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
			perror("Could not write data into file");
			close(sfd);
			exit(EXIT_FAILURE);
		}
		
		ACK_packet[2]=DATA_packet[2]; // Build ACK packet: add block n°
		ACK_packet[3]=DATA_packet[3];
		
		if(sendto(sfd,ACK_packet,ACK_LENGTH,0,srv_addr->ai_addr,srv_addr->ai_addrlen) == -1){ // Send ACK packet
			perror("Could not send ACK");
		}
		
		fprintf(stdout,"Successfully received packet n°%d%d\n",ACK_packet[2],ACK_packet[3]);
		
	}while(received_size==data_size); // While there is still incoming data (ie the DATA packet has full size)
	
	free(DATA_packet);
	fclose(file);
	
	fprintf(stdout, "\n*** Successfully received entire file ***\n");
	
}

