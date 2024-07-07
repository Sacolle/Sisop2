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

#include "packet_generated.h"

#include "exceptions.hpp"
#include "defines.hpp"

#define SOCKET_READ_ATTEMPTS 3
#define PAYLOAD_HEADER_SIZE sizeof(u_int32_t)


namespace net{
	class Socket{
		public:
			//deleta os construtores de copy pra n dar ruim
			Socket& operator= (const Socket&) = delete;
			Socket(int sock_fd) noexcept : fd(sock_fd){ bzero(read_buff, SK_BUFFSIZE); }
			Socket(int sock_fd, const char* ip, uint16_t port) noexcept : fd(sock_fd), their_ip(ip), their_port(port) {
				bzero(read_buff, SK_BUFFSIZE);
			}
			~Socket() noexcept { close(fd); }
			void print_address();

			void set_connection_info(const std::string& username, 
				const uint64_t user_id, const Net::ChannelType channel_type);
			inline std::string& get_username() { return username; }
			inline uint64_t get_user_id() { return user_id; }
			inline Net::ChannelType get_channel_type() { return channel_type; }

			
			void send_checked(const void *buf, const int len);
			void send_checked(flatbuffers::FlatBufferBuilder *buff);
			uint8_t* read_full_pckt();
		private:
			inline int recv(void *buf, const int len) noexcept { return ::recv(fd, buf, len, 0); }
			inline int send(const void *msg, const int len) noexcept { return ::send(fd, msg, len, 0); }

			const int fd = 0;
			std::string their_ip;
			uint16_t their_port;
			uint8_t read_buff[SK_BUFFSIZE];

			//NOTE: poderia ser em uma estrutura separada, porém faz mais sentido o id, nome e tipo estar aqui
			std::string username;
			uint64_t user_id;
			Net::ChannelType channel_type;
	};
	class ServerSocket{
		public:
			ServerSocket& operator= (const ServerSocket&) = delete;
			ServerSocket() noexcept {}

			void open(const char* port, const int backlog);
			Socket *accept();
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
