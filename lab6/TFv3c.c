/* Name: Ashley Troske
 * Date: 5/5/20
 * COEN146L - W 5:15
 * Title: Lab 5: rdt 3.0 -- Stop and Wait Client
 * Description: Client program for a stop and wait sending a file to a server on an unreliable channel with timeout. Implemented to simulate errors in sending wrong checksums or sequence numbers, or dropping packets and waiting for timeout
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

typedef struct{
  int seq_ack;
  int len;
  int cksum;
} Header;

typedef struct{
  Header header;
  char data[10];
} Packet;

//Calculates the checksum of the given packet
int getChecksum(Packet packet) {
    packet.header.cksum = 0;

    int checksum = 0;
    char *ptr = (char *)&packet;
    char *end = ptr + sizeof(Header) + packet.header.len;

    while (ptr < end) {
        checksum ^= *ptr++;
//	printf("check : %d\n",checksum);
    }

    return checksum;
}

void logPacket(Packet packet) {
    printf("Packet{ header: { seq_ack: %d, len: %d, cksum: %d }, data: \"",
            packet.header.seq_ack,
            packet.header.len,
            packet.header.cksum);
    fwrite(packet.data, (size_t)packet.header.len, 1, stdout);
    printf("\" }\n");
}



void ClientSend(int sockfd, const struct sockaddr *address, socklen_t addrlen, Packet packet, int seqnum, unsigned retries) {
   	
     while (1) {
	printf("Created: ");
	logPacket(packet);
	
	// simulate lost packet
	int num = rand() % 100;
	if (num > 70) {
		   printf("Random number: %d, greater than 70 ---- Dropping packet\n", num);
	} else {  // send the packet
	    printf("Sending packet: Seqnum %d, checksum %d\n", packet.header.seq_ack, packet.header.cksum);
	    sendto(sockfd, &packet, sizeof(packet), 0, address, addrlen);
	}	
	
	// wait until either a packet is recieved ot timeout, i.e using the select statement
	struct timeval tv;
	int rv; //select returned value
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	fd_set readfds;
	fcntl (sockfd, F_SETFL, O_NONBLOCK); 	

	FD_ZERO (&readfds);
	FD_SET (sockfd, &readfds);

	//call select
	rv = select(sockfd+1, &readfds, NULL, NULL, &tv);
	
	if(rv == 0) {
	    if(retries >= 3 && packet.header.len == 0) {
		printf("Already sent last message 3 times!\n");
		exit(1);
	    }	
	    printf("Timeout --- resend packet, there have been %d retries so far.\n\n", retries);
		retries++;
	}
	else
	{
           // receive an ACK from the server
           Packet recvpacket;
     	   recvfrom(sockfd, &recvpacket, sizeof(recvpacket), 0, NULL, NULL);
        
	   // log what was received
           printf("Received ACK %d, checksum %d - ", recvpacket.header.seq_ack, recvpacket.header.cksum);

           // validate the ACK
	   int chksum = getChecksum(recvpacket);
           if (recvpacket.header.cksum != chksum) {
               // bad checksum, resend packet
               printf("Bad checksum, expected checksum was: %d\n", chksum);
	       packet.header.seq_ack = seqnum; // reset sequence number incase wrong sequence number was sent
	       packet.header.cksum = getChecksum(packet); // re-do checksum incase packet was corrupted 
	       retries++;
           } else if (recvpacket.header.seq_ack != seqnum) {
               // incorrect sequence number, resend packet
               printf("Bad seqnum, expected sequence number was: %d\n", seqnum);
	       packet.header.seq_ack = seqnum;
	       packet.header.cksum = getChecksum(packet); 
               retries++;
           } else {
               // good ACK, we're done
               printf("Good ACK\n");
               break;
           }
	}
    }
    printf("----------\n");
    return;
}
int main(int argc, char *argv[]) {
    // check arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <infile>\n", argv[0]);
        return 1;
    }

    // seed the RNG
    srand((unsigned)time(NULL));

    // create and configure the socket [name the socket sockfd] 
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("Failure to setup an endpoint socket");
	exit(1);
     }

    // initialize the server address structure using argv[2] and argv[1]
    struct sockaddr_in address, servAddr;
    struct hostent *host;
    host = (struct hostent *)gethostbyname(argv[1]);

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[2]));
    address.sin_addr = *((struct in_addr *)host->h_addr);
    socklen_t address_len = sizeof(address);

    // open file using argv[3]
    int file;
    file = open(argv[3], O_RDONLY, O_EXCL);
    if(file < 0) {
	printf("Problem opening file\n");
	exit(1);
    }
	
    // send file contents to server
    int seqnum = 0, num, rb;
    Packet packet; //-create a packet

    while((rb = read(file, packet.data, sizeof(packet.data))) > 0) {
	packet.header.len = rb;  //-set the len field
	packet.header.seq_ack = (!seqnum);
	//Generate random numbers so 30% of the time we don't send the right sequence number, to test for errors
	num = rand() % 100;
	printf("Random number: %d\n", num);
	if(num < 70) {
		packet.header.seq_ack = seqnum;  //-set the seq_ack field 
    	}
	else{
		printf("\t Number larger than 70, sequence number corrupted.\n");
	}
	//Generate random numbers so30% of the time we don't calculate the checksum, to test for errors
	num = rand() % 100;
	printf("Random number: %d\n", num);
	if(num < 70) {
		packet.header.cksum = getChecksum(packet); //-calculate the checksum
	}
	else {
		printf("\t Number larger than 70, checksum corrupted.\n");
	}
	ClientSend(sockfd, (struct sockaddr *)&address, address_len, packet, seqnum, 0);
	seqnum = (seqnum + 1) % 2;
	printf("Changing sequence number, sending next packet...\n");
    }

    // create and prepare zero-length packet
    Packet zeropacket;
    zeropacket.header.seq_ack = seqnum;
    zeropacket.header.len = 0;
    zeropacket.header.cksum = getChecksum(zeropacket);
    // send zero-length packet to server to end connection
    ClientSend(sockfd, (struct sockaddr *)&address, address_len, zeropacket, seqnum, 0);

    // clean up
    close(file);
    close(sockfd);

    return 0;
}
