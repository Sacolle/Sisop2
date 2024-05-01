#include "sock.hpp"

#include "packet_generated.h"

#include <iostream>
#include <memory>

#define IP "127.0.0.1"
#define PORT "20001"

void basic_op(std::unique_ptr<net::Socket> socket){
	std::string nome("alberto");
	try{
		socket->send_connect(nome);
		auto response = socket->read_operation();

		if(response->operation_type != Net::Operation_Response){
			std::cerr << "cliente: erro no teste, operação enviada errada" << std::endl;
			exit(2);
		}
		std::cout << "reposta do servidor: " << response->payload.response.msg << std::endl;

		//send file
		std::string filename("Win10_22H2_English_x64v1.iso");
		socket->send_file(filename);
	}catch(std::exception& e){
		
		std::cerr << "erro na execução das funções: " << e.what() << std::endl;
	}
}


int main() {
	net::ClientSocket socket;

	try{
		socket.connect(IP, PORT);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	basic_op(socket.build());
    return 0;
}

