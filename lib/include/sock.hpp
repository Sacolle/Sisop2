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

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

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

			inline int get_fd(){ return fd; }

			//NOTE: jeito mais facil de fazer isso
			int election_weight = 0;

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
			ServerSocket(bool nonblocking = false): nonblocking(nonblocking){}

			void open(const char* port, const int backlog);
			Socket *accept();
		private:
			bool nonblocking;
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

	class UDPSocketAdress {
		public:
			UDPSocketAdress(const std::string& ip, const int port);
		protected:
			UDPSocketAdress(){
				bzero(&adress_info, sizeof(adress_info));
			}
			struct ::sockaddr_in adress_info;
		
		friend class UDPSocket;
	};

	class UDPSocket {
		public:
			UDPSocket& operator= (const UDPSocket&) = delete;
			UDPSocket(){}
			~UDPSocket() noexcept {
				close(fd);
			}

			//bind to own port and ip
			void open(const std::string& ip, const int port);
			//receive from -> returns the socket adress of the message
			//buff: buffer para colocar a mensagem
			//size: tamanho da mensagem colocado no buffer
			//emite uma exeção caso size seja menor que o tamanho esperado
			UDPSocketAdress recv(std::vector<uint8_t>& buff, int* size);

			//envia o buff para UDPSocketAdress
			//se não mandou a mensagem completa emite uma exeção
			void send(UDPSocketAdress& adress, std::vector<uint8_t>& buff);
			void send(UDPSocketAdress& adress, flatbuffers::FlatBufferBuilder *buff);
		private:
			int fd = 0;
			struct ::sockaddr_in adress_info;
	};
};


#endif
