#ifndef SERIALIZER_HEADER
#define SERIALIZER_HEADER

#include "packet_generated.h"

#include <utility>
#include <memory>

using flatbuffers::FlatBufferBuilder;

namespace net{

	class Serializer {
		public:
			Serializer();
			//retorna um pointer para dentro de pckt
			const Net::Packet* parse(uint8_t* pckt);
			const Net::Packet* parse_expect(uint8_t* pckt, Net::Operation excepted_op);

			FlatBufferBuilder* build_listfiles();
			FlatBufferBuilder* build_ping();
			FlatBufferBuilder* build_exit();
			FlatBufferBuilder* build_filemeta(std::string const& filename, uint64_t size);
			FlatBufferBuilder* build_sendfilerequest(std::string const& filename, uint64_t hash);
			FlatBufferBuilder* build_filedata(uint8_t* buff, int size);
			FlatBufferBuilder* build_connect(std::string const& username, Net::ChannelType type, uint64_t id);
			FlatBufferBuilder* build_download(std::string const& filename);
			FlatBufferBuilder* build_delete(std::string const& filename);
			FlatBufferBuilder* build_redefine_server(std::string const& ip, std::string const& port);
			FlatBufferBuilder* build_response(Net::Status status, std::string const& msg, std::string *port = nullptr);
			FlatBufferBuilder* build_election(const int valor);
			FlatBufferBuilder* build_coordinator();
			FlatBufferBuilder* build_relay_conection(std::string const& ip, std::string const& port);

		private:
			FlatBufferBuilder builder;
	};
}


#endif