#include <iostream>
#include <memory>
#include <string>

#include "packet_generated.h"

#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"

std::unique_ptr<net::Payload> get_cli_payload(){
	std::string cmd, args;
	while(1){
		cmd.clear();
		args.clear();
		std::cin >> cmd;
		std::getline(std::cin, args);
		utils::trim(args);

		try{
			if(cmd == "upload"){ return std::make_unique<net::Upload>(args.c_str());
			}else if(cmd == "sendreq"){ return std::make_unique<net::SendFileRequest>(args.c_str());
			}else if(cmd == "delete"){ return std::make_unique<net::Delete>(args.c_str());
			}else if(cmd == "exit"){ return std::make_unique<net::Exit>();
			}else if(cmd == "ping"){ return std::make_unique<net::Ping>();
			}else if(cmd == "download"){
				//clean_file == true, pois esse arquivo aberto é o arquivo para colocar as informações dos pckts
				return std::make_unique<net::Download>(args.c_str());
			}else if(cmd == "list"){
				if(args == "server"){
					return std::make_unique<net::ListFiles>();
				}else if(args == "client"){
					std::cout << "arquivos do cliente são: " << std::endl;
				}else{
					std::cout << "Args " << args << " não reconhecido.\n";
					std::cout << "Tente: client ou server." << std::endl;
				}
			}else{
				std::cout << "Comando " << cmd << " não reconhecido.\n";
				std::cout << "Tente: upload, download, delete, list ou exit." << std::endl;
			}
		}catch(const std::exception& e){
			std::cout << "Error: " << e.what() << std::endl;
		}
	}
}

void initial_handshake(net::Serializer& serde, std::shared_ptr<net::Socket> socket, const char* username){
	//TODO: read the connect info from args[]
	net::Connect connect(
		username, 
		Net::ChannelType_Main,
		0
	);
	connect.send(serde, socket);
	connect.await_response(serde, socket);
}

void basic_op(std::shared_ptr<net::Socket> socket, const char* username){
	net::Serializer serde;
	try{
		initial_handshake(serde, socket, username);
	}catch(...){
		std::cout << "failed to initialize connection" << '\n';
		exit(1);
	}

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

int main(int argc, char** argv){
	if(argc < 4){
		std::cerr << "argumentos insuficientes para começar o servidor" << '\n';
		std::cerr << "inicie no padrão: ./client <username> <server_ip_address> <port>" << std::endl;
		exit(2);
	}

	net::ClientSocket socket;
	try {
		socket.connect(argv[2], argv[3]);
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	basic_op(socket.build(), argv[1]);
    return 0;
}

