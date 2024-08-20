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
#include <poll.h>
#include <iterator>
#include <filesystem>

#define BACKLOG 10

pthread_mutex_t g_replicator_mutex = PTHREAD_MUTEX_INITIALIZER;


void send_packet_to_replicas(net::Serializer& serde, std::shared_ptr<net::Payload>& payload){
	auto& election_manager = net::ElectionManager::getInstance(); 
	pthread_mutex_lock(&g_replicator_mutex);
	for(auto s_opt: election_manager.get_send_sockets()){
		try {
			if(!s_opt.has_value()) continue;
			auto s = s_opt.value();
			std::shared_ptr<net::Payload> replicated_payload(payload->clone());
			if (replicated_payload->get_type() == Net::Operation_FileMeta){
				auto upload = std::dynamic_pointer_cast<net::Upload>(replicated_payload);
				upload->is_server = true; 
			}
			std::cout << "Sending to replica: " << utils::pckt_type_to_name(replicated_payload->get_type()) << std::endl;
			replicated_payload->send(serde, s);
			replicated_payload->await_response(serde, s);
		} catch(const net::ReceptionException& e){
			std::cerr << "Error reading replica response: " << e.what() << std::endl;
		} catch(const net::CloseConnectionException& e){
			std::cerr << "Lost connection with replica: " << e.what() << std::endl; 
			election_manager.remove_send_socket(s_opt.value()); 
		} catch(const std::exception& e){
			std::cerr << "Unexpected exception while sending packet to replica: " << e.what() << std::endl; 
		}
	}
	pthread_mutex_unlock(&g_replicator_mutex);
}

// Connect user's session if the user does not have more than 1 session and the id of the session is unique
std::string initial_handshake(net::Serializer& serde, std::shared_ptr<net::Socket> socket, int* id, bool is_command){
	auto& election_manager = net::ElectionManager::getInstance();
	auto buff = socket->read_full_pckt();
	auto pckt = serde.parse_expect(buff, Net::Operation_Connect);
	auto connect_raw = pckt->op_as_Connect();
	net::Connect connect(
		connect_raw->username()->c_str(), 
		connect_raw->id(),
		election_manager.data_port.c_str(),
		connect_raw->coordinator_port()->str()
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

	std::shared_ptr<net::Payload> payload_client_info = std::make_shared<net::ClientInfo>(
		socket->get_their_ip(),
		connect_raw->coordinator_port()->str(),
		true
	);
	send_packet_to_replicas(serde, payload_client_info); 

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
			payload->filename()->c_str(), payload->username()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		//std::cout << "filemeta" << std::endl;
		return std::make_shared<net::Upload>(
			payload->name()->c_str(),
			payload->size(), payload->username()->c_str()
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

	auto& election_manager = net::ElectionManager::getInstance();

	while(1){
		try{
			/* Replicação de dados */
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			//std::cout << "Recebido pacote: " << utils::pckt_type_to_name(payload->get_type()) << std::endl;
			
			if (payload->get_type() == Net::Operation_FileMeta){
				std::shared_ptr<net::Upload> upload = std::dynamic_pointer_cast<net::Upload>(payload); 

				//excuta o efeito do payload
				auto res = upload->recv_and_save_file(serde, socket);
				//manda para as replicas
				send_packet_to_replicas(serde, payload); 
				//depois manda um ok para o cliente
				upload->send_response(serde, socket, Net::Status_Ok, res);
				controller.add_data_packet(username, payload);

			}else if (payload->get_type() == Net::Operation_Delete){
				std::shared_ptr<net::Delete> del = std::dynamic_pointer_cast<net::Delete>(payload); 

				//excuta o efeito do payload
				auto res = del->delete_file();
				//manda para as replicas
				send_packet_to_replicas(serde, payload); 
				//depois manda um ok para o cliente
				del->send_response(serde, socket, Net::Status_Ok, res);
				controller.add_data_packet(username, payload);
			}else{
				payload->reply(serde, socket);
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
		net::Upload file(entry.path().filename().string().c_str(), socket->get_username().c_str());
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
	int replicacoes = *((int*) s);
	std::string ip, porta;
	int valor;
	net::Serializer serde;

	auto& election_manager = net::ElectionManager::getInstance();

	//wait for everyone to open the ports
	sleep(10);
	try{
		for(int i = 0; i < replicacoes; i++){
			std::cin >> ip >> porta >> valor;
// Possivelmente é melhor ter um coordenador inicial para deixar responsável pelo setup e fazer a sincronização inicial
			std::cout << "Creating socket " << ip << ":" << porta << std::endl;
			net::ClientSocket socket;
			socket.connect(ip.c_str(), porta.c_str());

			auto s = socket.build();
			s->election_weight = valor;
			
			//manda essa msg para saber o peso da conexão
			//importante depois na votação
			//fazendo desse jeito seco, pq sim
			auto pckt = serde.build_connect("", election_manager.valor);
			s->send_checked(pckt);
			
			election_manager.add_send_socket(s);
		}
	}catch(const net::NetworkException& e){
		std::cout << "Setup failed:\n" << e.what() << std::endl;
		exit(1);
	}
	pthread_exit(0);

	return nullptr;
}

void setup_election(char* election_port, int number_of_replications){

	auto& election_manager = net::ElectionManager::getInstance();
	net::Serializer serde;
	net::ServerSocket socket_election_recv;

	try{
		socket_election_recv.open(election_port, BACKLOG);
	}catch(const net::NetworkException& e){
		std::cerr << "Failed to open socket:\n" << e.what() << '\n';
		exit(1);
	}
	
	//cria a thread para estabelecer as conexões
	pthread_t election_connect;
	pthread_create(&election_connect, NULL, election_socket_setup, &number_of_replications);
	std::cout << "Starting setup " << std::endl; 
	//aceita number_of_replications conexões
	for(int i = 0; i < number_of_replications; i++){
		auto socket_ptr = socket_election_recv.accept();
		
		//NOTE: usando o pacote connect para passar o peso da socket
		auto buff = socket_ptr->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Connect);
		auto connect = pckt->op_as_Connect();
		
		//connect->id() contém o peso da socket
		socket_ptr->election_weight = connect->id();
		std::cout << "peso é " << connect->id() << std::endl;

		std::shared_ptr<net::Socket> s(socket_ptr);
		election_manager.add_recv_socket(s);
	}
	//espera se conetar a todas as replicações
	pthread_join(election_connect, NULL);

	//começa uma eleição
	election_manager.set_in_election(true);
}

//does the eleição LOL
void do_the_election(){
	net::Serializer serde;
	auto& election_manager = net::ElectionManager::getInstance();

	std::list<std::shared_ptr<net::Socket>> sockets_to_remove;

	election_manager.set_in_election(true);

	net::Election election(election_manager.valor);

	std::cout << "meu peso é: " << election_manager.valor << std::endl;
	//std::cout << "loopando pelos sockets" << std::endl;

	for(auto s_opt: election_manager.get_send_sockets()){
		if(!s_opt.has_value()) continue;
		auto s = s_opt.value();
		//std::cout << "socket de peso: " << s->election_weight <<std::endl;
		if(s->election_weight > election_manager.valor){
			//std::cout << "sending election to peso: " << s->election_weight <<std::endl;
			try{
				election.send(serde, s);
				//std::cout << "esperando resposta" << std::endl;
				election.await_response(serde, s);
			}catch(...){
				//std::cout << "removendo socket de ip: " << s->get_their_ip() << std::endl;
				sockets_to_remove.push_back(s);
			}
		}
	}
	//remove as sockets que foram desconectadas
	for(auto s: sockets_to_remove){
		election_manager.remove_send_socket(s);
	}
	sockets_to_remove.clear();


	if(!election.got_response()){
		//esse cara aqui eh o coordenador
		//NOTE: espera todos os election serem mandados e lidos e tal
		sleep(2);
		net::Coordinator coordinator;
		for(auto s_opt: election_manager.get_send_sockets()){
			if(!s_opt.has_value()) continue;
			auto s = s_opt.value();
			try{
				coordinator.send(serde, s);
				coordinator.await_response(serde, s);
			}catch(...){
				std::cout << "removendo socket de ip: " << s->get_their_ip() << std::endl;
				sockets_to_remove.push_back(s);
			}
		}
		//TODO: faz aqui as alterações no election_manager
		election_manager.set_is_coordinator(true);
		std::cout << "eu sou o coordenador" << std::endl;
		std::list<std::shared_ptr<net::ClientInfo>> clients_to_remove; 
		for(auto& client_info: election_manager.get_clients_info()){
			net::ClientSocket notify_clients;
			std::cout << "Sending connect to client " << client_info->ip << ":" << client_info->port << std::endl; 
			try{
				notify_clients.connect(client_info->ip.c_str(), client_info->port.c_str());
			}catch(const net::NetworkException& e){
				std::cout << "Failed to connect to client: " << client_info->ip << ":" << client_info->port << std::endl;
				clients_to_remove.push_back(client_info); 
				continue;
			}
			auto s = notify_clients.build();
			net::RedefineServer redefine_server(election_manager.root_port.c_str()); //my port
			try {
				redefine_server.send(serde, s);
				redefine_server.await_response(serde, s);
			} catch(...){
				std::cout << "Falha em comunicar com o ip: " << s->get_their_ip() << std::endl;
			}
		}
		for (auto& client_info : clients_to_remove){
			election_manager.remove_client_info(client_info); 
		}		

		election_manager.set_in_election(false);
	}
	//remove as sockets que removem erro
	for(auto s: sockets_to_remove){
		election_manager.remove_send_socket(s);
	}
	sockets_to_remove.clear();
	while(election_manager.in_election());
	election_manager.remove_staged_send_socket();	
}

std::shared_ptr<net::Payload> parse_election_payload(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	case Net::Operation_Coordinator: {
		return std::make_shared<net::Coordinator>();
	} break;
	case Net::Operation_Election: {
		auto payload = msg->op_as_Election();
		return std::make_shared<net::Election>(
			net::ElectionManager::getInstance().valor,
			payload->weight()
		);
	} break;
	case Net::Operation_Response:
		throw net::ReceptionException(std::string("Unexpected packet at Payload::parse_from_buffer ")
			.append(utils::pckt_type_to_name(msg->op_type())));
		break;
	default:
		throw net::ReceptionException("Didn't match any operation known\n");
	}
}


void* election_observer(void* args){
	auto& election_manager = net::ElectionManager::getInstance();
	net::Serializer serde;

	auto& recv_sockets = election_manager.get_recv_sockets();

	#define MAX_CONNECTIONS 5
	struct pollfd poll_fds[MAX_CONNECTIONS];

	int count = 0;
	for(auto s_opt: recv_sockets){
		if(!s_opt.has_value()) continue;
		auto s = s_opt.value();

		poll_fds[count].fd = s->get_fd();
		poll_fds[count].events = POLLIN | POLLPRI;
		count++;
	}
	std::list<std::shared_ptr<net::Socket>> sockets_to_remove;

	while(1){
		int poll_num = poll(poll_fds, count, -1);

		if (poll_num == -1) {
			if (errno == EINTR)
				continue;
			perror("poll");
			exit(EXIT_FAILURE);
		}
		if (poll_num > 0) {
			for(int i = 0; i < count; i++){
				if(poll_fds[i].revents & POLLIN){

					auto opt_s = recv_sockets[i];
					if(!opt_s.has_value()) continue;
					auto s = opt_s.value();
					try{
						auto pckt = s->read_full_pckt();
						//NOTE: talvez dê problemas de receber election depois de receber coordinator
						auto payload = parse_election_payload(pckt);
						payload->reply(serde, s);
						if (payload->get_type() == Net::Operation_Coordinator){
							//se recebeu uma msg coordinator, seta a socket como a coordinator socket
							//e remove ele da lista de election
							election_manager.set_coordinator_socket(s);
							election_manager.stage_send_socket_to_remove(s);
							sockets_to_remove.push_back(s);
							std::cout << "Não sou o coordenador" << std::endl;
							election_manager.set_in_election(false);
							break;
						}else{
							election_manager.set_in_election(true);
						}
					}catch(...){
						sockets_to_remove.push_back(s);
					}
				}
			}
		}
		bool changed_socket_list = false;
		for(auto s: sockets_to_remove){
			election_manager.remove_recv_socket(s);
			changed_socket_list = true;
		}
		sockets_to_remove.clear();
		
		//se removeu uma socket da lista tem q resetar a lista do poll
		if(changed_socket_list){
			for(auto s: recv_sockets){
				if(!s.has_value()){
					poll_fds[count].fd = -1;
					poll_fds[count].events = POLLHUP;
				}
			}
		}
	}
}

std::shared_ptr<net::Payload> parse_server_replication(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	case Net::Operation_IpInformation: {
		auto payload = msg->op_as_IpInformation();
		return std::make_shared<net::ClientInfo>(
			payload->ip()->c_str(), payload->port()->c_str(), payload->isConnected()
		);
	} break; 
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_shared<net::Delete>(
			payload->filename()->c_str(), payload->username()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		auto upload = std::make_shared<net::Upload>(
			payload->name()->c_str(),
			payload->size(), payload->username()->c_str()
		);
		return upload;
	} break;
	case Net::Operation_Response:
		throw net::ReceptionException(std::string("Unexpected packet at Payload::parse_from_buffer ")
			.append(utils::pckt_type_to_name(msg->op_type())));
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("Didn't match any operation known\n");
	}
}

int main(int argc, char** argv) {

	/* Na chamada do ./server, os seguintes argumentos são necessários */
	/* election_value -> valor de id para eleição, deve concordar com o valor nos arquivos txt de setup */
	/* number_of_replications -> número de replicações, sem contar a chamada atual de ./server */
	/* cmd_port -> porta arbitrária; vai ser usada como argumento <port_comandos> (3º argumento) do cliente */
	/* data_port -> porta arbitrária */
	/* election_port -> porta de eleição, deve concordar com o valor nos arquivos txt de setup */

	/* Além dos argumentos, deve se passar via pipe um arquivo txt contendo as seguintes informações, */
	/* Para cada replicação, além da chamada atual, uma linha com o seguinte formato: */
	/* <ip_replica> <porta_replica> <value_replica> */
	/* onde, */
	/* ip_replica -> ip da máquina onde está localizada a réplica */
	/* porta_replica -> porta de eleição da replica, referente a election_port na chamada de ./server da réplica */
	/* value_replica -> valor de id para eleição, referente a election_value na chamada de ./server da réplica */

	if(argc < 6){
		std::cout << "numero insuficiente de argumentos" << std::endl;
		exit(1);
	}
	int election_value = std::stoi(argv[1]);
	int number_of_replications = std::stoi(argv[2]);

	char* cmd_port = argv[3];
	char* data_port = argv[4];
	char* election_port = argv[5];

	//primeira vez q chama get instance, precisa passar o valor de eleição
	auto& election_manager = net::ElectionManager::getInstance(election_value, cmd_port, data_port); 
	net::Serializer serde;

	net::ServerSocket socket_command_listen_server(true);
	net::ServerSocket socket_data_listen_server(true);
	
	// Conecta a socket de comandos
	try{
		socket_command_listen_server.open(cmd_port, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	// Conecta a socket de dados
	try{
		socket_data_listen_server.open(data_port, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	setup_election(election_port, number_of_replications);
	pthread_t election_observer_handler;
	pthread_create(&election_observer_handler, NULL, election_observer, NULL);
	std::cout << "iniciou a eleição" << std::endl;
	do_the_election();
	
	while(1){
		if(!election_manager.is_coordinator()){
			while(1){
				std::cout << "recebendo pacotes do main" << std::endl;
				if(election_manager.in_election()) break;
				try{
					auto coord_socket = election_manager.get_coordinator_socket();
					auto pckt = coord_socket->read_full_pckt();
					auto payload = parse_server_replication(pckt);
					if (payload->get_type() == Net::Operation_IpInformation) {
						std::shared_ptr<net::ClientInfo> clientInfo = std::dynamic_pointer_cast<net::ClientInfo>(payload); 
						if (clientInfo->isConnected){
							election_manager.add_client_info(clientInfo); 
						} else {
							election_manager.remove_client_info(clientInfo); 
						}
					}
					payload->reply(serde, coord_socket);
				}catch(const net::CloseConnectionException& e){
					std::cout << "Coordenador desconectou" << std::endl;
					break;
				}catch(const std::exception& e){
					std::cout << "Erro na replicação:\n" << e.what() << std::endl;
				}
			}
			do_the_election();

		}else{
			std::cout << "recebendo conexões" << std::endl;
			/* Se chegar a esse ponto, significa que o RM é o coordenador */
			while(1){
				try {
					auto s_commands = socket_command_listen_server.accept();
					auto s_data = socket_data_listen_server.accept();
					// Cria nova thread e começa o server loop nela 
					// A thread atual se mantém a espera de conexão 
					//TODO: mandar para as replicações os IDs dos clientes
					if (s_commands != nullptr){
						//mutex
						//for send_socket -> envia o ip e porta do kr
						//guardar info no election_manager (com mutex)
						//cria a pasta com o payload
						//mutex

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
					std::cerr << "erro em aceitar clientes:\n" << e.what() << '\n';
				}
			}
		}
	}
	return 0;
}

