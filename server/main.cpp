#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"
#include "controller.hpp"

#include "packet_generated.h"

#include <iostream>
#include <utility>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <filesystem>


#define BACKLOG 10


net::Controller& controller = net::Controller::getInstance();  

void exit_session(const std::string username, int id){
	// TODO should disconnect the other thread 
	controller.remove_session(username, id);
}


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
	/*case Net::Operation_Connect: {
		auto payload = msg->op_as_Connect();
		return std::make_unique<net::Connect>(
			payload->username()->c_str(),
			payload->type(),
			payload->id()
		);
	} break;*/
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
		//return std::make_unique
	} break;
	case Net::Operation_FileData:
	case Net::Operation_Response:
		throw net::ReceptionException(std::string("Unexpected packet at Payload::parse_from_buffer ").append(utils::pckt_type_to_name(msg->op_type())));
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("Didn't match any operation known\n");
	}
	//NOTE: could make a trycatch which catches and sends the error after
}

void update_client (net::Serializer& serde, std::shared_ptr<net::Socket> socket) {
	std::string path = utils::get_sync_dir_path(socket->get_username());
	/* For all files in sync_dir, send it to client */
	for (const auto &entry : std::filesystem::directory_iterator(path)) {
		std::shared_ptr<net::Upload> file = std::make_shared<net::Upload>(entry.path().filename().string().c_str());
		file->is_server = true; 
		file->send(serde, socket);
		file->await_response(serde, socket);
	}
	std::cout << "Ended sync" << std::endl; 
}

pthread_mutex_t mutex_check_directory_exists = PTHREAD_MUTEX_INITIALIZER;

// void *server_loop(std::shared_ptr<net::Socket> socket)
void *server_loop_commands(void *arg){
	std::shared_ptr<net::Socket> socket((net::Socket *) arg);
	net::Serializer serde;
	int id; 
	std::string username; 
	try{
		username = initial_handshake(serde, socket, &id,  true);
	}catch(std::exception e){
		std::cout << "Failed to initialize connection: " << e.what() << std::endl;
	}

	utils::test_and_set_folder(username); 

	std::string session = "Session " + username + " [" + std::to_string(id) + "]"; 
	/* Waits for files to be synched at start */
	while(!controller.is_files_synched(username, id)) {
		sleep(1); 
	 }
	 std::cout << "Starting Command Thread " << session << std::endl; 

	while(1){
		try{
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			std::cout << "Recebido pacote: " << utils::pckt_type_to_name(payload->get_type()) << std::endl;
			payload->reply(serde, socket);
			if (payload->get_type() == Net::Operation_FileMeta || payload->get_type() == Net::Operation_Delete){
				controller.add_data_packet(username, id, payload);
			}
		}catch(const net::CloseConnectionException& e){
			exit_session(session, id);
		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
			//TODO: await a bit, flush the socket and send a ping to see if its ok
		}catch(const std::ios_base::failure& e){
			try {
				auto err_response = serde.build_response(Net::Status_Error, e.what());
				socket->send_checked(err_response);
			}catch(const net::CloseConnectionException& e){
				exit_session(session, id);
			}catch(const std::exception& e){
				std::cout << e.what() << '\n';
			}
		}catch(std::exception e){
			std::cerr << e.what() << std::endl;
		}
	}
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
	controller.set_files_synched(username, id);  

	std::string session = "Session " + username + " [" + std::to_string(id) + "]"; 

	while(1){
		try {
			controller.process_data_packet(username, id, serde, socket);
			sleep(1);  // TODO should be a thread_cond	
			/* Fazer algo pra não ficar o tempo todo travando o mutex de data packet */
			/* Ou não */
		}catch(const net::CloseConnectionException& e){
			exit_session(username, id);
		}catch(const net::ReceptionException& e){
			std::cerr << "Falha ao ler o pacote: " << e.what() << std::endl;
		}catch(const std::ios_base::failure& e){
			try {
				auto err_response = serde.build_response(Net::Status_Error, e.what());
				socket->send_checked(err_response);
			}catch(const net::CloseConnectionException& e){
				exit_session(username, id);
			}catch(const std::exception& e){
				std::cout << e.what() << '\n';
			}
		}catch(std::exception e){
			std::cerr << e.what() << std::endl;
		}
	}
}


int main() {
	net::ServerSocket socket_command_listen_server;
	net::ServerSocket socket_data_listen_server;
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

