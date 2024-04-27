#ifndef SOCKFILE
#define SOCKFILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdexcept>
#include <string>


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

			//destructor closes the socket
			~Socket(){
				close(fd);
			}
			int read(void *buf, int len);
			int send(const void *msg, int len);
		protected:
			int fd = 0;

	};
	class ServerSocket : public Socket{
		public:
			ServerSocket& operator= (const ServerSocket&) = delete;
			ServerSocket(){}

			void open(const char* port);
			void listen(const int backlog);
			Socket accept();

		private:
			//TODO:
	};

	class ClientSocket : public Socket{
		public:
			ClientSocket& operator= (const ClientSocket&) = delete;
			ClientSocket();

			void connect(const char* ip, const char* port);
		private:
			//TODO:
	};
};

#endif
