#ifndef SOCKFILE
#define SOCKFILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

#include "flatbuffers/flatbuffers.h"

#include "exceptions.hpp"

#define READ_BUFFER_SIZE 1024
#define SOCKET_READ_ATTEMPTS 3
#define PAYLOAD_HEADER_SIZE sizeof(u_int32_t)


namespace net{
	class Socket{
		public:
			//deleta os construtores de copy pra n dar ruim
			Socket& operator= (const Socket&) = delete;
			Socket(int sock_fd) noexcept : fd(sock_fd){ bzero(read_buff, READ_BUFFER_SIZE); }
			Socket(int sock_fd, const char* ip, uint16_t port) noexcept : fd(sock_fd), their_ip(ip), their_port(port) {
				bzero(read_buff, READ_BUFFER_SIZE);
			}
			~Socket() noexcept { close(fd); }
			void print_their_info();
			void send_checked(const void *buf, const int len);
			void send_checked(flatbuffers::FlatBufferBuilder *buff);

			uint8_t* read_full_pckt();
		private:
			inline int recv(void *buf, const int len) noexcept { return ::recv(fd, buf, len, 0); }
			inline int send(const void *msg, const int len) noexcept { return ::send(fd, msg, len, 0); }

			const int fd = 0;
			std::string their_ip;
			uint16_t their_port;
			uint8_t read_buff[READ_BUFFER_SIZE];

	};
	class ServerSocket{
		public:
			ServerSocket& operator= (const ServerSocket&) = delete;
			ServerSocket() noexcept {}

			void open(const char* port, const int backlog);
			std::shared_ptr<Socket> accept();
		private:
			int fd;
	};

	class ClientSocket {
		public:
			ClientSocket& operator= (const ClientSocket&) = delete;
			ClientSocket() noexcept {}

			void connect(const char* ip, const char* port);
			std::shared_ptr<Socket> build();
		private:
			int fd = -1;
	};
};


#endif
