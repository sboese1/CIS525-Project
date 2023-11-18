#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"

#define MAX_CHAT_ROOMS 10
#define MAX_ROOM_NAME 30
#define MAX_ADDR 100

struct ChatRoom {
    char topic[MAX_ROOM_NAME];
    char address[MAX_ADDR];
    char port[5];
};


int main(int argc, char **argv) {
    char s[MAX*10] = {'\0'};
    fd_set readset;
    int sockfd;
    struct sockaddr_in serv_addr;
    int nread;
    int nickname = 0;

    struct ChatRoom chatRooms[MAX_CHAT_ROOMS];
    struct ChatRoom curRoom;
    int numChatRooms = 0;

    /* Set up the address of the directory server to be contacted. */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    serv_addr.sin_port = htons(SERV_TCP_PORT);

    /* Create a socket (an endpoint for communication). */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("client: can't open stream socket");
        exit(1);
    }

    /* Connect to the server. */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("client: can't connect to server");
        exit(1);
    }

    printf("Connected to the directory server.\n");

    write(sockfd, "Q", 1);

    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);

    if (FD_ISSET(sockfd, &readset)) {
        if ((nread = read(sockfd, s, MAX*10)) <= 0) {
            printf("Error reading from directory server\n");
        } else {
        if (s[0] == 'A') {
            	numChatRooms = 0;
		char* tempRooms[5];
            	char *token = strtok(s+1, ";");
		while (token != NULL) {
			tempRooms[numChatRooms] = token;
			token = strtok(NULL, ";");
			numChatRooms++;
	    	}

            for (int i = 0; i < numChatRooms; i++) {
            char *token2 = strtok(tempRooms[i], ":");
                if (token2 != NULL) {
                        strcpy(chatRooms[i].topic, token2);
                    token2 = strtok(NULL, ":");
                }

                if (token2 != NULL) {
                        strcpy(chatRooms[i].address, token2); 
                    token2 = strtok(NULL, ":");
                }

                if (token2 != NULL) {
                        strcpy(chatRooms[i].port, token2);        			
                }
            }

            printf("Available chat rooms:\n");
            for (int i = 0; i < numChatRooms; i++) {
                printf("%d. %s\n", i+1, chatRooms[i].topic);
            }
            } else {
                printf("Directory Server Response: %s\n", s);
            }
        }    
    }

    int j = 0;
    char roomName[MAX_ROOM_NAME];
    while (j == 0) {
	printf("Type the name of the room you want to join: ");
	fgets(roomName, sizeof(roomName), stdin);
	roomName[strlen(roomName)-1] = '\0';
	for (int k = 0; k < numChatRooms; k++) {
		if (strcmp(chatRooms[k].topic, roomName) == 0) {
			curRoom = chatRooms[k];
			j = 1;
			break;
		}
	}
	if (j == 0) printf("Room not found.\n");
    }

    /* Set up the address of the server to be contacted. */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(curRoom.address);
    serv_addr.sin_port = htons(atoi(curRoom.port));

    /* Create a socket (an endpoint for communication). */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("client: can't open stream socket");
        exit(1);
    }

    /* Connect to the server. */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("client: can't connect to server");
        exit(1);
    }


    for (;;) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(sockfd, &readset);

        if (select(sockfd+1, &readset, NULL, NULL, NULL) > 0) {

// Check whether there's user input to read
            if (FD_ISSET(STDIN_FILENO, &readset)) {
                if (fgets(s, MAX, stdin) != NULL) {
	            char message[strlen(s)+1];
		    if (nickname == 0) {
			snprintf(message, sizeof(message), "N%s", s);
	            	write(sockfd, message, strlen(s)+1);
			nickname = 1;
		    }
		    else {
			snprintf(message, sizeof(message), "C%s", s);
			write(sockfd, message, strlen(s)+1);
		    }
                } else {
                    printf("Error reading or parsing user input\n");
                }
            }

            // Check whether there's a message from the server to read
            if (FD_ISSET(sockfd, &readset)) {
                if ((nread = read(sockfd, s, MAX)) <= 0) {
                    printf("Error reading from server\n");
                    break;
                } else {
                    s[nread] = '\0';
                    printf("%s\n", s);
		    if (strcmp("Nickname already taken. Enter another:", s) == 0) {
        	    	nickname = 0;
        	    }
                }
            }
        }
    }
    close(sockfd);
    return 0;
}