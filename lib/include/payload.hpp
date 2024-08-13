#ifndef PAYLOAD_HEADER
#define PAYLOAD_HEADER

#include <string>
#include <fstream>
#include <ios>
#include <memory>
#include <vector>

#include "packet_generated.h"
#include "sock.hpp"
#include "serializer.hpp"

namespace net{
	class SyncFile {
		public:
			//opens a file in the dir_name, which is the username of 'file_sync'
			void open_recv(const std::string& _filename, const std::string& dir_name);
			//opens a file, 
			ssize_t open_read(const std::string& _filename, const std::string& dir_name);
			ssize_t open_read(const std::string& _filename);
			bool open_read_if_exists(const std::string& _filename, const std::string& dir_name);

			ssize_t read(const std::vector<uint8_t>& buff);
			void write(const uint8_t* buff, const size_t size);
			inline bool eof(){ return file.eof(); }
			//rename cannot happen when a file is being read
			//the controler needs to acount for that
			void finish_and_rename();
			void finish();
		private:
			ssize_t _open_read();

			std::string tmp_filename;
			std::string filename;
			std::fstream file;

			static const size_t RANDOM_NAME_SIZE = 32; 
		};

	class Payload {
		public:
			Payload(Net::Operation op): operation_type(op){}
			inline Net::Operation get_type(){ return operation_type; }

			virtual void send(Serializer& serde, std::shared_ptr<Socket> socket) = 0;
			virtual void reply(Serializer& serde, std::shared_ptr<Socket> socket) = 0;
			//comportamento default de await_response é esperar um ok, se n da err 
			virtual void await_response(Serializer& serde, std::shared_ptr<Socket> socket);

			virtual Payload* clone() = 0;
		private:
			const Net::Operation operation_type;
	};
	/*
	class Response : public Payload {
		public:
			Response(Net::Status status, const char* msg): 
				status(status), msg(msg), Payload(Net::Operation_Response){}
			
			void send();
			void reply();
			void await_response();
		private:
			Net::Status status;
			std::string msg;	
	};
	*/

	//lida com IO
	class Upload : public Payload {
		public:
			//opens the file for the request
			Upload(const char* filename);
			Upload(const char* filename, uint64_t file_size);
			//size(size), filename(filename), Payload(Net::Operation_FileMeta)

			//read the file and sends the packets 
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//receives the packets and writes to file
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for ok or err pkct
			//void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Upload(filename.c_str(), size); }

			bool is_server = false; 	
		private:
			std::string filename;
			SyncFile file;
			uint64_t size;
	};

	class SendFileRequest : public Payload {
		public:
			//opens the file for the request
			SendFileRequest(const char* filename);
			SendFileRequest(const char* filename, uint64_t hash);

			//read the file and sends the packets 
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//receives the packets and writes to file
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for ok or err pkct
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new SendFileRequest(filename.c_str(), hash); }
		private:
			const std::string filename;
			SyncFile file;
			uint64_t hash;
	};
	//lida com IO
	class Download : public Payload {
		public:
			//dir_name is the username or 'sync_dir' 
			Download(const char* filename);

			//sends the name of the file to be downloaded
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//opens the file (if it has), and sends the meta + chunks of data
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for the file and saves it
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Download(filename.c_str()); }
		private:
			const std::string filename;
			SyncFile file;
			uint64_t size;
	};
	//NOTE: creio q esse pckt nunca vai ser recebido seco, puro, se sim its a dokie
	/*
	class FileDataPayload : public Payload {
		public:
			FileDataPayload(const uint8_t* data, int size): 
				data(data, data + size), Payload(Net::Operation_FileData){}
			std::vector<uint8_t> data;

	};*/

	//lida com IO
	class Connect : public Payload {
		public:
			//gatters the info
			Connect(const char* username, uint64_t id, const char* data_port = "");
			//username(username), Payload(Net::Operation_Connect){}

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//sends all the files associated with the username and then an response at the end
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for all the files
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Connect(username.c_str(), id, data_port.c_str()); }

			const uint64_t id; //value unique to a client
			const std::string username;
			
			std::string data_port;

			bool valid_connection = true;
			bool command_connection = false; 
			std::string port;
	};

	class Ping : public Payload {
		public:
			Ping();
			//: Payload(Net::Operation_ListFiles){}

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			// sends the reply and closes the connection
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for an ok
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Ping(); }
	};

	//lida com IO
	class ListFiles : public Payload {
		public:
			ListFiles();
			//: Payload(Net::Operation_ListFiles){}

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//reads the username folder and returns a response with the name of the files there
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for the response
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket);

			inline Payload* clone(){ return new ListFiles(); }
	};

	class Exit : public Payload {
		public:
			Exit();
			//: Payload(Net::Operation_ListFiles){}

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			// sends the reply and closes the connection
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for an ok
			//void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Exit(); }
	};
	//lida com IO
	class Delete : public Payload {
		public:
			Delete(const char* filename);

			//sends the name of the file to be deleted
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//does the operation, returns response depending on the result
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for the response
			//void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			inline Payload* clone(){ return new Delete(filename.c_str()); }
		private:
			const std::string filename;	
	};
	class RedefineServer : public Payload {
		public:
			RedefineServer(const std::string& port);

			// sends new ip and port of the new main server ( can be sent only be the coordinator for each client )
			void send(Serializer& serde, std::shared_ptr<Socket> socket);

			// TODO: decide what it replies
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);

			inline Payload* clone(){ return new RedefineServer(port); }
			const std::string port; 	

	};

	class Election : public Payload {
		public:
			Election(const int my_peso, const int other_peso = 0);

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//reads the username folder and returns a response with the name of the files there
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for the response
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket);

			inline Payload* clone(){ return new Election(my_peso, other_peso); }

			inline bool got_response(){ return response; }

		private:
			const int my_peso;
			const int other_peso;
			bool response = false;
	};

	class Coordinator : public Payload {
		public:
			Coordinator();

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//reads the username folder and returns a response with the name of the files there
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);

			inline Payload* clone(){ return new Coordinator(); }

	};

	class RelayConnection : public Payload {
		public:

			RelayConnection(const std::string& ip, const std::string& port);

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//reads the username folder and returns a response with the name of the files there
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);

			inline Payload* clone(){ return new RelayConnection(ip, port); }

			const std::string ip;
			const std::string port;
	};
}

#endif
