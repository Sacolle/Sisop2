#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"
#include "controller.hpp"
#include "election_manager.hpp"

#include "packet_generated.h"

#include <iostream>
#include <string>
#include <utility>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <filesystem>

#define BACKLOG 10

// Connect user's session if the user does not have more than 1 session and the id of the session is unique
std::string initial_handshake(net::Serializer& serde, std::shared_ptr<net::Socket> socket, int* id, bool is_command){
	auto buff = socket->read_full_pckt();
	auto pckt = serde.parse_expect(buff, Net::Operation_Connect);
	auto connect_raw = pckt->op_as_Connect();
	net::Connect connect(
		connect_raw->username()->c_str(), 
		connect_raw->type(), 
		connect_raw->id()
	);
	*id = connect_raw->id(); 
	connect.command_connection = is_command; 

	auto& controller = net::Controller::getInstance();  
	if (!controller.add_session(connect.username, connect.id)){
		connect.valid_connection = false; 
		connect.reply(serde, socket);
		std::cout << "Login for user " << connect.username << " and id "  << connect.id << " failed" << std::endl; 
	}
	connect.reply(serde, socket);
	return connect.username;
}

std::shared_ptr<net::Payload> parse_payload(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	case Net::Operation_ListFiles: {
		return std::make_shared<net::ListFiles>();
	} break;
	case Net::Operation_Ping: {
		return std::make_shared<net::Ping>();
	} break;
	case Net::Operation_Exit: {
		return std::make_shared<net::Exit>();
	} break;
	case Net::Operation_Download: {
		auto payload = msg->op_as_Download();
		return std::make_shared<net::Download>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_shared<net::Delete>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		//std::cout << "filemeta" << std::endl;
		return std::make_shared<net::Upload>(
			payload->name()->c_str(),
			payload->size()
		);
	} break;
	case Net::Operation_SendFileRequest: {
		auto payload = msg->op_as_SendFileRequest();
		return std::make_shared<net::SendFileRequest>(
			payload->name()->c_str(),
			payload->hash()
		);
		//TODO:
		//return std::make_shared
	} break;
	case Net::Operation_FileData:
	case Net::Operation_Response:
		throw net::ReceptionException(std::string("Unexpected packet at Payload::parse_from_buffer ")
			.append(utils::pckt_type_to_name(msg->op_type())));
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("Didn't match any operation known\n");
	}
	//NOTE: could make a trycatch which catches and sends the error after
}


// void *server_loop(std::shared_ptr<net::Socket> socket)
void *server_loop_commands(void *arg){
	std::shared_ptr<net::Socket> socket((net::Socket *) arg);
	net::Serializer serde;
	int id; 
	std::string username; 
	try{
		username = initial_handshake(serde, socket, &id,  true);
	}catch(std::exception& e){
		std::cout << "Failed to initialize connection: " << e.what() << std::endl;
	}

	utils::test_and_set_folder(username); 

	std::string session = "Session " + username + " [" + std::to_string(id) + "]"; 

	auto& controller = net::Controller::getInstance();
	/* Waits for files to be synched at start */
	while(!controller.is_files_synched(username, id)) {
		sleep(1); 
	}
	std::cout << "Starting Command Thread " << session << std::endl; 

	while(1){
		try{
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			//std::cout << "Recebido pacote: " << utils::pckt_type_to_name(payload->get_type()) << std::endl;
			payload->reply(serde, socket);
			if (payload->get_type() == Net::Operation_FileMeta || payload->get_type() == Net::Operation_Delete){
				controller.add_data_packet(username, payload);
			}
		}catch(const net::CloseConnectionException& e){
	
			std::cout << controller.remove_session(username, id) << std::endl;
			std::cout << "Saindo da sessão de comandos de " << session << std::endl;
			pthread_exit(0);

		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
			//TODO: await a bit, flush the socket and send a ping to see if its ok
		}catch(const std::ios_base::failure& e){
			try {
				auto err_response = serde.build_response(Net::Status_Error, e.what());
				socket->send_checked(err_response);
			}catch(const net::CloseConnectionException& e){
				
				controller.remove_session(username, id);
				std::cout << "Saindo da sessão de comandos de " << session << std::endl;
				pthread_exit(0);

			}catch(const std::exception& e){
				std::cout << "Failed to send error response, quitting sessiong anyways: " << e.what() << '\n';
				controller.remove_session(username, id);
				pthread_exit(0);
			}
		}catch(std::exception& e){
			std::cerr << "Generic exception at top level, unacounted failure at server_loop_commands of "
			<< username << ":\n\t"
			<< e.what() 
			<< ".\n\tExiting session..."
			<< std::endl;
			exit(1);
		}
	}
}

void update_client (net::Serializer& serde, std::shared_ptr<net::Socket> socket) {
	std::string path = utils::get_sync_dir_path(socket->get_username());
	/* For all files in sync_dir, send it to client */
	for (const auto &entry : std::filesystem::directory_iterator(path)) {
		net::Upload file(entry.path().filename().string().c_str());
		file.is_server = true; 
		file.send(serde, socket);
		file.await_response(serde, socket);
	}
	std::cout << "Ended sync" << std::endl; 
}

/* Loop para a thread de sincronização de arquivos entre sessions */
/* Encaminha pacotes de uma session para outra */
void *server_loop_data(void *arg) {
	std::shared_ptr<net::Socket> socket((net::Socket *) arg);
	net::Serializer serde;
	std::string username;
	int id;
	try{
		username = initial_handshake(serde, socket, &id, false);
	}catch(std::exception e){
		std::cout << "Failed to initialize connection: " << e.what() << std::endl; 
	}

	utils::test_and_set_folder(username);

	/* Overwrite what the clients has - in order to persist server data */
	update_client(serde, socket);

	auto& controller = net::Controller::getInstance();
	controller.set_files_synched(username, id);  

	std::string session = "Session " + username + " [" + std::to_string(id) + "]"; 

	std::cout << "Starting Data Thread " << session << std::endl; 

	while(1){
		try {
			auto payload_opt = controller.get_data_packet(username, id);
			if(payload_opt.has_value()){
				std::cout << "Data thread of " << session << " has packet: " 
				<< utils::pckt_type_to_name(payload_opt.value()->get_type()) << std::endl;


				auto payload = payload_opt.value();
				if (payload->get_type() == Net::Operation_FileMeta) {
					static_cast<net::Upload*>(payload.get())->is_server = true; 
				}
				payload->send(serde, socket);
				payload->await_response(serde, socket);
			}
		}catch(const net::CloseConnectionException& e){
			
			controller.remove_session(username, id);
			std::cout << "Saindo da sessão de dados de " << session << std::endl;
			pthread_exit(0);

		}catch(const net::CloseSessionException& e){ //ocorre quando não há sessão correspondente
			//normalmente quando o cmd thread quita
			//emitido por controller.get_data_packet
			std::cout << "Saindo da sessão de dados de " << session << std::endl;
			pthread_exit(0);

		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
		}catch(const std::ios_base::failure& e){
			try {
				auto err_response = serde.build_response(Net::Status_Error, e.what());
				socket->send_checked(err_response);
			}catch(const net::CloseConnectionException& e){
				
				controller.remove_session(username, id);
				std::cout << "Saindo da sessão de dados de " << session << std::endl;
				pthread_exit(0);

			}catch(const std::exception& e){

				std::cout << "Failed to send error response, quitting sessiong anyways: " << e.what() << '\n';
				controller.remove_session(username, id);
				pthread_exit(0);

			}
		}catch(std::exception& e){
			std::cerr << "Generic exception at top level, unacounted failure at server_loop_data of "
			<< username << ":\n\t"
			<< e.what() 
			<< ".\n\tExiting session..."
			<< std::endl;
			exit(1);
		}
	}
}


void* election_socket_setup(void* s){


}

int main(int argc, char** argv) {
	//meu valor numero de replicações
	//ip - porta - valor
	if(argc < 3){
		std::cout << "numero insuficiente de argumentos" << std::endl;
		exit(1);
	}
	int election_value = std::stoi(argv[1]);
	int number_of_replications = std::stoi(argv[2]);

	//primeira vez q chama get instance, precisa passar o valor de eleição
	{ net::ElectionManager::getInstance(election_value); }
	

	net::ServerSocket socket_command_listen_server(true);
	net::ServerSocket socket_data_listen_server(true);
	
	// Conecta a socket de comandos
	try{
		socket_command_listen_server.open(PORT_COMMAND, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	// Conecta a socket de dados
	try{
		socket_data_listen_server.open(PORT_DATA, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	net::ServerSocket socket_election_recv;
	try{
		socket_election_recv.open(PORT_ELECTION, BACKLOG);
	}catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	//fazer as threads

	while(1){
		try {
			auto s_commands = socket_command_listen_server.accept();
			auto s_data = socket_data_listen_server.accept();
			// Cria nova thread e começa o server loop nela 
			// A thread atual se mantém a espera de conexão 
			if (s_commands != nullptr){
				s_commands->print_address();
				pthread_t t_commands;
				pthread_create(&t_commands, NULL, server_loop_commands, s_commands);
			}
			if (s_data != nullptr){
				s_data->print_address();
				pthread_t t_data;
				pthread_create(&t_data, NULL, server_loop_data, s_data);
			}
			if (s_commands == nullptr && s_data == nullptr){
				// TODO busy wait
			}
		}catch(const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}
	return 0;
}

