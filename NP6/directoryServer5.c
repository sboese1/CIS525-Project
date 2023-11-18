#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"

#define MAX_ROOMS 10
#define MAX_ROOM_NAME 30
#define MAX_ADDR 100

struct ChatRoom {
    char topic[MAX_ROOM_NAME];
    char address[MAX_ADDR];
    char port[10];
    int sockfd;
};

int main(int argc, char **argv) {
    int sockfd, newsockfd;
    unsigned int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char s[MAX*10];
    struct ChatRoom chatRooms[MAX_ROOMS];
    int numChatRooms = 0;

    /* Create communication endpoint */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket");
        exit(1);
    }

    /* Bind socket to local address */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_TCP_PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("server: can't bind local address");
        exit(1);
    }

    listen(sockfd, 5);

    clilen = sizeof(cli_addr);

    for (;;) {

        /* Accept a new connection request */
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("server: accept error");
            exit(1);
        }

	memset(s, 0, sizeof(s));

        /* Read the request from the client */
        if (read(newsockfd, s, MAX*10) <= 0) {
            fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
            exit(1);
        }

	char topic[MAX_ROOM_NAME];
	char port[5];
	char ip[MAX_ADDR];

        switch (s[0]) { /* based on the first character of the client's message */
            case 'R': // Registration request
		
		strcpy(topic, strtok(s+1, "|"));
		strcpy(port, strtok(NULL, "|"));
                strcpy(ip, inet_ntoa(cli_addr.sin_addr));

                // Check if the chat room already exists
                int roomIndex = -1;
                for (int i = 0; i < numChatRooms; i++) {
                    if (strcmp(chatRooms[i].topic, topic) == 0) {
			roomIndex = i;
			snprintf(s, MAX, "Topic already taken.");
			break;
                    }
		    if (strcmp(chatRooms[i].port, port) == 0) {
			roomIndex = i;
			snprintf(s, MAX, "Port already taken.");
			break;
                    }

                }

                if (roomIndex == -1) {
                    if (numChatRooms >= MAX_ROOMS) {
                        snprintf(s, MAX*10, "Maximum number of chat rooms reached. Registration failed.\n");
                    } else {
                        // Register the new chat room
                        strcpy(chatRooms[numChatRooms].topic, topic);
                        strcpy(chatRooms[numChatRooms].address, ip);
                        strcpy(chatRooms[numChatRooms].port, port);
			chatRooms[numChatRooms].sockfd = newsockfd;
                        numChatRooms++;
                        snprintf(s, MAX, "Room registered successfully.");
                    }
                }
                break;
            case 'Q': // Query request
		memset(s, 0, sizeof(s));
		snprintf(s, 2, "A");
                for (int i = 0; i < numChatRooms; i++) {
		    int len = strlen(chatRooms[i].topic) + strlen(chatRooms[i].address) + strlen(chatRooms[i].port);
                    snprintf(s + strlen(s), MAX + len + 4, "%s:%s:%s;",
                             chatRooms[i].topic, chatRooms[i].address, chatRooms[i].port);
                }
                break;
	    case 'D':
		for (int i = 0; i < numChatRooms; i++) {
                    if (strcmp(chatRooms[i].topic, s+1) == 0) {
    			for (int j = i; j < numChatRooms - 1; j++) {
        			chatRooms[j] = chatRooms[j + 1];
    			}
        		memset(&chatRooms[numChatRooms - 1], 0, sizeof(struct ChatRoom));
			numChatRooms--;
                        break;
                    }
                }

		break;
            default:
                snprintf(s, MAX, "Invalid request\n");
                break;
        }

        /* Send the reply to the client. */
        write(newsockfd, s, MAX);
        close(newsockfd);
    }
}