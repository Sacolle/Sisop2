#include <iostream>
#include <memory>
#include <string>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <filesystem>
#include <utility>

#include "packet_generated.h"

#include "mutex.hpp"
#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"
#include "exceptions.hpp"

#define BACKLOG 10
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))


pthread_mutex_t mutex_close_threads = PTHREAD_MUTEX_INITIALIZER;
net::Mutex<bool> g_close_cmd_thread = net::Mutex(false);
net::Mutex<bool> g_close = net::Mutex(false);
std::pair<std::shared_ptr<net::Socket>, std::shared_ptr<net::Socket>> connect_client(
	const std::string& ip, const std::string& port, int id, const std::string& username, net::Serializer& serde);

net::ServerSocket wait_for_coordinator_socket_listen(true);
int client_loop_commands(	std::shared_ptr<net::Socket> socket,
							const std::string& username,
							int id);
void *client_loop_data(void *arg);


std::string username_global; 
net::Payload* get_cli_payload(std::string &cmd, std::string& args){
	//TODO: parse
	utils::trim(cmd);
	utils::trim(args);

	try{
		if(cmd == "upload"){ return new net::Upload(args.c_str(), username_global.c_str());
		}else if(cmd == "sendreq"){ return new net::SendFileRequest(args.c_str());
		}else if(cmd == "delete"){ return new net::Delete(args.c_str(), username_global.c_str());
		}else if(cmd == "exit"){
			g_close.lock().get() = true;
		}else if(cmd == "ping"){ return new net::Ping();
		}else if(cmd == "download"){
			return new net::Download(args.c_str());
		}else if(cmd == "list_server" || cmd == "list_client"){
			return new net::ListFiles();
		}else{
			std::cout << "Comando " << cmd << " não reconhecido.\n";
			std::cout << "Tente: upload, download, delete, list ou exit." << std::endl;
			return nullptr;
		}
	}catch(const std::exception& e){
		std::cout << "Error: " << e.what() << std::endl;
	}
	return nullptr;
}

std::shared_ptr<net::Payload> parse_payload(uint8_t* buff){
	auto msg = Net::GetSizePrefixedPacket(buff);

	switch (msg->op_type()){
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_shared<net::Delete>(
			payload->filename()->c_str(), payload->username()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		//std::cout << "filemeta" << std::endl;
		auto upload = std::make_shared<net::Upload>(
			payload->name()->c_str(),
			payload->size(),
			payload->username()->c_str()
		);
		upload->is_server = true;
		return upload;
	} break;
	case Net::Operation_IpInformation: {
		auto payload = msg->op_as_IpInformation();
		auto redefine_server = std::make_shared<net::RedefineServer>(
			payload->port()->c_str()
		);
		return redefine_server; 
	} break; 
	case Net::Operation_ListFiles:
	case Net::Operation_Ping:
	case Net::Operation_Exit:
	case Net::Operation_Download:
	case Net::Operation_SendFileRequest:
	case Net::Operation_FileData:
		throw net::ReceptionException(
			std::string("Unexpected packet at Payload::parse_from_buffer ")
				.append(utils::pckt_type_to_name(msg->op_type()))
		);
		break;
	case Net::Operation_Response: //TODO: se for response gerar outra exceção
		throw net::ReceptionException(
			std::string("Unexpected packet Payload::parse_from_buffer ")
				.append(utils::pckt_type_to_name(msg->op_type()))
				.append(msg->op_as_Response()->msg()->str())
		);
		break;
	default:
		//didn't match any operation known
		throw net::ReceptionException("Didn't match any operation known\n");
	}
}

std::string initial_handshake(
	net::Serializer& serde, 
	std::shared_ptr<net::Socket> socket, 
	const char* username, 
	int id, 
	const std::string& coordinator_port
	){
	//TODO: read the connect info from args[]
	net::Connect connect(
		username, 
		id,
		"",
		coordinator_port
	);
	connect.send(serde, socket);
	connect.await_response(serde, socket);
	return connect.data_port; 
}

void execute_payload(net::Serializer& serde, std::shared_ptr<net::Socket> socket, std::string& cmd, std::string& args){
	/*
	net::Serializer serde;
	*/
	net::Payload* payload;
	try{
		payload = get_cli_payload(cmd, args);
		if(payload == nullptr) return;

		payload->send(serde, socket);
		payload->await_response(serde, socket);

	}catch(std::exception& e){
		std::cerr << "erro na execução das funções: " << e.what() << std::endl;
	}
	delete payload;
}

 void handle_events(int inotify_fd, net::Serializer& serde, std::shared_ptr<net::Socket> socket){
	/* Some systems cannot read integer variables if they are not
		properly aligned. On other systems, incorrect alignment may
		decrease performance. Hence, the buffer used for reading from
		the inotify file descriptor should have the same alignment as
		struct inotify_event. */
	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;

	/* Loop while events can be read from inotify file descriptor. */
	for (;;) {
		/* Read some events. */
		ssize_t len = read(inotify_fd, buf, sizeof(buf));
		if (len == -1 && errno != EAGAIN) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		/* If the nonblocking read() found no events to read, then
			it returns -1 with errno set to EAGAIN. In that case,
			we exit the loop. */
		if (len <= 0)
			break;

		/* Loop over all events in the buffer. */
		for (char *ptr = buf; ptr < buf + len;
				ptr += sizeof(struct inotify_event) + event->len) {

			event = (const struct inotify_event *) ptr;

			const char* filename = event->name;
			//std::cout << "[FILE] " << filename << std::endl;
			if(utils::is_tmp_file(filename)){
				continue;
			}

			const bool isdir = event->mask & IN_ISDIR;

			#define MASKPRINT(x, s) if(event->mask & x) std::cout << s << std::endl

			std::string correct_path = utils::get_sync_dir_path(socket->get_username()) + "/"; 
			correct_path += filename;

			//acontece quando arquivo é criado, renomeado, movido para dentro, ou editado
			if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)){
				//upload
				// MASKPRINT(IN_CREATE, "criar");
				// MASKPRINT(IN_CLOSE_WRITE, "editar");
				// MASKPRINT(IN_MOVED_TO, "mover");
				
				net::SendFileRequest req(correct_path.c_str());
				req.send(serde, socket);
				req.await_response(serde, socket);
			}
			if (event->mask & (IN_MOVED_FROM | IN_DELETE)){
				//delete
				// MASKPRINT(IN_MOVED_FROM, "tirado");
				// MASKPRINT(IN_DELETE, "deletado");

				net::Delete req(filename, username_global.c_str());
				req.send(serde, socket);
				req.await_response(serde, socket);
			}
		}
	}
}

std::pair<std::shared_ptr<net::Socket>, std::shared_ptr<net::Socket>> connect_client(
	const std::string& ip, const std::string& port, const std::string& coordinator_port,
	int id, const std::string& username, net::Serializer& serde){
	
	/* Conexão com o socket de comandos */
	net::ClientSocket base_socket_commands;

	try {
		base_socket_commands.connect(ip.c_str(), port.c_str());
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	auto socket_commands = base_socket_commands.build();
	
	std::string server_port_data;

	// Command handshake
	try {
		server_port_data = initial_handshake(serde, socket_commands, username.c_str(), id, coordinator_port);
	} catch (net::InvalidConnectionException& e){
		std::cout << "Failed to initialize connection: " <<  e.what() << std::endl;
		exit(1);
	}
	std::cout << "Command connection established! server_port_data: " << server_port_data << std::endl; 

	/* Conexão com o socket de receber dados da outra session */

	net::ClientSocket base_socket_data;

	try {
		base_socket_data.connect(ip.c_str(), server_port_data.c_str());
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	auto socket_data = base_socket_data.build();

	/* Data handshake */
	try {
		initial_handshake(serde, socket_data, username.c_str(), id, coordinator_port);
	} catch (net::InvalidConnectionException e){
		std::cout << "Failed to initialize connection: " <<  e.what() << std::endl;
		exit(1);
	}

	return std::make_pair(socket_data, socket_commands);

}

int client_loop_commands(std::shared_ptr<net::Socket> socket,
							const std::string& username,
							int id) {
	std::string userfolder = utils::get_sync_dir_path(username); 

	net::Serializer serde;

	/* Create the file descriptor for accessing the inotify API. */
	int inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd == -1) {
		perror("inotify_init1");
		exit(EXIT_FAILURE);
	}


	int watch_folder_fd = inotify_add_watch(inotify_fd, userfolder.c_str(), IN_MOVE | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE);
	if(watch_folder_fd == -1){
		std::cerr << "Cannot watch '" << userfolder << "' : " << strerror(errno) << std::endl;
		exit(EXIT_FAILURE);
	}	
	/* Prepare for polling. */
	static const nfds_t nfds = 2;

    struct pollfd fds[nfds];
	fds[0].fd = STDIN_FILENO;       /* Console input */
	fds[0].events = POLLIN;

	fds[1].fd = inotify_fd;         /* Inotify input */
	fds[1].events = POLLIN;

	/* Wait for events and/or terminal input. */
	while (1) {
		{	
			auto lock = g_close_cmd_thread.lock();
			if(lock.get()){
				lock.get() = false;
				break;
			}
		}
		if(g_close.lock().get()){
			std::cout << "Exiting program" << std::endl; 
			return 0; 
		}
		
		try {

			int poll_num = poll(fds, nfds, 1000);

			if (poll_num == -1) {
				if (errno == EINTR)
					continue;
				perror("poll");
				exit(EXIT_FAILURE);
			}
			if (poll_num > 0) {
				if (fds[0].revents & POLLIN) {

					std::string cmd, args;
					std::cin >> cmd;
					std::getline(std::cin, args);

					// std::cout << "cmd: " <<  cmd << std::endl << "args: " << args << std::endl; 
					execute_payload(serde, socket, cmd, args);
				}
				/* directory synchronization */
				if (fds[1].revents & POLLIN) {
					/* Inotify events are available. */
					// std::cout << "registered event" << std::endl;
					handle_events(inotify_fd, serde, socket);
				}
			}
		}
		catch (const net::CloseConnectionException& e) {
			std::cout << "Main loop closed conection" << std::endl;

			auto lock = g_close_cmd_thread.lock();
			lock.get() = false;
			break;
		}
	}
	std::cout << "closing iNotify" << std::endl; 
	close(inotify_fd);
	std::cout << "closed loop comand" << std::endl; 
	return -1;
}

void *client_loop_data(void *arg) {
	std::shared_ptr<net::Socket> socket = *((std::shared_ptr<net::Socket>*) arg);	
	net::Serializer serde;
	while(1) {
		try {
			if (g_close.lock().get()) {
				pthread_exit(0);
			}
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			std::cout << "Receiving data: " << utils::pckt_type_to_name(payload->get_type()) << std::endl; 
			payload->reply(serde, socket);
		}
		catch(const net::CloseConnectionException& e) {
			/* Conexão acabou -> Fecha thread e a thread de comandos reinicia a operação com novas conexões */
			std::cout << "Closed data thread" << std::endl;
			auto lock = g_close_cmd_thread.lock();
			lock.get() = true;
			pthread_exit(0);
		}catch (std::exception& e) {
			std::cerr << "uncaught exception: "  << e.what() << std::endl;
			pthread_exit(0);
		}
	}
}



int main(int argc, char** argv){

	/* Na chamada do ./client, os seguintes argumentos são necessários */
	/* username -> arbitrario */
	/* server_ip_address -> endereço de IP da máquina que contém o servidor coordenador */
	/* port_comandos -> porta que define a socket de comandos do server, dada como <cmd_port> (3º argumento) do servidor */
	/* port_data -> porta arbitrária */

	if(argc < 4){
		std::cerr << "argumentos insuficientes para começar o servidor" << '\n';
		std::cerr << "inicie no padrão: ./client <username> <server_ip_address> <port_comandos> <port_data>" << std::endl;
		exit(2);
	}

	int id = utils::random_number();
	std::string ip(argv[2]); 
	std::string port(argv[3]); 
	std::string coordinator_port(argv[4]); 
	std::string username(argv[1]);
	username_global = username; 
	std::string userfolder = utils::get_sync_dir_path(username);

	// Create an empty directory to receive data from server
	std::filesystem::remove_all(userfolder);
	std::filesystem::create_directory(userfolder);


	/* Criação de socket para receber a mensagem do novo coordenador */
	net::ServerSocket wait_for_coordinator_socket_listen; 
	// Abre a socket que aguardará mensagem do novo coordenador
	try{
		wait_for_coordinator_socket_listen.open(coordinator_port.c_str(), BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}

	net::Serializer serde;
	/* Faz o loop de operação, caso servidor falhe, reinicia conexão com réplica */
	while(1) {
		std::cout << "Connecting to server: " << ip << ":" << port << std::endl; 
		auto sockets = connect_client(ip, port, coordinator_port, id, username, serde);
		auto socket_data = sockets.first;
		auto socket_commands = sockets.second;
		/* Início da thread para leitura de dados */
		pthread_t t_data;
		pthread_create(&t_data, NULL, client_loop_data, &socket_data);
		/* Loop de espera de comandos */
		int ret =  client_loop_commands(socket_commands, username, id);
		pthread_join(t_data, NULL); 
		/* Conexão acabou -> Criar socket para aguardar conexão do novo coordenador -> Aguardar mensagem do novo coordenador -> Criar novas conexões */
		if (ret) {

			/* Espera a conexão com o novo coordenador */
			try {
				std::cout << "Waiting for new server..." << std::endl; 
				std::shared_ptr<net::Socket> wait_for_coordinator_socket(wait_for_coordinator_socket_listen.accept());
				std::cout << "New server connected" << std::endl; 
				/* Recebe a mensagem com o novo ip a se conectar e atualiza a informação de qual é o ip do server */
				std::string new_ip, new_port;
				auto buff = wait_for_coordinator_socket->read_full_pckt();
				auto payload = parse_payload(buff);			
				if (payload->get_type() != Net::Operation_IpInformation){
					std::string s = utils::pckt_type_to_name(payload->get_type()); 
					throw std::runtime_error("Unexpected packet " + s); 
				}
				payload->reply(serde, wait_for_coordinator_socket); 
				sleep(2); 
				ip = wait_for_coordinator_socket->get_their_ip();
				port =  static_cast<net::RedefineServer*>(payload.get())->port;
				std::cout << "New ip and port received: " << ip << ":" << port << std::endl; 
			}
			catch (const net::NetworkException& e){
				std::cerr << e.what() << '\n';
			}
		}
		
	}

	exit(EXIT_SUCCESS);
    return 0;
}

