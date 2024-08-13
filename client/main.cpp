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

#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"
#include "exceptions.hpp"
#include "mutex.hpp"

#define BACKLOG 10
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

pthread_mutex_t mutex_close_threads = PTHREAD_MUTEX_INITIALIZER;

std::pair<std::shared_ptr<net::Socket>, std::shared_ptr<net::Socket>> connect_client(
	const std::string& ip, const std::string& port, int id, const std::string& username, net::Serializer& serde);

net::ServerSocket wait_for_coordinator_socket_listen(true);
int client_loop_commands(	std::shared_ptr<net::Socket> socket,
							const std::string& username,
							int id);
void *client_loop_data(void *arg);

net::Mutex<bool> g_close_cmd_thread = net::Mutex(false);
net::Mutex<bool> g_close = net::Mutex(false);

net::Payload* get_cli_payload(std::string &cmd, std::string& args){
	//TODO: parse
	utils::trim(cmd);
	utils::trim(args);

	try{
		if(cmd == "upload"){ return new net::Upload(args.c_str());
		}else if(cmd == "sendreq"){ return new net::SendFileRequest(args.c_str());
		}else if(cmd == "delete"){ return new net::Delete(args.c_str());
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
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		//std::cout << "filemeta" << std::endl;
		auto upload = std::make_shared<net::Upload>(
			payload->name()->c_str(),
			payload->size()
		);
		upload->is_server = true;
		return upload;
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

std::string initial_handshake(net::Serializer& serde, std::shared_ptr<net::Socket> socket, const char* username, int id){
	//TODO: read the connect info from args[]
	net::Connect connect(username, id);
	connect.send(serde, socket);
	connect.await_response(serde, socket);
	return connect.port; 
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
			if (event->mask & (IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO)){
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

				net::Delete req(filename);
				req.send(serde, socket);
				req.await_response(serde, socket);
			}
		}
	}
}

std::pair<std::shared_ptr<net::Socket>, std::shared_ptr<net::Socket>> connect_client(
	const std::string& ip, const std::string& port, int id, const std::string& username, net::Serializer& serde){
	
	/* Conexão com o socket de comandos */

	std::cout << "começou connect client" << std::endl;
	net::ClientSocket base_socket_commands;

	try {
		base_socket_commands.connect(ip.c_str(), port.c_str());
	}
	catch(const std::exception& e){
		std::cerr << "failed to connect command socket:\n" <<  e.what() << '\n';
		exit(1);
	}

	auto socket_commands = base_socket_commands.build();
	
	std::string server_port_data;

	// Command handshake
	try {
		server_port_data = initial_handshake(serde, socket_commands, username.c_str(), id);
	} catch (net::InvalidConnectionException& e){
		std::cout << "Failed to initialize connection: " <<  e.what() << std::endl;
		exit(1);
	}

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
		initial_handshake(serde, socket_data, username.c_str(), id);
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
		{
			if(g_close.lock().get()){
				exit(0);
			}
		}

		try {
			//lista de descitores, numero deles, timeout em MILISEGUNDOS
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
			std::cout << "main loop closed conection" << std::endl;

			auto lock = g_close_cmd_thread.lock();
			lock.get() = false;
			break;
		}
	}

	/* Close inotify file descriptor. */
	close(inotify_fd);
	close(watch_folder_fd);
	
	std::cin.clear();
	std::cout.flush();

	return -1;
}

void *client_loop_data(void *arg) {
	std::shared_ptr<net::Socket> socket = *((std::shared_ptr<net::Socket>*) arg);
	net::Serializer serde;
	while(1) {
		try {
			if (g_close.lock().get()) {
				exit(EXIT_SUCCESS);
			}
			auto buff = socket->read_full_pckt();
			auto payload = parse_payload(buff);
			std::cout << "Receiving data: " << utils::pckt_type_to_name(payload->get_type()) << std::endl; 
			payload->reply(serde, socket);
		}
		catch(const net::CloseConnectionException& e) {
			/* Conexão acabou -> Fecha thread e a thread de comandos reinicia a operação com novas conexões */
			std::cout << "closed data thread" << std::endl;
			auto lock = g_close_cmd_thread.lock();
			lock.get() = true;
			pthread_exit(0);
			return nullptr;
		}catch (std::exception& e) {
			std::cerr << "uncaught exception: "  << e.what() << std::endl;
		}
	}
}

int main(int argc, char** argv){
	if(argc < 4){
		std::cerr << "argumentos insuficientes para começar o servidor" << '\n';
		std::cerr << "inicie no padrão: ./client <username> <server_ip_address> <port>" << std::endl;
		exit(2);
	}

	int id = utils::random_number();
	std::string ip(argv[2]); 
	std::string port(argv[3]); 
	std::string username(argv[1]);
	std::string userfolder = utils::get_sync_dir_path(username);

	// Create an empty directory to receive data from server
	std::filesystem::remove_all(userfolder);
	std::filesystem::create_directory(userfolder);
	net::Serializer serde;

	net::ServerSocket wait_for_coordinator_socket_listen; 
	// Abre a socket que aguardará mensagem do novo coordenador
	try{
		wait_for_coordinator_socket_listen.open(PORT_CHANGE_COORDINATOR, BACKLOG);
	}
	catch(const net::NetworkException& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	/* Faz o loop de operação, caso servidor falhe, reinicia conexão com réplica */
	while(1) {
		auto sockets = connect_client(ip, port, id, username, serde);
		auto socket_data = sockets.first;
		auto socket_commands = sockets.second;
		/* Início da thread para leitura de dados */
		pthread_t t_data;
		pthread_create(&t_data, NULL, client_loop_data, &socket_data);
		/* Loop de espera de comandos */
		int commands_error = client_loop_commands(socket_commands, username, id);
		pthread_join(t_data, nullptr);

		//se chegou neste ponto quer dizer q tanto o client_loop data e commands param a execução,
		//o que só ocorre num close connection

		try {
			std::cout << "accept" << std::endl;
			std::shared_ptr<net::Socket> wait_for_coordinator_socket(wait_for_coordinator_socket_listen.accept());
			/* Recebe a mensagem com o novo ip a se conectar e atualiza a informação de qual é o ip do server */
			std::string new_port;
			std::cout << "buff" << std::endl;
			auto buff = wait_for_coordinator_socket->read_full_pckt();
			//só deve ser um redefine server
			std::cout << "payload" << std::endl;
			auto payload = serde.parse_expect(buff, Net::Operation_RedefineServer);			

			auto redefine_server_paylod = payload->op_as_RedefineServer();
			
			net::RedefineServer redefine_server(redefine_server_paylod->port()->str());

			std::cout << "reply" << std::endl;
			redefine_server.reply(serde, wait_for_coordinator_socket); 

			//pega o ip de quem mandou via socket e a porta do pacote mandou
			ip = wait_for_coordinator_socket->get_their_ip();
			port = redefine_server.port;

			std::cout << "going to connect to: " << ip << ":" << port << std::endl;
			
		}
		catch (const net::NetworkException& e){
			std::cerr << e.what() << '\n';
		}
	}

	exit(EXIT_SUCCESS);
    return 0;
}

