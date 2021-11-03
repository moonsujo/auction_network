all: seller server

seller: seller.cpp
	g++ seller.cpp -lpthread -lstdc++ -Wall -Wextra -std=c++17 -o seller.out

buyer: buyer.cpp
	g++ buyer.cpp -lpthread -lstdc++ -Wall -Wextra -std=c++17 -o buyer.out

server: server.cpp
	g++ server.cpp -lpthread -lstdc++ -Wall -Wextra -std=c++17 -o server.out

run-phase1: seller server
	bash run-phase1.sh

run-phase2: seller buyer server
	bash run-phase2.sh

run-phase3: seller buyer server
	bash run-phase2.sh

clean:
	rm -rf seller.out server.out