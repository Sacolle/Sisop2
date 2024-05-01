#include "sock.hpp"

#include "packet_generated.h"

#include <iostream>

#define PORT "20001"
#define BACKLOG 10
// Arquivo Histórico .fbs == .facebook 
// Arquivo histórico: "lacall"
void server_loop(std::unique_ptr<net::Socket> socket){
	char raw_buff[1024];
	bool isConnected = false; // solução temporária -> Supõe-se que não vai funcionar pra múltiplos clientes/threads
	while(1){
		try{
			auto data = socket->read_operation();
			
			/* Garante que a primeira operação é Operation_Connect  */
			if(!isConnected && data->operation_type != Net::Operation_Connect){
				std::cerr << "Servidor: erro no teste, primeira operação enviada não é um Connect" << std::endl;
				exit(2);
			} else if (!isConnected) {
				/* Operação de conexão server-side */
				isConnected = true;
				std::cout << "Cliente connectado: " << data->payload.text << std::endl;
				std::string res("Conectado corretamente!");
				socket->send_response(Net::Status_Ok, res);
			}
			std::cout << (int)data->operation_type << std::endl; 
			if ( data->operation_type == Net::Operation_FileMeta ){
				socket->receive_file(data->payload.filemeta.name, data->payload.filemeta.size); 
			}
			

			//TODO: write the read_file function

		}catch(const net::TransmissionException& e){
			std::cerr << "Erro no envio do pacote" << std::endl;
			//TODO: resend
		}catch(const net::ReceptionException& e){
			// TODO: Consertar erro ao enviar um arquivo com 0 bytes
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
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
			server_loop(std::move(s));
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

