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

 void handle_events(int fd, int *wd, int argc, char* argv[])
       {
           /* Some systems cannot read integer variables if they are not
              properly aligned. On other systems, incorrect alignment may
              decrease performance. Hence, the buffer used for reading from
              the inotify file descriptor should have the same alignment as
              struct inotify_event. */

           char buf[4096]
               __attribute__ ((aligned(__alignof__(struct inotify_event))));
           const struct inotify_event *i;
           ssize_t len;

           /* Loop while events can be read from inotify file descriptor. */

           for (;;) {

               /* Read some events. */

               len = read(fd, buf, sizeof(buf));
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
                       ptr += sizeof(struct inotify_event) + i->len) {

                   i = (const struct inotify_event *) ptr;

                   /* Print event type. */

                  
                    printf("mask = ");
    if (i->mask & IN_CREATE)        printf("IN_CREATE ");
    if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
    if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
    if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
    printf("\n");

                   /* Print the name of the watched directory. */

                   for (size_t s = 1; s < argc; ++s) {
                       if (wd[s] == i->wd) {
                           printf("%s/", argv[s]);
                           break;
                       }
                   }

                   /* Print the name of the file. */

                   if (i->len)
                       printf("%s", i->name);

                   /* Print type of filesystem object. */

                   if (i->mask & IN_ISDIR)
                       printf(" [directory]\n");
                   else
                       printf(" [file]\n");
               }
           }
       }

int main(int argc, char** argv){
	char buf;
    int fd, i, poll_num;
	int* wd;
    
    nfds_t nfds;
    struct pollfd fds[2];
	/*if(argc < 4){
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
	basic_op(socket.build(), argv[1]);*/

  if (argc < 2) {
               printf("Usage: %s PATH [PATH ...]\n", argv[0]);
               exit(EXIT_FAILURE);
           }

           printf("Press ENTER key to terminate.\n");

           /* Create the file descriptor for accessing the inotify API. */

           fd = inotify_init1(IN_NONBLOCK);
           if (fd == -1) {
               perror("inotify_init1");
               exit(EXIT_FAILURE);
           }

           /* Allocate memory for watch descriptors. */

           wd = (int *)calloc(argc, sizeof(int));
           if (wd == NULL) {
               perror("calloc");
               exit(EXIT_FAILURE);
           }

           /* Mark directories for events
              - file was opened
              - file was closed */

           for (i = 1; i < argc; i++) {
               wd[i] = inotify_add_watch(fd, argv[i],
                                         IN_MOVE | IN_MODIFY | IN_CREATE);
               if (wd[i] == -1) {
                   fprintf(stderr, "Cannot watch '%s': %s\n",
                           argv[i], strerror(errno));
                   exit(EXIT_FAILURE);
               }
           }

           /* Prepare for polling. */

           nfds = 2;

           fds[0].fd = STDIN_FILENO;       /* Console input */
           fds[0].events = POLLIN;

           fds[1].fd = fd;                 /* Inotify input */
           fds[1].events = POLLIN;

           /* Wait for events and/or terminal input. */

           printf("Listening for events.\n");
           while (1) {
               poll_num = poll(fds, nfds, -1);
               if (poll_num == -1) {
                   if (errno == EINTR)
                       continue;
                   perror("poll");
                   exit(EXIT_FAILURE);
               }

               if (poll_num > 0) {

                   if (fds[0].revents & POLLIN) {

                       /* Console input is available. Empty stdin and quit. */

                       while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                           continue;
                       break;
                   }

                   if (fds[1].revents & POLLIN) {

                       /* Inotify events are available. */

                       handle_events(fd, wd, argc, argv);
                   }
               }
           }

           printf("Listening for events stopped.\n");

           /* Close inotify file descriptor. */

           close(fd);

           free(wd);
           exit(EXIT_SUCCESS);
    return 0;
}

