#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <stdio_ext.h>
#include "crc.h"

//structs
struct sockaddr_in serv_addr;
struct hostent *server;
struct pollfd pollArray[2];

//functions
void readEnteredMessage();
void readMessage(int sockFd);
void sendMessage(int sockfd);
int connectToServer(int sockFd, int portNo);

//global variables
int statusCode = 0;
char buf[115];
int sendMessageCode = 0;
int syncNumber = 10000;
char syncMessage[3] = "S:";
char ackMessage[3] = "A:";
unsigned int crcValue = 0;
int count = 0;

int main(int argc, char *argv[]) {

	int sockFd = 0;
	int portNo = 0;
	int timeout = 0;
	int pollReturn = 0;
	int fd = 0;

	//initiate crc tester this allows for everything to get ready for crc.
	crcInit();

	if (argc < 3) {
		fprintf(stderr, "usage %s hostname port\n", argv[0]);
		exit(0);
	}

	//connection variables.
	portNo = atoi(argv[2]);
	sockFd = socket(AF_INET, SOCK_STREAM, 0);
	server = gethostbyname(argv[1]);

	//connect to server
	sockFd = connectToServer(sockFd, portNo);

	pollArray[0].fd = fd;
	pollArray[0].events = 0;
	pollArray[0].events = POLLIN;
	pollArray[1].fd = sockFd;
	pollArray[1].events = 0;
	pollArray[1].events = POLLIN;

	while (1) {

		timeout = 200;
		pollReturn = poll(pollArray, 2, timeout);

		if (pollReturn == 0) {
			//timeout holder
		}

		else {

			if (pollArray[1].revents & POLLIN) {
				readMessage(sockFd);

			} else {

				if (pollArray[0].revents & POLLIN) {
					readEnteredMessage();
				}

			}

			sendMessage(sockFd);
		}

	}

}

/**
 *
 *
 *  This Method will attempt to connect to the server with
 *  the variables passed in sockFd and portNo will return error
 *  if unsuccessful or socket file descriptor.
 *
 * **/

int connectToServer(int sockFd, int portNo) {

	if (sockFd < 0) {
		perror("ERROR opening socket");
		exit(1);
	}

	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portNo);

	/* Now connect to the server */
	if (connect(sockFd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR connecting");
		exit(1);
	}

	return sockFd;

}

/**
 *
 * This Method will attempt to read the message entered into stdin by
 * the user. It will set the global variable buffer to the message written
 * in. Will return an error if their is a problem reading from buffer.
 *
 * **/

void readEnteredMessage() {
	int ret = 0;

	if (sendMessageCode != 1) {
		memset(buf, 0, sizeof(buf));
		fgets(buf, sizeof(buf), stdin);
		fflush(stdin);

	}

	if (ret != -1) {

		statusCode = 1;

	}
	if (ret < 0) {
		perror("ERROR writing to socket");
		exit(1);
	}

}

/**
 *
 * This Method will attempt to read from the passed in sockFd, segment the message
 * and set the send message code for the send Message
 *
 * **/

void readMessage(int sockFd) {

	int n = 0;

	char sequnceNumberHolder[10];
	char ackHolder[2];
	char messageHolder[256];
	int sequenceNumbers = 0;
	char messageRecievedBuffer[1000];
	char crcHolder[256];
	char recievedCRCForTesting[256];
	char charHolder[10];

	memset(messageRecievedBuffer, 0, sizeof(messageRecievedBuffer));
	memset(sequnceNumberHolder, 0, sizeof(sequnceNumberHolder));
	memset(ackHolder, 0, sizeof(ackHolder));
	memset(messageHolder, 0, sizeof(messageHolder));
	memset(crcHolder, 0, sizeof(crcHolder));
	memset(charHolder, 0, sizeof(charHolder));

	n = read(sockFd, messageRecievedBuffer, sizeof(messageRecievedBuffer));
	__fpurge(stdout);

	if (n == 0) {
		fprintf(stderr, "\n ************ Finished ***********\n");
		close(sockFd);
		close(0);
		exit(0);
	}

	for (int i = 0; i < 10; i++) {
		if (messageRecievedBuffer[i] != ':') {
			strncpy(charHolder, &messageRecievedBuffer[i], 1);
			strcat(crcHolder, charHolder);

		} else {
			i++;
			strncpy(sequnceNumberHolder, &messageRecievedBuffer[i], 5);
			i = i + 6;
			strncpy(ackHolder, &messageRecievedBuffer[i], 1);
			i = i + 2;
			strncpy(messageHolder, &messageRecievedBuffer[i],
					strlen(messageRecievedBuffer));
			break;
		}

	}

	sscanf(sequnceNumberHolder, "%d", &sequenceNumbers);
	sscanf(crcHolder, "%d", &crcValue);
	memset(recievedCRCForTesting, 0, sizeof(recievedCRCForTesting));
	snprintf(recievedCRCForTesting, sizeof(recievedCRCForTesting), "%d:%s:%s",
			sequenceNumbers, ackHolder, messageHolder);
	int crcValue2 = crcFast(recievedCRCForTesting,
			strlen(recievedCRCForTesting));

	//Debug information
	//fprintf(stderr, "\nWhat got recieved in buffer: %s", testBuffer);
	//fprintf(stderr, "Recieved Sequence Number: %s\n", sequnceNumberHolder);
	//fprintf(stderr, "Actual Sync Number: %d\n", sync_number);
	//fprintf(stderr, "CRC check Message Result: %d\n", crcValue2);
	//fprintf(stderr, "\nMessage for CRC Testing: %s", recievedCRCForTesting);
	//fprintf(stderr, "CRC Recieved in Message: %s\n", crcHolder);
	//fprintf(stderr, "ACK holder: %s\n", ackHolder);
	//fprintf( stderr, "Message holder: %s\n", messageHolder);

	if (n < 0) {
		herror("READ ERROR!");
	}

	if (crcValue == crcValue2) {
		if (sequenceNumbers == syncNumber) {

			if (ackHolder[0] == 'S') {
				statusCode = 5;
				printf("%s", messageHolder);
				fflush(stdout);

			}

			else if (ackHolder[0] == 'A') {
				statusCode = -1;
				syncNumber++;
				count = 0;
				//allow their to be a new message to be printed to printf.
				sendMessageCode = 0;

			}

		} else {
			count++;
			if (count > 3) {
				statusCode = 10;
			}
		}

	}
}

/**
 *
 * Here i send the message to the passed in socket descriptor, first i check the
 * crc value then combine the message to send.
 *
 * **/

void sendMessage(int sockfd) {

	int n;
	char messageBuffer[256];
	char crcTestBuffer[256];

	if (statusCode == -1) {
		//do nothing
	}

	if (statusCode == 1) {

		usleep(60000);

		memset(crcTestBuffer, 0, sizeof(crcTestBuffer));
		snprintf(crcTestBuffer, sizeof(crcTestBuffer), "%d:%s%s", syncNumber,
				syncMessage, buf);
		//calculating the crc value
		crcValue = crcFast(crcTestBuffer, strlen(crcTestBuffer));
		memset(messageBuffer, 0, sizeof(messageBuffer));
		snprintf(messageBuffer, sizeof(messageBuffer), "%d:%d:%s%s", crcValue,
				syncNumber, syncMessage, buf);
		sendMessageCode = 1;
		n = write(sockfd, messageBuffer, strlen(messageBuffer));

	}

	if (statusCode == 5) {
		char emptyMessage[3] = "E\n";

		memset(messageBuffer, 0, sizeof(messageBuffer));
		memset(crcTestBuffer, 0, sizeof(crcTestBuffer));
		snprintf(crcTestBuffer, sizeof(crcTestBuffer), "%d:%s%s", syncNumber,
				ackMessage, emptyMessage);
		crcValue = crcFast(crcTestBuffer, strlen(crcTestBuffer));
		snprintf(messageBuffer, sizeof(messageBuffer), "%d:%d:%s%s", crcValue,
				syncNumber, ackMessage, emptyMessage);
		n = write(sockfd, messageBuffer, strlen(messageBuffer));
		//increase sync number after send
		syncNumber++;
		statusCode = -1;
		sendMessageCode = 0;

		if (n < 0) {
			printf("Error in writing message back..");
		}

	}
	if (statusCode == 10) {

		int resendSyncNumber = syncNumber - 1;
		char resendMessage[3] = "E\n";
		char testers[256];

		memset(testers, 0, sizeof(testers));
		memset(crcTestBuffer, 0, sizeof(crcTestBuffer));
		snprintf(crcTestBuffer, sizeof(crcTestBuffer), "%d:%s%s",
				resendSyncNumber, ackMessage, resendMessage);
		crcValue = crcFast(crcTestBuffer, strlen(crcTestBuffer));
		snprintf(testers, sizeof(testers), "%d:%d:%s%s", crcValue,
				resendSyncNumber, ackMessage, resendMessage);

		n = write(sockfd, testers, strlen(testers));

		if (n < 0) {
			printf("Error in writing message back..");
		}

	}

}


