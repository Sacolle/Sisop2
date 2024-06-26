#include "sock.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#include <cerrno>
#include <iostream>


namespace net{

	int Socket::read(void *buf, int len){
		return ::recv(fd, buf, len, 0);
	}

	int Socket::send(const void *msg, int len){
		return ::send(fd, msg, len, 0);
	}

	void Socket::print_their_info(){
		std::cout << "IP: " << their_ip << ":" << their_port << std::endl;
	}

	//server
	void ServerSocket::open(const char* port, const int backlog){
		struct addrinfo *servinfo, *p, hints;
		int res, yes = 1;
		
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE; 

		if ((res = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
			//NOTE: dealloc servinfo
			freeaddrinfo(servinfo);

			std::string error_message("Error at getting own adress: \n\t");
			error_message += gai_strerror(res);
			throw NetworkException(error_message);
		}

		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("server: socket");
				continue;
			}

			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
				freeaddrinfo(servinfo);
				throw NetworkException(strerror(errno));
			}

			if (bind(fd, p->ai_addr, p->ai_addrlen) == -1) {
				perror("server: bind");
				continue;
			}
			break;
		}

		if(p == NULL){
			throw NetworkException("Server Failed to bind");
		}
		//save data

		freeaddrinfo(servinfo); // all done with this structure
		//at this point fd = the socket file descriptor
		if(::listen(fd, backlog) == -1){
			std::string error_message("Failed to listen to port: \n\t");
			error_message += strerror(errno);
			throw NetworkException(error_message);
		}
	}

	std::unique_ptr<Socket> ServerSocket::accept(){
		struct sockaddr_storage their_addr;
		socklen_t addr_size = sizeof(their_addr);
		int accept_sock = ::accept(fd, (struct sockaddr *)&their_addr, &addr_size);
		if(accept_sock == -1){
			std::string error_message("Failed to accept port: \n\t");
			error_message += strerror(errno);
			throw NetworkException(error_message);
		}
		//le as informações de porta e ip do endereço conectado
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &((struct sockaddr_in*)&their_addr)->sin_addr, ip, INET_ADDRSTRLEN );
		u_int16_t port = ntohs(((struct sockaddr_in*)&their_addr)->sin_port);
		//could be saved at socket
		return std::make_unique<Socket>(accept_sock, ip, port);
	}
	//client
	void ClientSocket::connect(const char* ip, const char* port){
		struct addrinfo hints, *servinfo, *p;
		int res;

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if ((res = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
			freeaddrinfo(servinfo);

			std::string error_message("Error at getting adress: \n\t");
			error_message += gai_strerror(res);
			throw NetworkException(error_message);
		}

		// loop through all the results and connect to the first we can
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("client: socket");
				continue;
			}
			if (::connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
				perror("client: connect");
				continue;
			}
			break;
		}
		if (p == NULL) {
			throw NetworkException("Server Failed to connect");
		}
		freeaddrinfo(servinfo); // all done with this structure
	}
};