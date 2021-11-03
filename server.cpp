%%writefile server.cpp
#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>

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
using namespace chrono_literals;


void seller_thread(sockaddr_storage client_addr, int new_socket, mutex &item_mutex, vector<item> &items, time_t &t_last_item_accepted) {
    
  int numbytes;

  time_t t_start = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  char ipstr[INET6_ADDRSTRLEN];
  void const *addr;
  struct sockaddr_in *sin = (struct sockaddr_in *)&client_addr;
  addr = &(sin->sin_addr);
  unsigned short int port;
  port = sin->sin_port;
  inet_ntop(AF_INET, addr, ipstr, sizeof ipstr);
  cout << std::put_time(std::localtime(&t_start), "%c %Z") << ": Accepted a new TCP connection from " << ipstr << ":" << ntohs(port) << endl;

  TCP_Message msg;
  while(1) {
    numbytes = recv(new_socket, &msg, sizeof(TCP_Message), 0);
    if (numbytes == -1) {
        cerr << "recv" << endl;
        //return 1;
    }
    if (numbytes != 0) {
      t_start = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
      cout << std::put_time(std::localtime(&t_start), "%c %Z") << ": Received" << msg.name << " for sale for $" << ntohl(msg.price) << " from " << ipstr;
      cout << ":" << htons(port) << " (thread: " << this_thread::get_id() << ")." << endl;
      item new_item;
      new_item.price = msg.price;
      new_item.bidPlaced = false;
      new_item.bidClosed = false;
      new_item.seller_addr = client_addr;
      new_item.seller_socket = new_socket;
      //first argument: string to copy the other string into
      strcpy(new_item.name, msg.name);
      //save data to structure
      item_mutex.lock();
      items.push_back(new_item);
      item_mutex.unlock();
      item_mutex.lock();
      t_last_item_accepted = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
      item_mutex.unlock();
    }
  }
}

/*
 * start a thread that counts the time elapsed since the item had a bid.
 * note: starts when an item receives a bid for the first time.
 */
void counter_thread(item &item, int buyerSocket) {
    int numbytes;
    time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    while((t_now - item.t_last_bid_placed) < 3) {
        sleep(1);
        t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    }

    //item has been sold to the last bidder
    //send message requesting shipping address
    UDP_Message msg_request_shipping;
    msg_request_shipping.action = htonl(4);
    msg_request_shipping.price = htonl(item.price);
    msg_request_shipping.seller_addr = item.seller_addr;
    msg_request_shipping.seller_socket = item.seller_socket;
    strcpy(msg_request_shipping.name, item.name);

    char      ipstr_highest_bidder[INET6_ADDRSTRLEN];
    uint16_t  port_highest_bidder;
      
    if (item.highest_bidder_addr.ss_family == AF_INET) {
      struct sockaddr_in *ipv4_highest_bidder = (struct sockaddr_in *)&(item.highest_bidder_addr);
      inet_ntop(AF_INET, &(ipv4_highest_bidder->sin_addr), ipstr_highest_bidder, sizeof ipstr_highest_bidder);
      port_highest_bidder = ntohs(ipv4_highest_bidder->sin_port);
    }

    socklen_t highest_bidder_addr_len = sizeof(item.highest_bidder_addr);
    numbytes = sendto(buyerSocket, &msg_request_shipping, sizeof(msg_request_shipping), 0, (struct sockaddr *)&item.highest_bidder_addr, highest_bidder_addr_len);
    cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sold! Requesting shipping address from " << ipstr_highest_bidder << ":";
    cout << port_highest_bidder << " for" << item.name << " bought for $" << ntohl(item.price) << "." << endl;
    if (numbytes == -1) {
      cerr << "server: sentto" << endl;
      //return 1;
    }

    item.bidClosed = true;

}

void sendAddressToSeller(UDP_Message &msg) {
    char ipstr_seller[INET6_ADDRSTRLEN];
    void const *addr_seller;
    struct sockaddr_in *sin = (struct sockaddr_in *)&msg.seller_addr;
    addr_seller = &(sin->sin_addr);
    unsigned short int port_seller;
    port_seller = sin->sin_port;
    inet_ntop(AF_INET, addr_seller, ipstr_seller, sizeof ipstr_seller);

    //formulate message
    TCP_Message msg_share_address_seller;
    msg_share_address_seller.price = msg.price;
    msg_share_address_seller.seller_socket = msg.seller_socket;
    strcpy(msg_share_address_seller.name, msg.name);
    strcpy(msg_share_address_seller.shipping_address, msg.shipping_address);
    int rv = send(msg.seller_socket, &msg_share_address_seller, sizeof(TCP_Message), 0);
    if (rv == -1) {
        cerr << "seller: send transaction id" << endl;
        //return 1;
    }
    
    time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sharing buyer shipping address, ";
    cout << msg_share_address_seller.shipping_address << ", for " << msg_share_address_seller.name << " bought for $";
    cout << ntohl(msg_share_address_seller.price) << " with seller " << ipstr_seller << ":" << htons(port_seller) << "." << endl;
}

/*
 * find the item in the auction with the matching name and the lowest price.
 * returns the index in the list.
 */
int returnMatchIndex(vector<item> &items, char* name) {
  int match_index = -1;
  uint32_t match_price = 0;
  int currentIndex = 0;
  for (auto &item : items) {
    //if an item with a matching name is found
    if (strcmp(name, item.name) == 0) {
      //found for the first time
      if (match_price == 0) {
        //save its index and the price
        match_index = currentIndex;
        match_price = ntohl(item.price);
        //keep searching the vector
      //found for second time or more
      } else if (match_price > ntohl(item.price)) {
        //save its index and the price
        match_index = currentIndex;
        match_price = ntohl(item.price);
        //keep searching the vector
      }
    }
    currentIndex++;
  }
  return match_index;
}

void updateBid(item &matching_item, UDP_Message &msg, sockaddr_storage client_addr, int buyerSocket) {
  int numbytes;
  matching_item.price = msg.price;
  matching_item.highest_bidder_addr = client_addr;

  char      ipstr_highest_bidder[INET6_ADDRSTRLEN];
  uint16_t  port_highest_bidder;
    
  if (matching_item.highest_bidder_addr.ss_family == AF_INET) {
    struct sockaddr_in *ipv4_highest_bidder = (struct sockaddr_in *)&matching_item.highest_bidder_addr;
    inet_ntop(AF_INET, &(ipv4_highest_bidder->sin_addr), ipstr_highest_bidder, sizeof ipstr_highest_bidder);
    port_highest_bidder = ntohs(ipv4_highest_bidder->sin_port);
  }

  //send "accepted" message to the new buyer
  UDP_Message msg_new_buyer;
  msg_new_buyer.action = htonl(2);
  //msg's price field is in network form
  //don't need to convert it
  msg_new_buyer.price = msg.price;
  strcpy(msg_new_buyer.name, msg.name);
  socklen_t new_highest_bidder_addr_len = sizeof(matching_item.highest_bidder_addr);
  numbytes = sendto(buyerSocket, &msg_new_buyer, sizeof(msg_new_buyer), 0, (struct sockaddr *)&matching_item.highest_bidder_addr, new_highest_bidder_addr_len);
  matching_item.t_last_bid_placed = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&matching_item.t_last_bid_placed), "%c %Z") << ": Sent bid $" << htonl(msg_new_buyer.price);
  cout << " on" << msg_new_buyer.name << " accepted to " << ipstr_highest_bidder << ":" << port_highest_bidder << "." << endl;
  if (numbytes == -1) {
    cerr << "server: sentto" << endl;
    //return 1;
  }

  matching_item.bidPlaced = true;
  //count
  thread t_counter (counter_thread, ref(matching_item), buyerSocket);
  t_counter.detach();
}

/*
 * sends a rejected notice to the bidder.
 * note: only runs when a bid is placed for the very first time,
 * before anyone has bidded on the item.
 * client_addr: the address information of the bidder.
 * itemToBid: the item whose name matches the bid.
 */
void sendRejectionFirstBid(item &itemToBid, UDP_Message &msg, uint16_t port, char* ipstr, int buyerSocket, sockaddr_storage client_addr) {
  int numbytes;
  UDP_Message msg_rejected;
  msg_rejected.action = htonl(3);
  strcpy(msg_rejected.name, msg.name);
  //price on the item
  //already converted (network format)
  msg_rejected.price = itemToBid.price;
  socklen_t client_addr_len = sizeof(client_addr);
  numbytes = sendto(buyerSocket, &msg_rejected, sizeof(msg_rejected), 0, (struct sockaddr *)&client_addr, client_addr_len);
  if (numbytes == -1) {
    cerr << "server: sentto" << endl;
    //return 1;
  }
  time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sent bid $" << ntohl(msg.price);
  //must convert port from htons
  //isn't this the host, and it's already converted into host from above though?
  cout << " on" << msg_rejected.name << " rejected to " << ipstr << ":" << htons(port) << "." << endl;
}

void sendRejectionOldBuyer(UDP_Message &msg, item &item, int buyerSocket, char *ipstr_highest_bidder, uint16_t port_highest_bidder) {
  int numbytes;
  UDP_Message msg_old_buyer;
  msg_old_buyer.action = htonl(3);
  //msg's price field is in network form
  //bid price from buyer
  msg_old_buyer.price = msg.price;
  strcpy(msg_old_buyer.name, msg.name);

  socklen_t highest_bidder_addr_len = sizeof(item.highest_bidder_addr);
  numbytes = sendto(buyerSocket, &msg_old_buyer, sizeof(msg_old_buyer), 0, (struct sockaddr *)&item.highest_bidder_addr, highest_bidder_addr_len);
  time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sent bid $" << ntohl(item.price);
  cout << " on" << msg_old_buyer.name << " rejected to " << ipstr_highest_bidder << ":" << port_highest_bidder << "." << endl;
  if (numbytes == -1) {
      cerr << "server: sentto" << endl;
      //return 1;
  }
}


void sendAcceptedNewBuyer(UDP_Message &msg, item &item, int buyerSocket, char* ipstr_highest_bidder, uint16_t port_highest_bidder) {
  int numbytes;
  UDP_Message msg_new_buyer;
  msg_new_buyer.action = htonl(2);
  //msg's price field is in network form
  //don't need to convert it
  msg_new_buyer.price = msg.price;
  strcpy(msg_new_buyer.name, msg.name);
  socklen_t new_highest_bidder_addr_len = sizeof(item.highest_bidder_addr);
  numbytes = sendto(buyerSocket, &msg_new_buyer, sizeof(msg_new_buyer), 0, (struct sockaddr *)&item.highest_bidder_addr, new_highest_bidder_addr_len);
  item.t_last_bid_placed = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sent bid $" << htonl(msg_new_buyer.price);
  cout << " on" << msg_new_buyer.name << " accepted to " << ipstr_highest_bidder << ":" << port_highest_bidder << "." << endl;
  if (numbytes == -1) {
      cerr << "server: sentto" << endl;
      //return 1;
  }
}

/*
 * Sends a rejection to the buyer if the bid was lower than the current bid.
 * note: only runs when at least one bid has been placed.
 * client_addr: the bidder's ip information
 * msg: the bidder's message with the bid
 */
void sendRejectionNewBid(UDP_Message &msg, item &item, uint16_t port, char* ipstr, int buyerSocket, sockaddr_storage client_addr) {
  int numbytes;
  UDP_Message msg_rejected;
  msg_rejected.action = htonl(3);
  strcpy(msg_rejected.name, msg.name);
  //price on the item
  //already converted (network format)
  msg_rejected.price = item.price;
  socklen_t client_addr_len = sizeof(client_addr);
  numbytes = sendto(buyerSocket, &msg_rejected, sizeof(msg_rejected), 0, (struct sockaddr *)&client_addr, client_addr_len);
  if (numbytes == -1) {
    cerr << "server: sentto" << endl;
    //return 1;
  }
  time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sent bid $" << ntohl(msg.price);
  //must convert port from htons
  //isn't this the host, and it's already converted into host from above though?
  cout << " on" << msg_rejected.name << " rejected to " << ipstr << ":" << htons(port) << "." << endl;
}

void buyer_thread(sockaddr_storage client_addr, int buyerSocket, time_t &t_last_item_accepted, vector<item> &items) {
  
  int rv, numbytes;
  while (1) {
      
    char      ipstr[INET6_ADDRSTRLEN];
    uint16_t  port;
      
    if (client_addr.ss_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
      inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof ipstr);
      port = ntohs(ipv4->sin_port);
    } else {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
      inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipstr, sizeof ipstr);
      port = ntohs(ipv6->sin6_port);
    }
      
    UDP_Message msg;

    socklen_t client_addr_len = sizeof(client_addr);

    rv = recvfrom(buyerSocket, &msg, sizeof(msg), 0, (struct sockaddr *)&client_addr, &client_addr_len);
    if (rv == -1) {
      cerr << "server: recvfrom" << endl;
      //return 1;
    }

    //in seconds
    time_t t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());

    //time out for 3 seconds since the last item was listed
    if (t_now - t_last_item_accepted < 3) {
      cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Sent bidding not open to ";
      cout << ipstr << ":" << ntohs(port) << "." << endl;
      msg.action = 0;
      numbytes = sendto(buyerSocket, &msg, sizeof(msg), 0, (struct sockaddr *)&client_addr, client_addr_len);
      if (numbytes == -1) {
        cerr << "server: sentto" << endl;
        //return 1;
      }
    } else if (ntohl(msg.action) == 4) {
          //receive shipping address
          //use message from above
          t_now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
          cout << std::put_time(std::localtime(&t_now), "%c %Z") << ": Received shipping address, ";
          cout << msg.shipping_address << ", from " << ipstr << ":" << port << " for ";
          cout << msg.name << " bought for $" << ntohl(msg.price) << "." << endl;

          //send to seller
          sendAddressToSeller(ref(msg));

    } else {
        
        int match_index = returnMatchIndex(ref(items), msg.name);
        
        if (items[match_index].bidPlaced == false) {
            
            if (ntohl(items[match_index].price) <= ntohl(msg.price)) {
                
                updateBid(ref(items[match_index]), ref(msg), client_addr, buyerSocket);

            } else {

                sendRejectionFirstBid(ref(items[match_index]), msg, port, ipstr, buyerSocket, client_addr);

            }

        } else {
            
            if (ntohl(items[match_index].price) < ntohl(msg.price)) {
                
                char      ipstr_highest_bidder[INET6_ADDRSTRLEN];
                uint16_t  port_highest_bidder;
                  
                if (items[match_index].highest_bidder_addr.ss_family == AF_INET) {
                  struct sockaddr_in *ipv4_highest_bidder = (struct sockaddr_in *)&items[match_index].highest_bidder_addr;
                  inet_ntop(AF_INET, &(ipv4_highest_bidder->sin_addr), ipstr_highest_bidder, sizeof ipstr_highest_bidder);
                  port_highest_bidder = ntohs(ipv4_highest_bidder->sin_port);
                }

                //send "rejected" message to the old buyer
                sendRejectionOldBuyer(ref(msg), ref(items[match_index]), buyerSocket, ipstr_highest_bidder, port_highest_bidder);

                //update the item's highest bidder addr and price
                items[match_index].price = msg.price;
                items[match_index].highest_bidder_addr = client_addr;
                  
                if (items[match_index].highest_bidder_addr.ss_family == AF_INET) {
                    struct sockaddr_in *ipv4_highest_bidder = (struct sockaddr_in *)&items[match_index].highest_bidder_addr;
                    inet_ntop(AF_INET, &(ipv4_highest_bidder->sin_addr), ipstr_highest_bidder, sizeof ipstr_highest_bidder);
                    port_highest_bidder = ntohs(ipv4_highest_bidder->sin_port);
                }

                //send "accepted" message to the new buyer
                sendAcceptedNewBuyer(ref(msg), ref(items[match_index]), buyerSocket, ipstr_highest_bidder, port_highest_bidder);

            } else {
                //send "rejected" message
                sendRejectionNewBid(ref(msg), ref(items[match_index]), port, ipstr, buyerSocket, client_addr);

            }
        }
    }
  }
}



int main() { 
    
  time_t t;

  vector<item> items;
  mutex item_mutex;

  int sockfd, new_socket;               // listen on sock_fd, new connection on new_socket
  struct addrinfo hints, *servinfo;
  struct sockaddr_storage client_addr;  // connector's address information
  socklen_t sin_size;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  rv = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo);
  if (rv != 0) {
    cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    return 1;
  }

  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (sockfd == -1) {
    cerr << "server: socket" << endl;
    return 1;
  }

  // updated line
  int yes = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  rv = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
  if (rv == -1) {
    close(sockfd);
    cerr << "server: bind" << endl;
    return 1;
  }
  t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
  cout << std::put_time(std::localtime(&t), "%c %Z") << ": Waiting for connection on port " << TCP_PORT << "." << endl;


  //UDP
  int sockfd_udp;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;            // <<<---- First Change
  hints.ai_flags = AI_PASSIVE;
  rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo);

  sockfd_udp = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (sockfd_udp == -1) {
    cerr << "server: udp socket" << endl;
    return 1;
  }
  
  setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  rv = bind(sockfd_udp, servinfo->ai_addr, servinfo->ai_addrlen);
  if (rv == -1) {
    close(sockfd_udp);
    cerr << "server: udp bind" << endl;
    return 1;
  }

  freeaddrinfo(servinfo); // all done with this structure

  time_t t_last_item_accepted;

  //must wrap variable in "ref()" when you want to pass it by reference
  thread tBuyer (buyer_thread, client_addr, sockfd_udp, ref(t_last_item_accepted), ref(items));
  tBuyer.detach();

  rv = listen(sockfd, BACKLOG);
  if (rv == -1) {
    cerr << "server: listen" << endl;
    //return 1;
  }



  while (1) {
      
    sin_size = sizeof client_addr;
  
    new_socket = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (new_socket == -1) {
      cerr << "server: accept" << endl;
      //return 1;
    }

    thread t (seller_thread, client_addr, new_socket, ref(item_mutex), ref(items), ref(t_last_item_accepted));
    t.detach(); 
  }

  //thread blocked because of "accept"
  

  close(new_socket);
  close(sockfd);

  return 0;
} 