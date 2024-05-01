#ifndef SOCKFILE
#define SOCKFILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

#include "packet_generated.h"

#define FLATBUFFER_DEFAULT_SIZE 1024
#define READ_BUFFER_SIZE 1024
#define SOCKET_READ_ATTEMPTS 3
#define HEADER_SIZE sizeof(u_int32_t)
namespace net{
	class NetworkException : public std::runtime_error{
		public:
			NetworkException(const std::string& what) : std::runtime_error(what){}
	};
	class TransmissionException : public std::exception {
		public:
			TransmissionException(Net::Operation type): type(type){}
			Net::Operation type;
	};
	class ReceptionException : public std::runtime_error {
		public:
			ReceptionException(const std::string& what) : std::runtime_error(what){}
	};
	class CloseConnectionException : public std::exception {};

	//TUDO BOILERPLATE PQ C++ N SABE SE COMPORTAR
	//na real é definindo os construtores de cada sub estrutura, 
	//colocando em um namespace _inner para n poluir o header
	namespace _inner{

		struct t_response {
			t_response(Net::Status status, const char* msg): status(status), msg(msg){};

			Net::Status status;
			::std::string msg;	
		};
		struct t_filemeta {
			t_filemeta(uint64_t size, const char* name): 
				size(size), name(name){};

			uint64_t size;
			::std::string name;
		};
		struct t_filedata{
			t_filedata(const Net::FileData& _fd): fd(_fd) {};

			const Net::FileData& fd;

		};

		union t_payload {
			t_payload(){ memset( this, 0, sizeof( t_payload ) ); }
			t_payload(const char* str): text(str){}
			t_payload(Net::Status status, const char* msg): response(status, msg){}
			t_payload(uint64_t size, const char* name): filemeta(size, name){} 
			t_payload(const Net::FileData& fd) : filedata(fd) {};

			~t_payload(){}
			t_filedata filedata;
			t_filemeta filemeta;
			t_response response;
			::std::string text;
		};
	};
	//NOTE: implementação de ids únicos,
	//tomar muito cuidado quando fazer multi thread
	uint64_t get_fileid();


	class PayloadData {
		public:
			PayloadData(Net::Operation op): operation_type(op){}
			PayloadData(Net::Operation op, const char* text): 
				operation_type(op), payload(text){}
			PayloadData(Net::Status status, const char* msg): 
				operation_type(Net::Operation_Response), payload(status, msg){}
			PayloadData(uint64_t size, const char* name):
				operation_type(Net::Operation_FileMeta), payload(size, name){}
			PayloadData(const Net::FileData& file_data):
				operation_type(Net::Operation_FileData), payload(file_data){}

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
			inline int recv(void *buf, int len){
				return ::recv(fd, buf, len, 0);
			}
			inline int send(const void *msg, int len){
				return ::send(fd, msg, len, 0);
			}
			int send_checked(void *buf, int len);
			Net::Operation recv_flatbuffer(void *buf, int len);
			inline void set_username(std::string n){ username = std::move(n); }

			void print_their_info();


			//section on message passing
			void send_file(std::string& filename); //TODO:

			void send_connect(std::string& username);
			void send_listfiles();
			void send_download(std::string& filename);
			void send_delete(std::string& filename);
			void send_response(Net::Status status, std::string& msg); 
		
			std::unique_ptr<PayloadData> read_operation();
			void read_and_save_file(std::string& filename, uint64_t id, uint64_t size); //TODO:
			void receive_file(std::string& filename, uint64_t size);

		protected:
			int fd = 0;
		private:

			void send_filemeta(uint64_t size, std::string& filename);
			void send_filedata(std::string& filename, std::unique_ptr<std::ifstream> file);
			void send_filedata_chunk(std::string& filename, uint64_t chunk_size, std::vector<uint8_t>& chunk);

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
