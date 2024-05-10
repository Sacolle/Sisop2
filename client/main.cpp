#include <iostream>
#include <memory>
#include <string>

#include "packet_generated.h"

#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"

#define IP "127.0.0.1"
#define PORT "20001"

std::unique_ptr<net::Payload> get_cli_payload(){
	std::string cmd, args;
	while(1){
		cmd.clear();
		args.clear();
		std::cin >> cmd;
		std::getline(std::cin, args);
		utils::trim(args);

		try{
			if(cmd == "upload"){
				return std::make_unique<net::Upload>(args.c_str());
			}else if(cmd == "download"){
				//clean_file == true, pois esse arquivo aberto é o arquivo para colocar as informações dos pckts
				return std::make_unique<net::Download>(args.c_str(), true);
			}else if(cmd == "delete"){
				return std::make_unique<net::Delete>(args.c_str());
			}else if(cmd == "list"){
				if(args == "server"){
					return std::make_unique<net::ListFiles>();
				}else if(args == "client"){
					std::cout << "arquivos do cliente são: " << std::endl;
				}else{
					std::cout << "Args " << args << " não reconhecido.\n";
					std::cout << "Tente: client ou server." << std::endl;
				}
			}else if(cmd == "exit"){
				return std::make_unique<net::Exit>();
			}else if(cmd == "ping"){
				return std::make_unique<net::Ping>();
			}else{
				std::cout << "Comando " << cmd << " não reconhecido.\n";
				std::cout << "Tente: upload, download, delete, list ou exit." << std::endl;
			}
		}catch(const std::exception& e){
			std::cout << "Error: " << e.what() << std::endl;
		}
	}
}

void basic_op(std::shared_ptr<net::Socket> socket){
	net::Serializer serde;
	std::string nome("alberto");
	while(1){
		try{
			auto payload = get_cli_payload();
			payload->send(serde, socket);
			payload->await_response(serde, socket);
		}catch(std::exception& e){
			std::cerr << "erro na execução das funções: " << e.what() << std::endl;
		}
	}
}

int main() {
	net::ClientSocket socket;
	try {
		socket.connect(IP, PORT);
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	basic_op(socket.build());
    return 0;
}

