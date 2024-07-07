#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"

#include "packet_generated.h"

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT "20001"
#define BACKLOG 10
// Arquivo Histórico .fbs == .facebook 
// Arquivo histórico: "lacall"

//read an operation:
// supoem-se que já conectou -> sabe-se o nome do usuário e validou o nº de conexões
/*
	## REQ
	- listfiles -> sends the names of the files in the sync_dir
	- dowload -> checks if has the file, then sends the file to the client
	- delete -> deletes the file and sends a response 
	- connect -> saves the user name and sends an ok
		|> all receives a simple text and respondes with simple text
	
	- get_sync_dir -> sends the files to the client
		|> receives simple text, sends a lot of files
		|> needs a finish message -> finished sending or 
			an inicial message, telling how many files will be sent

	- upload -> receives the file from the client and saves it in it's sync_dir
		|> receives a file and sends a simple ok

	## RES
	- OK + data -> delete, listfiles, upload
	- OK + file -> dowload, get_sync_dir
	- ERR
*/


//faz o inicio da conexão, 
//checando o número de pessoas conectadas (max 2)
//e setando o nome do user dessa socket
std::string initial_handshake(net::Serializer& serde, std::shared_ptr<net::Socket> socket){
	
	//check num of conections
	auto buff = socket->read_full_pckt();
	auto pckt = serde.parse_expect(buff, Net::Operation_Connect);
	auto connect_raw = pckt->op_as_Connect();
	net::Connect connect(
		connect_raw->username()->c_str(), 
		connect_raw->type(), 
		connect_raw->id()
	);
	connect.reply(serde, socket);
	return connect.username;
}

std::unique_ptr<net::Payload> parse_payload(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	/*case Net::Operation_Connect: {
		auto payload = msg->op_as_Connect();
		return std::make_unique<net::Connect>(
			payload->username()->c_str(),
			payload->type(),
			payload->id()
		);
	} break;*/
	case Net::Operation_ListFiles: {
		return std::make_unique<net::ListFiles>();
	} break;
	case Net::Operation_Ping: {
		return std::make_unique<net::Ping>();
	} break;
	case Net::Operation_Exit: {
		return std::make_unique<net::Exit>();
	} break;
	case Net::Operation_Download: {
		auto payload = msg->op_as_Download();
		return std::make_unique<net::Download>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_unique<net::Delete>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		//std::cout << "filemeta" << std::endl;
		return std::make_unique<net::Upload>(
			payload->name()->c_str(),
			payload->size()
		);
	} break;
	case Net::Operation_SendFileRequest: {
		auto payload = msg->op_as_SendFileRequest();
		return std::make_unique<net::SendFileRequest>(
			payload->name()->c_str(),
			payload->hash()
		);
		//TODO:
		//return std::make_unique
	} break;
	case Net::Operation_FileData:
	case Net::Operation_Response:
		throw net::ReceptionException("Unexpected packet at Payload::parse_from_buffer why dog");
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("didn't match any operation known");
	}
	//NOTE: could make a trycatch which cathes and sends the error after
}

void server_loop(std::shared_ptr<net::Socket> socket){
	net::Serializer serde;
	std::string username; 
	try{
		username = initial_handshake(serde, socket);
	}catch(...){
		std::cout << "failed to initialize connection" << '\n';
		exit(1);
	}
	
	std::string userfolder = "sync_dir_";
	userfolder.append(username);

	// Create Directory if doesnt exist
	struct stat folder_st = {0};
	if (stat(userfolder.c_str(), &folder_st) == -1) {
		mkdir(userfolder.c_str(), 0700);
	}

	while(1){
		try{
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			std::cout << "Recebido pacote: " << utils::pckt_type_to_name(payload->get_type()) << std::endl;
			payload->reply(serde, socket);
		}catch(const net::CloseConnectionException& e){
			std::cerr << "Cliente desconectado" << std::endl;
			exit(1);
		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
			//TODO: await a bit, flush the socket and send a ping to see if its ok
		}catch(const std::ios_base::failure& e){
			try {
				auto err_response = serde.build_response(Net::Status_Error, e.what());
				socket->send_checked(err_response);
			}catch(const net::CloseConnectionException& e){
				std::cerr << "Cliente desconectado" << std::endl;
				exit(1);
			}catch(const std::exception& e){
				std::cout << e.what() << '\n';
			}
		}catch(...){
			std::cout << "idk man maybe" << std::endl;
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
			s->print_address();
			server_loop(s);
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
    return 0;
}

