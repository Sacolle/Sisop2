#include "sock.hpp"

#include "packet_generated.h"

#include <iostream>

#define PORT "20001"
#define BACKLOG 10

void echo(std::unique_ptr<net::Socket> s){
	char buff[256];
	flatbuffers::FlatBufferBuilder builder;
	int seqn = 0;
	while(1){
		bzero(buff, 256);
		builder.Clear();
		int read_bytes = s->read(buff, 256);
		//buff[read_bytes] = '\0';
		//std::cout << buff;

		auto expected_msg_size = flatbuffers::GetSizePrefixedBufferLength((u_int8_t*) buff);
		if(read_bytes != expected_msg_size){
			std::cerr << "pacote ainda n foi recebido inteiro" << std::endl;
			exit(1);
		}

		std::cout << "recebeu " << expected_msg_size << " bytes " << std::endl;

		auto msg = Net::GetSizePrefixedPacket(buff);

		if(msg->op_type() != Net::Operation_Connect){
			std::cerr << "q operação eh ent?" << std::endl;
			continue;
		}
		auto payload = static_cast<const Net::Connect*>(msg->op());
		std::cout << "Connection from " << payload->username()->str() << std::endl;
		
		std::string msg_res("Eai ");
		msg_res += payload->username()->str();

		auto response = Net::CreateResponse(builder, Net::Status_Ok, builder.CreateString(msg_res));
		auto packet = Net::CreatePacket(builder, Net::Operation_Response, response.Union());
		//NOTE: termina préfixando o tamanho do pacote no buffer
		builder.FinishSizePrefixed(packet);

		int sent_bytes = s->send(builder.GetBufferPointer(), builder.GetSize());

		std::cout << "Enviou " << builder.GetSize() << " bytes " << std::endl;

		if(sent_bytes != builder.GetSize()){
			std::cerr << "n enviou tudo ;-;" << std::endl;
			continue;
		}
	}
}


int main() {
	net::ServerSocket socket;

	try{
		socket.open(PORT, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	while(1){
		try {
			auto s = socket.accept();
			s->print_their_info();
			echo(std::move(s));
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

