#include "sock.hpp"
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#include <cerrno>
#include <iostream>

namespace net{
	void Socket::send_connect(std::string& username){
		builder.Clear();

		auto connect = Net::CreateConnect(builder, builder.CreateString(username));
		auto packet = Net::CreatePacket(builder, Net::Operation_Connect, connect.Union());
		builder.FinishSizePrefixed(packet);

		int sent_bytes = send(builder.GetBufferPointer(), builder.GetSize());

		if(sent_bytes != builder.GetSize()){
			throw TransmissionException(Net::Operation_Connect); 
		}
	}
	void Socket::send_listfiles(){
		builder.Clear();

		auto list_files = Net::CreateListFiles(builder);
		auto packet = Net::CreatePacket(builder, Net::Operation_ListFiles, list_files.Union());
		builder.FinishSizePrefixed(packet);

		int sent_bytes = send(builder.GetBufferPointer(), builder.GetSize());

		if(sent_bytes != builder.GetSize()){
			throw TransmissionException(Net::Operation_ListFiles); 
		}
	}
	void Socket::send_download(std::string& filename){
		builder.Clear();

		auto dowload = Net::CreateDownload(builder, builder.CreateString(filename));
		auto packet = Net::CreatePacket(builder, Net::Operation_Download, dowload.Union());
		builder.FinishSizePrefixed(packet);

		int sent_bytes = send(builder.GetBufferPointer(), builder.GetSize());

		if(sent_bytes != builder.GetSize()){
			throw TransmissionException(Net::Operation_Download); 
		}

	}
	void Socket::send_delete(std::string& filename){
		builder.Clear();

		auto del = Net::CreateDelete(builder, builder.CreateString(filename));
		auto packet = Net::CreatePacket(builder, Net::Operation_Delete, del.Union());
		builder.FinishSizePrefixed(packet);

		int sent_bytes = send(builder.GetBufferPointer(), builder.GetSize());

		if(sent_bytes != builder.GetSize()){
			throw TransmissionException(Net::Operation_Delete); 
		}

	}
	void Socket::send_response(Net::Status status, std::string& msg){
		builder.Clear();

		auto response = Net::CreateResponse(builder, status, builder.CreateString(msg));
		auto packet = Net::CreatePacket(builder, Net::Operation_Response, response.Union());
		builder.FinishSizePrefixed(packet);

		int sent_bytes = send(builder.GetBufferPointer(), builder.GetSize());

		if(sent_bytes != builder.GetSize()){
			throw TransmissionException(Net::Operation_Response); //incomplete message
		}
	}

	//reads the whole packet, and returns the data associated with it
	std::unique_ptr<PayloadData> Socket::read_operation(){
		builder.Clear();
		bzero(read_buff, READ_BUFFER_SIZE);

		int read_bytes = read(read_buff, READ_BUFFER_SIZE);
		if(read_bytes == 0){
			throw CloseConnectionException(); //closed connection
		}
		int tries = SOCKET_READ_ATTEMPS;

		auto expected_msg_size = flatbuffers::GetSizePrefixedBufferLength(read_buff);
		
		//enquanto n ler todos os bytes esperados do pacote, append no buffers os bytes chegando
		while(read_bytes < expected_msg_size && tries > 0){
			read_bytes += read(read_buff + read_bytes, READ_BUFFER_SIZE - read_bytes);
			tries--;
		}
		if(read_bytes != expected_msg_size){
			throw ReceptionException(); //incomplete message
		}

		auto msg = Net::GetSizePrefixedPacket(read_buff);

		switch (msg->op_type()){
		case Net::Operation_Connect: {
			auto payload = static_cast<const Net::Connect*>(msg->op());
			return std::make_unique<PayloadData>(
				Net::Operation_Connect, 
				payload->username()->c_str()
			);
		} break;
		case Net::Operation_ListFiles: {
			return std::make_unique<PayloadData>(Net::Operation_ListFiles);
		} break;
		case Net::Operation_Download: {
			auto payload = static_cast<const Net::Download*>(msg->op());
			return std::make_unique<PayloadData>(
				Net::Operation_Download, 
				payload->filename()->c_str()
			);
		} break;
		case Net::Operation_Delete: {
			auto payload = static_cast<const Net::Delete*>(msg->op());
			return std::make_unique<PayloadData>(
				Net::Operation_Delete, 
				payload->filename()->c_str()
			);
		} break;
		case Net::Operation_Response: {
			auto payload = static_cast<const Net::Response*>(msg->op());
			return std::make_unique<PayloadData>(
				payload->status(),
				payload->msg()->c_str()
			);
		} break;
		case Net::Operation_FileMeta: {
			auto payload = static_cast<const Net::FileMeta*>(msg->op());
			return std::make_unique<PayloadData>(
				payload->id(),
				payload->size(),
				payload->name()->c_str()
			);
		} break;
		case Net::Operation_FileData: 
			//TODO:
			return std::make_unique<PayloadData>(3);
			break;
		default:
			//didn't match any operation known
			throw ReceptionException();
		}
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
			fd = -1;
			throw NetworkException("Server Failed to connect");
		}
		freeaddrinfo(servinfo); // all done with this structure
	}

	std::unique_ptr<Socket> ClientSocket::build(){
		return std::make_unique<Socket>(fd);
	}
};