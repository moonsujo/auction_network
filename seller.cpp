#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>

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
    
    char ipstr[INET6_ADDRSTRLEN];
  
    // use argv[1] to read items from a file
    string fileName = argv[1];
    ifstream input_file(fileName);

    int N;
    time_t t;

    if (!input_file.is_open()) {
        cerr << "failed to open file" << endl;
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
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo("localhost", TCP_PORT, &hints, &servinfo);
    if (rv != 0) {
      cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
      return 1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
      cerr << "seller: socket" << endl;
      return 1;
    }

    rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (rv == -1) {
      close(sockfd);
      cerr << "seller: connect" << endl;
      return 1;
    } else {
        t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
        void const *addr;
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)servinfo->ai_addr;
        addr = &(ipv4->sin_addr);
        unsigned short int port;
        port = ipv4->sin_port;
        inet_ntop(servinfo->ai_family, addr, ipstr, sizeof ipstr);
        cout << std::put_time(std::localtime(&t), "%c %Z") << ": Connected to " << ipstr << " on port " << ntohs(port) << "." << endl;
    }

    freeaddrinfo(servinfo); // all done with this structure

    for (int i = 0; i < N; i++) {
      //write to file
      int itemPrice;
      string itemName;
      input_file >> itemPrice;
      getline(input_file, itemName);

      //formulate message
      TCP_Message msg;
      msg.price = htonl(itemPrice);
      strcpy(msg.name, itemName.c_str());
      rv = send(sockfd, &msg, sizeof(TCP_Message), 0);
      if (rv == -1) {
          cerr << "seller: send transaction id" << endl;
          return 1;
      }


      t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
      cout << std::put_time(std::localtime(&t), "%c %Z") << ": Sent" << itemName << " for sale for $" << itemPrice << ".\n";
      sleep(0.5);
    }

    while(1) {
      TCP_Message msg;
      rv = recv(sockfd, &msg, sizeof(TCP_Message), 0);
      t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
      cout << std::put_time(std::localtime(&t), "%c %Z") << ": Received shipping address, ";
      cout << msg.shipping_address << ", for" << msg.name << " bought for $" << ntohl(msg.price) << endl;
      if (rv == -1) {
          cerr << "recv" << endl;
          //return 1;
      }
    }

    close(sockfd);

    return 0;
} 
