/*This work has been adapted from online sources including:
- Lecture 18 slides from class
- https://github.com/um4ng-tiw
- https://github.com/NateWilliams2
- https://www.youtube.com/watch?app=desktop&v=oHBi8k31fgM
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

#define MAX_NAME_LEN 30
#define MAX_MSG_LEN 500
#define MAX_RAW_LEN MAX_NAME_LEN + MAX_MSG_LEN + 2
#define MAX_PEERS 100


// Keep the username in a global so we can access it from the callback
const char* username;
int connections = 0;
int peer_sockets[MAX_PEERS] = {-1};

void *receive_connections(void *server_socket_fd){
  int server_fd = *(int*)server_socket_fd;
  if(listen(server_fd, 1)) {
    perror("Failed to listen");
    exit(2);
  }
  while(1){
    int peer_socket_fd = server_socket_accept(server_fd);
    if(peer_socket_fd == -1){
      perror("Cannot open server socket");
      exit(2);
    }
    peer_sockets[connections++] = peer_socket_fd;
  }
}

// sends messages to all but source
void send_msg(const char* msg, int source){
  for(int i=0; i < connections; ++i){
    if(peer_sockets[i] == -1) break;
    if(i != source) {
      if(write(peer_sockets[i], msg, MAX_RAW_LEN) == -1){
          perror("Unable to write to peer");
          exit(2);
      } 
    }
  }
}

// listens and sends messages
void *listen_func(){
  char buf[MAX_RAW_LEN];
  struct timeval t;
  while(1){

    t.tv_sec = 1;
    t.tv_usec = 0;
    fd_set reads;
    FD_ZERO(&reads);
    for (int i = 0; i < connections; ++i){
      FD_SET(peer_sockets[i], &reads);
    }

    int rc = select(peer_sockets[connections-1]+1, &reads, NULL, NULL, &t);
    if(rc == -1){
      perror("Cannot select peer sockets");
      exit(2);
    }
    if(rc != 0) {
      for (int i=0; i < connections; i++){
        if (FD_ISSET(peer_sockets[i], &reads)){
          if(peer_sockets[i] == -1) break;
          // Reed from peer socket
          int rd = read(peer_sockets[i], buf, MAX_RAW_LEN);
          if(rd != 0) {
            if(rd == -1){
            perror("Cannnot receive message");
            exit(EXIT_FAILURE);
          }
          char rec_name[MAX_NAME_LEN+1], rec_msg[MAX_MSG_LEN+1];
          memset(rec_name, '\0', sizeof(rec_name));
          memset(rec_msg, '\0', sizeof(rec_msg));
          const char *display_msg = rec_msg;
          const char *display_name = rec_name;
          strncpy(rec_name, buf, MAX_NAME_LEN + 1);
          strncpy(rec_msg, buf + MAX_NAME_LEN + 1, MAX_MSG_LEN + 1);
          ui_display(display_name, display_msg);
          send_msg(buf, i);
          }
        }
      }
    }
  }
}


// This function is run whenever the user hits enter after typing a message
void input_callback(const char* message) {
  if(strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {
    ui_exit();
  } else {
    ui_display(username, message);
    char msg_raw[MAX_RAW_LEN];
    memset(msg_raw, '\0', sizeof(msg_raw));
    strncpy(msg_raw, username, MAX_NAME_LEN + 1);
    strncpy(msg_raw + MAX_NAME_LEN + 1, message, MAX_MSG_LEN + 1);

    //send msg to peers
    send_msg(msg_raw, -1);
  }
}

int main(int argc, char** argv) {
  // Make sure the arguments include a username
  if(argc != 2 && argc != 4) {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the username in a global
  username = argv[1];

  // TODO: Set up a server socket to accept incoming connections
  unsigned short server_port = 0;
  int socket_fd = server_socket_open(&server_port);
  if(socket_fd == -1) {
    perror("Unable to open server socket");
    exit(EXIT_FAILURE);
  }

  // Did the user specify a peer we should connect to?
  if(argc == 4) {
    // Unpack arguments
    char* peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);
    // TODO: Connect to another peer in the chat network
    int peer_socket_fd = socket_connect(peer_hostname, peer_port);
    if(peer_socket_fd == -1) {
      perror("Failed to connect to another peer");
      exit(EXIT_FAILURE);
    }
    peer_sockets[connections++] = peer_socket_fd;
  }

  //run threads
  pthread_t server_thread;
  if(pthread_create(&server_thread, NULL, receive_connections, &socket_fd) != 0) {
    perror("Unable to create thread");
    exit(EXIT_FAILURE);
  }
  pthread_t listener_thread;
  if(pthread_create(&listener_thread, NULL, listen_func, NULL) != 0){
    perror("Unable to create thread");
    exit(EXIT_FAILURE);
  }

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  // Once the UI is running, you can use it to display log messages
  char listening_on[24];
  sprintf(listening_on, "%u", server_port);
  ui_display("listening on port", listening_on);

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();

  return 0;
}
