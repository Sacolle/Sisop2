#include "sock.hpp"

#include "packet_generated.h"

#include <iostream>

#define PORT "20001"
#define BACKLOG 10

void laco(std::unique_ptr<net::Socket> socket){
	char raw_buff[1024];
	while(1){
		try{
			auto data = socket->read_operation();

			if(data->operation_type != Net::Operation_Connect){
				std::cerr << "erro no teste, operação enviada errada" << std::endl;
				exit(2);
			}
			std::cout << "Cliente connectado: " << data->payload.text << std::endl;
			std::string res("Conectado corretamente!");
			socket->send_response(Net::Status_Ok, res);

			//TODO: write the read_file function

		}catch(const net::TransmissionException& e){
			std::cerr << "erro no envio do pacote" << std::endl;
			//TODO: resend
		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote" << std::endl;
		}catch(const net::CloseConnectionException& e){
			std::cerr << "Cliente desconectado" << std::endl;
			exit(1);
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
			laco(std::move(s));
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

