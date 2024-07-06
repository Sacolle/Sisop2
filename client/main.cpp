#include <iostream>
#include <memory>
#include <string>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

#include "packet_generated.h"

#include "sock.hpp"
#include "payload.hpp"
#include "serializer.hpp"
#include "utils.hpp"

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

net::Payload* get_cli_payload(std::string &cmd, std::string& args){
	//TODO: parse
	utils::trim(cmd);
	utils::trim(args);

	try{
		if(cmd == "upload"){ return new net::Upload(args.c_str());
		}else if(cmd == "sendreq"){ return new net::SendFileRequest(args.c_str());
		}else if(cmd == "delete"){ return new net::Delete(args.c_str());
		}else if(cmd == "exit"){ return new net::Exit();
		}else if(cmd == "ping"){ return new net::Ping();
		}else if(cmd == "download"){
			return new net::Download(args.c_str());
		}else if(cmd == "list"){
			if(args == "server"){
				return new net::ListFiles();
			}else if(args == "client"){
				std::cout << "arquivos do cliente são: " << std::endl;
				return nullptr;
			}else{
				std::cout << "Args " << args << " não reconhecido.\n";
				std::cout << "Tente: client ou server." << std::endl;
				return nullptr;
			}
		}else{
			std::cout << "Comando " << cmd << " não reconhecido.\n";
			std::cout << "Tente: upload, download, delete, list ou exit." << std::endl;
			return nullptr;
		}
	}catch(const std::exception& e){
		std::cout << "Error: " << e.what() << std::endl;
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
			if(utils::is_tmp_file(filename)){
				continue;
			}

			const bool isdir = event->mask & IN_ISDIR;

			#define MASKPRINT(x, s) if(event->mask & x) std::cout << s << std::endl

			std::string correct_path("sync_dir/");
			correct_path += filename;

			//acontece quando arquivo é criado, renomeado, movido para dentro, ou editado
			if (event->mask & (IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO)){
				//upload
				MASKPRINT(IN_CREATE, "criar");
				MASKPRINT(IN_CLOSE_WRITE, "editar");
				MASKPRINT(IN_MOVED_TO, "mover");
				

				net::SendFileRequest req(correct_path.c_str());
				req.send(serde, socket);
				req.await_response(serde, socket);
			}
			if (event->mask & (IN_MOVED_FROM | IN_DELETE)){
				//delete
				MASKPRINT(IN_MOVED_FROM, "tirado");
				MASKPRINT(IN_DELETE, "deletado");

				net::Delete req(filename);
				req.send(serde, socket);
				req.await_response(serde, socket);
			}
		}
	}
}

int main(int argc, char** argv){
	if(argc < 4){
		std::cerr << "argumentos insuficientes para começar o servidor" << '\n';
		std::cerr << "inicie no padrão: ./client <username> <server_ip_address> <port>" << std::endl;
		exit(2);
	}

	net::ClientSocket base_socket;
	try {
		base_socket.connect(argv[2], argv[3]);
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		exit(1);
	}
	auto socket = base_socket.build();

	net::Serializer serde;

	try{
		initial_handshake(serde, socket, argv[1]);
	}catch(...){
		std::cout << "failed to initialize connection" << '\n';
		exit(1);
	}
	const char* userfolder = "sync_dir";
	/* Create the file descriptor for accessing the inotify API. */
	int inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd == -1) {
		perror("inotify_init1");
		exit(EXIT_FAILURE);
	}

	int watch_folder_fd = inotify_add_watch(inotify_fd, userfolder, IN_MOVE | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE);
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
		int poll_num = poll(fds, nfds, -1);

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

				std::cout << "cmd: " <<  cmd << std::endl << "args: " << args << std::endl; 
				execute_payload(serde, socket, cmd, args);
			}
			if (fds[1].revents & POLLIN) {
				/* Inotify events are available. */
				std::cout << "registered event" << std::endl;
				handle_events(inotify_fd, serde, socket);
			}
		}
	}

	/* Close inotify file descriptor. */
	close(inotify_fd);
	close(watch_folder_fd);

	exit(EXIT_SUCCESS);
    return 0;
}

