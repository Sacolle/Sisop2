#include "sock.hpp"

#include <iostream>

#define PORT "20001"
#define BACKLOG 10

void echo(std::unique_ptr<net::Socket> s){
	char buff[256];
	while(1){
		int read_bytes = s->read(buff, 256);
		buff[read_bytes] = '\0';

		std::cout << buff << std::endl;

		int sent_bytes = s->send(buff, read_bytes);
		if(sent_bytes != read_bytes){
			std::cerr << "n enviou tudo ;-;" << std::endl;
			exit(2);
		}
	}
}


int main() {
	net::ServerSocket socket;

	try{
		socket.open(PORT, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	while(1){
		try {
			auto s = socket.accept();
			s->print_their_info();
			echo(std::move(s));
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

