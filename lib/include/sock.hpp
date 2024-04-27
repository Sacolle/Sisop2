#ifndef SOCKFILE
#define SOCKFILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <memory>


namespace net{
	class NetworkException : public std::runtime_error{
		public:
			NetworkException(const std::string& what) : std::runtime_error(what){}
	};

	class Socket{
		public:
			//deleta os construtores de copy pra n dar ruim
			Socket& operator= (const Socket&) = delete;
			
			Socket(){}
			Socket(int sock_fd): fd(sock_fd){}
			Socket(int sock_fd, char* ip, u_int16_t port): 
				fd(sock_fd), their_ip(ip), their_port(port){}
			//destructor closes the socket
			~Socket(){
				//NOTE: usado aqui para, nesse estágio incial, notar se n há double frees no design
				printf("Destrutor da socket %d\n", fd);
				close(fd);
			}

			int read(void *buf, int len);
			int send(const void *msg, int len);
			void print_their_info();
		protected:
			int fd = 0;
		private:
			std::string their_ip;
			u_int16_t their_port;

	};
	class ServerSocket : public Socket{
		public:
			ServerSocket& operator= (const ServerSocket&) = delete;
			ServerSocket(){}

			void open(const char* port, const int backlog);
			std::unique_ptr<Socket> accept();
		private:
	};

	class ClientSocket : public Socket{
		public:
			ClientSocket& operator= (const ClientSocket&) = delete;
			ClientSocket(){}

			void connect(const char* ip, const char* port);
		private:
			//TODO:
	};
};

#endif
