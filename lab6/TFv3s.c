/* Name: Ashley Troske
 * Date: 5/5/20
 * COEN146L - W 5:15
 * Title: Lab 6: rdt 3.0 --  Stop and Wait Server
 * Description: Server program for a stop and wait sending on an unreliable channel, server waits for connection with client who sends a file in packets. Made to handle errors in recieving corrupt packets (with wrong checksum or sequence number) or dropping packets and waiting for timeout. 
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

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
        checksum ^= *ptr++; // increment pointer, then derefence pointer for int value. Before we were trying to xor a pointer and int value
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

//Sending a ACK to the client, i.e., letting the client know what was the status of the packet it sent.
void ServerSend(int sockfd, const struct sockaddr *address, socklen_t addrlen, int seqnum) {
    Packet packet;

    //-Prepare the packet header: sequence number
    int num = rand() % 100;
    printf("Random number: %d, If greater than 70 seq number is corrupted\n", num);
    if(num < 70) {
    	packet.header.seq_ack = seqnum;
    }

    //-Prepare the packet header: length
    packet.header.len = 0; //ACK packet

    //-Prepare the packet header: checksum
    num = rand() % 100;
    printf("Random number: %d, If greater than 70 checksum is corrupted\n", num);
    if(num < 70) { 
   	packet.header.cksum = getChecksum(packet); 
    }

    //Simulates dropped ACK packet
    num = rand() % 100;
    if(num < 70) { 
    	sendto(sockfd, &packet, sizeof(packet), 0, address, addrlen); 	
        printf("Sent ACK %d, checksum %d\n", packet.header.seq_ack, packet.header.cksum);
    }
    else {
	printf("Random number: %d, greater than 70 ---- Dropping packet\n", num);
    } 
}

Packet ServerReceive(int sockfd, struct sockaddr *address, socklen_t *addrlen, int seqnum) {
    Packet packet;
    while (1) {
        //recv packets from the client
	recvfrom(sockfd, &packet, sizeof(packet), 0, address, addrlen);
        // log what was received
        printf("Received: ");
        logPacket(packet);

	int num = rand() % 100; 
	
	//find packet checksum
	int chksum = getChecksum(packet);

        if (packet.header.cksum != chksum) {
            printf("Bad checksum, expected checksum was: %d\n", chksum);
	    printf("Checksum recieved: %d\n", packet.header.cksum);
            ServerSend(sockfd, address, *addrlen, !seqnum);
        }
	else if (packet.header.seq_ack != seqnum) {
            printf("Bad seqnum, expected sequence number was:%d\n", seqnum);
            ServerSend(sockfd, address, *addrlen, !seqnum);
        }
	else {
            printf("Good packet\n");
            ServerSend(sockfd, address, *addrlen, seqnum);
            break;
        }
    }

    printf("\n");
    return packet;
}


int main(int argc, char *argv[]) {
    // check arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <outfile>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // seed the RNG
    srand((unsigned)time(NULL));

    // create a socket
    int sockfd;
 
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("Failure to setup an endpoint socket");
	exit(1);
    }
 
    // initialize the server address structure using argv[1]
    struct sockaddr_in address, serverAddr;
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[1]));
    address.sin_addr.s_addr = INADDR_ANY;
    
    // bind the socket
    if ((bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr))) < 0) {
	perror("Failure to bind server address to the endpoint socket");
	exit(1);
    }

	//[Q1] Briefly explain what is "AF_INET" used for.
		//AF_INET designates the family of address that the socket can communicate with. It specifies IPv4 addresses
	//[Q2] Briefly explain what is "SOCK_DGRAM" used for.
		//SOCK_DGRAM specifies what transport protocol we are using for our sockets. SOCK_DGRAM means that we are using UDP with one data segment transferred and one reply
	//[Q3] Briefly explain what is "htons" used for.
		//htons converts our given integer port number to a host byte order
	//[Q4] Briefly explain why bind is required on the server and not on the client.
		//We only need to bind the server because it's recieving data from the client while the client only recieves messages without data

    // open file using argv[2]
    int file;
    file = open(argv[2], O_WRONLY, O_CREAT);
    if(file < 0) {
	printf("Problem opening file\n");
	exit(1);
    }

    // get file contents from client
    int seqnum = 0;
    Packet packet;
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len = sizeof(clientaddr);
    do {
        packet = ServerReceive(sockfd, (struct sockaddr *)&clientaddr, &clientaddr_len, seqnum);
        write(file, packet.data, packet.header.len); //write packet into destination file
        seqnum = (seqnum + 1) % 2; //update sequence number
    } while (packet.header.len != 0);

    close(file);
    close(sockfd);
    return 0;
}
