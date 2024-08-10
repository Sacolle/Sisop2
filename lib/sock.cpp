#include "sock.hpp"
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#include <cerrno>
#include <iostream>

namespace net{

	void Socket::print_address(){
		std::cout << "IP: " << their_ip << ":" << their_port << std::endl;
	}

	void Socket::set_connection_info(const std::string& username, 
		const uint64_t user_id, const Net::ChannelType channel_type){

		Socket::username = username;
		Socket::user_id = user_id;
		Socket::channel_type = channel_type;
	}

	void Socket::send_checked(const void *buf, const int len){
		const int sent_bytes = send(buf, len);
		if(sent_bytes != len){
			throw TransmissionException(); 
		}
	}

	void Socket::send_checked(flatbuffers::FlatBufferBuilder *buff){
		const int size = buff->GetSize();
		const int sent_bytes = send(buff->GetBufferPointer(), size);
		if(sent_bytes != size){
			throw TransmissionException(); 
		}
	}
	//throws `CloseConnectionException` and `ReceptionException`
	uint8_t* Socket::read_full_pckt(){
		bzero(read_buff, PAYLOAD_HEADER_SIZE);	

		int read_bytes = recv(read_buff, PAYLOAD_HEADER_SIZE);
		if(read_bytes == 0){
			throw CloseConnectionException(); //closed connection
		}
		int tries = SOCKET_READ_ATTEMPTS;

		auto expected_msg_size = flatbuffers::GetSizePrefixedBufferLength(read_buff);
		
		//enquanto n ler todos os bytes esperados do pacote, append no buffers os bytes chegando
		while(read_bytes < expected_msg_size && tries > 0){
			read_bytes += recv(read_buff + read_bytes, expected_msg_size - read_bytes);
			tries--;
		}
		if(read_bytes != expected_msg_size){
			std::cout << "Lido: " << read_bytes << std::endl;
			std::cout << "Tries: " << tries << std::endl; 
			std::cout << "Esperado: " << expected_msg_size << std::endl; 
			throw ReceptionException("read_bytes != expected_msg_size"); //incomplete message
		}
		return read_buff;
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
		::fcntl(fd, F_SETFL, O_NONBLOCK);
		if(::listen(fd, backlog) == -1){
			std::string error_message("Failed to listen to port: \n\t");
			error_message += strerror(errno);
			throw NetworkException(error_message);
		}
	}

	Socket *ServerSocket::accept(){
		struct sockaddr_storage their_addr;
		socklen_t addr_size = sizeof(their_addr);
		int accept_sock = ::accept(fd, (struct sockaddr *)&their_addr, &addr_size);
		if(accept_sock == -1){
			if ((errno != EAGAIN && errno != EWOULDBLOCK)) {
				std::string error_message("Failed to accept port: \n\t");
				error_message += strerror(errno);
				throw NetworkException(error_message);
			} else {
				return nullptr;
			}
		}
		//le as informações de porta e ip do endereço conectado
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &((struct sockaddr_in*)&their_addr)->sin_addr, ip, INET_ADDRSTRLEN );
		u_int16_t port = ntohs(((struct sockaddr_in*)&their_addr)->sin_port);
		//could be saved at socket
		return new Socket(accept_sock, ip, port);
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

	std::shared_ptr<Socket> ClientSocket::build(){
		return std::make_shared<Socket>(fd);
	}

	UDPSocketAdress::UDPSocketAdress(const std::string& ip, const int port){
		adress_info.sin_family = AF_INET;
		adress_info.sin_port = htons(port);
		adress_info.sin_addr.s_addr = inet_addr(ip.c_str());
	}

	void UDPSocket::open(const std::string& ip, const int port){
		if((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
			throw NetworkException("Failed to create UDP socket file descriptor");
		}

		adress_info.sin_family = AF_INET;
		adress_info.sin_port = htons(port);
		adress_info.sin_addr.s_addr = inet_addr(ip.c_str());
		
		if(bind(fd, (struct sockaddr*)&adress_info, sizeof(adress_info)) < 0){
			throw NetworkException("Couldn't bind to the UDP port\n");
		}
	}
	//recebe um datagrama UDP, retornando o endereço de quem enviou
	//considera que todos os pacotes chegam completos, se não for o caso, gera uma exceção
	UDPSocketAdress UDPSocket::recv(std::vector<uint8_t>& buff, int* size){
		//clear the buff
		buff.clear();

		//create the struct for the adress of the receving packet
		UDPSocketAdress recv_adress;
		uint32_t adress_struct_size = sizeof( recv_adress.adress_info );

		const int read_bytes = ::recvfrom(fd, 
			buff.data(), buff.capacity(), //buffer space that bytes can be read to
			0, //flags
			(struct sockaddr*)&recv_adress.adress_info, //saves the receving adress in the struct
			&adress_struct_size
		);

		if(read_bytes < 0){
			throw ReceptionException("read_bytes < 0");
		}

		const auto expected_msg_size = flatbuffers::GetSizePrefixedBufferLength(buff.data());
		
		if(read_bytes != expected_msg_size){
			throw ReceptionException("read_bytes != expected_msg_size"); //incomplete message
		}
		return recv_adress;
	}

	void UDPSocket::send(UDPSocketAdress& adress, std::vector<uint8_t>& buff){

		const int sent_bytes = ::sendto(fd, 
			buff.data(), buff.size(), 
			0,
         	(struct sockaddr*) &adress.adress_info,
			sizeof(adress.adress_info)
		);

		if(sent_bytes != buff.size()){
			throw TransmissionException(); 
		}
	}
	void UDPSocket::send(UDPSocketAdress& adress, flatbuffers::FlatBufferBuilder *buff){
		const auto buff_size = buff->GetSize();
		const int sent_bytes = ::sendto(fd, 
			buff->GetBufferPointer(), buff_size, 
			0,
         	(struct sockaddr*) &adress.adress_info,
			sizeof(adress.adress_info)
		);

		if(sent_bytes != buff_size){
			throw TransmissionException(); 
		}
	}
};