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
		private:
			const std::string filename;
			SyncFile file;
			uint64_t size;
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
			Connect(const char* username, const Net::ChannelType channel_type, uint64_t id);
			//username(username), Payload(Net::Operation_Connect){}

			//builds the pckt and sends
			void send(Serializer& serde, std::shared_ptr<Socket> socket);
			//sends all the files associated with the username and then an response at the end
			void reply(Serializer& serde, std::shared_ptr<Socket> socket);
			//awaits for all the files
			void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;

			const uint64_t id; //value unique to a client
			const std::string username;
			const Net::ChannelType channel_type;
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
			//void await_response(Serializer& serde, std::shared_ptr<Socket> socket) override;
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
		private:
			const std::string filename;	
	};
}

#endif
