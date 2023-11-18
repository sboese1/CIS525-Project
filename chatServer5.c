#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include "inet.h"
#include "common.h"

#define MAX_NICKNAME 20
#define MAX_ROOM_NAME 30

struct client {
    int sockfd;
    char nickname[MAX_NICKNAME];
    struct client *next;
};

int send_message(struct client *recepients, struct client *current, char s[]);
int client_disconnected(struct client **recepients, struct client *current, char s[]);
int check_nickname_availability(struct client *existing, struct client *current, char s[]);

char name[MAX_ROOM_NAME];

void sigint_handler(int signum) {
    // Send a message to the directory server indicating shutdown
    int sockfd;
    struct sockaddr_in serv_addr;

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

    char *messages[] = {"D", name};
    char combined_message[MAX_ROOM_NAME + 6];
    strcpy(combined_message, "");

    for (int i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        strcat(combined_message, messages[i]);
    }

    write(sockfd, combined_message, strlen(combined_message));
    
    close(sockfd);
    exit(0);
}


int main(int argc, char **argv) {
    signal(SIGINT, sigint_handler);

    int sockfd, dir_sockfd, newsockfd;
    unsigned int clilen;
    struct sockaddr_in cli_addr, serv_addr, dir_serv_addr;
    fd_set readset, dir_readset;
    struct client *clients = NULL;
    char s[MAX*10];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> <topic1> <topic2> ... <topicN>\n", argv[1]);
        exit(1);
    }

    // Parse command-line arguments
    int serverPort = atoi(argv[2]);

    if (serverPort <= 0 || serverPort > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(1);
    }

    /* Bind socket to local address */
    memset((char *)&dir_serv_addr, 0, sizeof(dir_serv_addr));
    dir_serv_addr.sin_family = AF_INET;
    dir_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dir_serv_addr.sin_port = htons(SERV_TCP_PORT);

    /* Create a socket (an endpoint for communication). */
    if ((dir_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("client: can't open stream socket");
        exit(1);
    }

    /* Connect to the server. */
    if (connect(dir_sockfd, (struct sockaddr *)&dir_serv_addr, sizeof(dir_serv_addr)) < 0) {
        perror("client: can't connect to server");
        exit(1);
    }
	
    strcpy(name, argv[1]);
    char *messages[] = {"R", argv[1], "|", argv[2]};
    char combined_message[MAX_ROOM_NAME + 6];
    strcpy(combined_message, "");

    for (int i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        strcat(combined_message, messages[i]);
    }

    write(dir_sockfd, combined_message, strlen(combined_message));

    FD_ZERO(&dir_readset);
    FD_SET(dir_sockfd, &dir_readset);

    if (select(FD_SETSIZE, &dir_readset, NULL, NULL, NULL) > 0) {
	char dir_message[MAX];
	if (read(dir_sockfd, dir_message, MAX) > 0) {
		if (strcmp(dir_message, "Topic already taken.") == 0) {
			printf("Topic already taken.\n");
			return 0;
		}
		if (strcmp(dir_message, "Port already taken.") == 0) {
			printf("Port already taken.\n");
			return 0;
		}

		printf("Room created successfully!\n");
        }
    }
    close(dir_sockfd);    


    /* Create communication endpoint */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket");
        exit(1);
    }

    /* Bind socket to local address */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(serverPort);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("server: can't bind local address");
        exit(1);
    }

    listen(sockfd, 5);

    for (;;) {
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);

        // Add all client sockets to the readset
        struct client *current = clients;
        while (current != NULL) {
            FD_SET(current->sockfd, &readset);
            current = current->next;
        }

        if (select(FD_SETSIZE, &readset, NULL, NULL, NULL) > 0) {

            // Check if there's a new connection request
            if (FD_ISSET(sockfd, &readset)) {
                // Accept a new connection request
                clilen = sizeof(cli_addr);
                newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (inet_ntoa(cli_addr.sin_addr) == SERV_HOST_ADDR) {
			break;
		}
                if (newsockfd < 0) {
                    perror("server: accept error");
                    exit(1);
                }

		struct client *new_client = (struct client *)malloc(sizeof(struct client));
                new_client->sockfd = newsockfd;
		new_client->next = clients;
		clients = new_client;

		snprintf(s, MAX, "Please enter your nickname: ");
                write(newsockfd, s, strlen(s));                
	    }

            // Check if there are any messages from clients
            struct client *current = clients;
            while (current != NULL) {
                if (FD_ISSET(current->sockfd, &readset)) {
                    bzero(s, sizeof(s));
                    if (read(current->sockfd, s, MAX) <= 0) {
			client_disconnected(&clients, current, s);
                    } else {
			int size = strlen(s);
			s[size] = '\0';

			switch (s[0]) {
				case 'N': check_nickname_availability(clients, current, s+1);
					break;
				
				case 'C': send_message(clients, current, s+1);
					break;
					
				default: snprintf(s, MAX*10, "Invalid request");
					write(current->sockfd, s, MAX*10);
					break;
			}

                    }
                }
                current = current->next;
            }
        }
    }

    // Close all client sockets and free memory
    struct client *current = clients;
    while (current != NULL) {
        close(current->sockfd);
        struct client *temp = current;
        current = current->next;
        free(temp);
    }

    close(sockfd);
    return 0;
}

// Send message to all connected clients
int send_message(struct client *recepients, struct client *current, char s[]) {                        		
        while (recepients != NULL) {
        	if (recepients != current) {
                	char message[MAX + MAX_NICKNAME + 2];
                        snprintf(message, sizeof(message), "%s: %s", current->nickname, s);
                        write(recepients->sockfd, message, strlen(message));
                }
                recepients = recepients->next;
        }

	return 0;
}

int client_disconnected(struct client **clients, struct client *current, char s[]) {
        // Remove client from linked list
        struct client *temp = *clients;
        if (temp == current) {
        	*clients = temp->next;
		while (temp->next != NULL) {
			temp = temp->next;
			snprintf(s, MAX*10, "%s has disconnected.", current->nickname);
            		write(temp->sockfd, s, strlen(s));
                }
        } else {
        	while ((temp->next != NULL) && (temp->next != current)) {
			snprintf(s, MAX*10, "%s has disconnected.", current->nickname);
            		write(temp->sockfd, s, strlen(s));
                	temp = temp->next;
                }
        	if (temp->next != NULL) {
			snprintf(s, MAX*10, "%s has disconnected.", current->nickname);
            		write(temp->sockfd, s, strlen(s));
                	temp->next = temp->next->next;
                }
        }

        // Close the socket and free memory
        close(current->sockfd);
	free(current);
}

// Check nickname availability
int check_nickname_availability(struct client *existing, struct client *current, char s[]) {
    struct client *temp = existing;
    
    if (temp->next == NULL) {
	char message[MAX];
	snprintf(message, MAX, "You are the first user to join!");
        write(current->sockfd, message, MAX);
	strcpy(current->nickname, s);
    }
    else {
    	while (temp != NULL) {
        	if (strcmp(temp->nickname, s) == 0) {
		    char message[MAX];
		    snprintf(message, MAX, "Nickname already taken. Enter another:");
        	    write(current->sockfd, message, MAX);
        	    return 0; // Nickname already in use
        	}
        	temp = temp->next;
    	}

	strcpy(current->nickname, s);
	temp = existing;
        while (temp != NULL) {
        	if (temp != current) {
              		char message[(MAX*10) + MAX_NICKNAME + 37];
			snprintf(message, sizeof(message), "%s has joined the chat!", current->nickname);
                    	write(temp->sockfd, message, strlen(message));
               	}
               	temp = temp->next;
        }

    }

    return 1;
}