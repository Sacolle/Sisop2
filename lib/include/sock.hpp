#ifndef SOCKFILE
#define SOCKFILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <string>
#include <memory>

#include "packet_generated.h"

#define FLATBUFFER_DEFAULT_SIZE 1024
#define READ_BUFFER_SIZE 1024
#define SOCKET_READ_ATTEMPS 3

namespace net{
	class NetworkException : public std::runtime_error{
		public:
			NetworkException(const std::string& what) : std::runtime_error(what){}
	};
	class TransmissionException : public std::exception {
		/* NOTE: definir como fazer as exceções, se fazer com vários tipos, ou se fazer 1 com um enum
		public:
			const char* what() const noexcept override {

			}
		private:
			int type ;
		*/
	};
	namespace _inner{
		struct t_response {
			Net::Status status;
			std::string msg;	
		};
		struct t_filemeta {
			uint64_t id;
			uint64_t size;
			std::string name;
		};
		struct t_filedata{
			int file; //TODO: file descriptor (schenenigans)
		};

		union t_payload {
			t_payload(){ 
				memset( this, 0, sizeof( t_payload ) ); 
			}
			~t_payload(){}
			t_filedata filedata;
			t_filemeta filemeta;
			t_response response;
			std::string text;
		};
	};

	class PayloadData {
		public:
			PayloadData(Net::Operation op): operation_type(op){}
			PayloadData(Net::Operation op, std::string& text): operation_type(op){
				payload.text = text;
			}
			PayloadData(Net::Status status, std::string& msg): operation_type(Net::Operation_Response){
				payload.response.status = status;
				payload.response.msg = msg;
			}
			PayloadData(u_int32_t id, u_int32_t size, std::string& name): operation_type(Net::Operation_FileMeta){
				payload.filemeta.id = id;
				payload.filemeta.size = size;
				payload.filemeta.name = name;
			}
			PayloadData(int f): operation_type(Net::Operation_FileData){
				payload.filedata.file = f;
			}

			Net::Operation operation_type;
			_inner::t_payload payload;
	};

	class Socket{
		public:
			//deleta os construtores de copy pra n dar ruim
			Socket& operator= (const Socket&) = delete;
			
			Socket(){
				bzero(read_buff, READ_BUFFER_SIZE);
			}
			Socket(int sock_fd): fd(sock_fd), builder(FLATBUFFER_DEFAULT_SIZE){
				bzero(read_buff, READ_BUFFER_SIZE);
			}
			Socket(int sock_fd, char* ip, u_int16_t port): 
				fd(sock_fd), their_ip(ip), their_port(port), builder(FLATBUFFER_DEFAULT_SIZE)
			{
				bzero(read_buff, READ_BUFFER_SIZE);
			}
			//destructor closes the socket
			~Socket(){
				close(fd);
			}
			inline int read(void *buf, int len){
				return ::recv(fd, buf, len, 0);
			}
			inline int send(const void *msg, int len){
				return ::send(fd, msg, len, 0);
			}
			inline void set_username(std::string n){ username = std::move(n); }

			void print_their_info();


			//section on message passing
			void send_file(std::string& filename); //TODO:

			void send_connect(std::string& username);
			void send_listfiles();
			void send_download(std::string& filename);
			void send_delete(std::string& filename);
			void send_response(Net::Status status, std::string& msg); //define type enum
		
			std::unique_ptr<PayloadData> read_operation();

		protected:
			int fd = 0;
		private:
			//TODO: implementações de mandar os metadados e o chunk de arquivo
			void send_filemeta();
			void send_filedata();

			std::string username;
			std::string their_ip;
			u_int16_t their_port;
			flatbuffers::FlatBufferBuilder builder;
			u_int8_t read_buff[READ_BUFFER_SIZE];

	};
	class ServerSocket{
		public:
			ServerSocket& operator= (const ServerSocket&) = delete;
			ServerSocket(){}

			void open(const char* port, const int backlog);
			std::unique_ptr<Socket> accept();
		private:
			int fd;
	};

	class ClientSocket {
		public:
			ClientSocket& operator= (const ClientSocket&) = delete;
			ClientSocket(){}

			void connect(const char* ip, const char* port);
			std::unique_ptr<Socket> build();
		private:
			int fd = -1;
	};
};

#endif
