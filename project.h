#ifndef PROJECT_H 
#define PROJECT_H

#define TCP_PORT "8000"

#define UDP_PORT "8001"

#define BACKLOG 10   // how many pending connections queue will hold

struct TCP_Message {
  uint32_t  price;
  char      name[20];
  char      shipping_address[20];
  sockaddr_storage seller_addr;
  uint32_t seller_socket;
};

//action 4: contains shipping address
struct UDP_Message {
  uint32_t  action;
  uint32_t  price;
  char      name[20];
  char      shipping_address[20];
  sockaddr_storage seller_addr;
  uint32_t seller_socket;
};

struct item {
    uint32_t price;
    char     name[20];
    sockaddr_storage highest_bidder_addr;
    uint32_t seller_socket;
    sockaddr_storage seller_addr;
    bool bidPlaced;
    time_t t_last_bid_placed;
    bool bidClosed;
};

#endif /* PROJECT_H */