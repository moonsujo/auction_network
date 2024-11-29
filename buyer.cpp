#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <random>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "project.h"


using namespace std;


int main(int argc, char* argv[]) { 
    
  // use argv[1] to read items from a file
  // use argv[2] to read the shipping address
  string shipping_address = argv[1];

  //UDP
  //connect with server

  string fileName = argv[1];
  ifstream input_file(fileName);
  int N;
  time_t t;
  int numbytes;

  if (!input_file.is_open()) {
      cerr << "failed to open file" << endl;
      cout << "first return" << endl;
      return 1;
  } else {
      input_file >> N;
      t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
      cout << std::put_time(std::localtime(&t), "%c %Z") << ": Reading " << N << " items from " << fileName << endl;
  }

    struct addrinfo hints, *servinfo;

    int sockfd;

    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; // <<<---- First Change

    rv = getaddrinfo("localhost", UDP_PORT, &hints, &servinfo);
    if (rv != 0) {
      cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
      cout << "second return" << endl;
      return 1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
      cerr << "buyer: socket" << endl;
      cout << "third return" << endl;
      return 1;
    }

    for (int i = 0; i < N; i++) {
        
      UDP_Message msg;
      uint32_t itemPrice;
      string itemName;
      input_file >> itemPrice;
      getline(input_file, itemName);
      msg.price = htonl(itemPrice);
      strcpy(msg.name, itemName.c_str());

      int rv = 1;
      bool keepSending = true;
      while(keepSending) {
        msg.action = 1;
        numbytes = sendto(sockfd, &msg, sizeof(msg), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        if (numbytes == -1) {
          cerr << "buyer: sentto" << endl;
          cout << "fourth return" << endl;
          return 1;
        }

        cout << std::put_time(std::localtime(&t), "%c %Z") << ": Sent bid $" << itemPrice;
        cout << " on" << itemName << endl;

        struct sockaddr_storage client_addr; 
        socklen_t client_addr_len = sizeof(client_addr);

        UDP_Message msg_receive;

        rv = recvfrom(sockfd, &msg_receive, sizeof(msg_receive), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (rv == -1) {
          cerr << "buyer: recvfrom" << endl;
          cout << "fifth return" << endl;
          return 1;
        }

        if (ntohl(msg_receive.action) == 0) {
            cout << std::put_time(std::localtime(&t), "%c %Z") << ": Received bidding not open." << endl;
        } else if (ntohl(msg_receive.action) == 2) {
            cout << std::put_time(std::localtime(&t), "%c %Z") << ": Received bid $" << ntohl(msg_receive.price) << " on" << msg_receive.name << " accepted." << endl;
            keepSending = false;
        } else if (ntohl(msg_receive.action) == 3) {
            cout << std::put_time(std::localtime(&t), "%c %Z") << ": Received bid on" << msg_receive.name << " rejected, current bid price $";
            cout << ntohl(msg_receive.price) << endl;
            keepSending = false;
        }

        if (itemName != msg_receive.name) {
            sleep(0.5);
            keepSending = true;
        }

        this_thread::sleep_for(0.5s);
      
      }

    }

    //receive messages about winning bids
    while(1) {
      UDP_Message msg_receive;
      struct sockaddr_storage client_addr; 
      socklen_t client_addr_len = sizeof(client_addr);
      rv = recvfrom(sockfd, &msg_receive, sizeof(msg_receive), 0, (struct sockaddr *)&client_addr, &client_addr_len);
      if (rv == -1) {
        cerr << "buyer: recvfrom" << endl;
        cout << "sixth return" << endl;
        return 1;
      }
      if (ntohl(msg_receive.action) == 4) {
          cout << std::put_time(std::localtime(&t), "%c %Z") << ": Congratulations! Yon won an auction. " << endl;
          cout << "Received shipping address share request for" << msg_receive.name << " bought for $" << msg_receive.price << "." << endl;

          UDP_Message message_send_address;
          message_send_address.action = htonl(4);
          strcpy(message_send_address.shipping_address, shipping_address.c_str());
          strcpy(message_send_address.name, msg_receive.name);
          message_send_address.price = ntohl(msg_receive.price);
          message_send_address.seller_socket = msg_receive.seller_socket;
          message_send_address.seller_addr = msg_receive.seller_addr;

          numbytes = sendto(sockfd, &message_send_address, sizeof(message_send_address), 0, servinfo->ai_addr, servinfo->ai_addrlen);
          if (numbytes == -1) {
            cerr << "buyer: send address" << endl;
            cout << "seventh return" << endl;
            return 1;
          }
          
      }
    }



    close(sockfd);


    cout << "last return" << endl;
    return 0;
} 
