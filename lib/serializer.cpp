
#include "serializer.hpp"
#include "exceptions.hpp"
#include "utils.hpp"
#include "defines.hpp"

#include <iostream>

namespace net{
	Serializer::Serializer(): builder(FB_BUFFSIZE){}

	const Net::Packet* Serializer::parse(uint8_t* pckt){
		return Net::GetSizePrefixedPacket(pckt);
	}

	const Net::Packet* Serializer::parse_expect(uint8_t* pckt, Net::Operation excepted_op){
		const Net::Packet* p = parse(pckt);
		if(p->op_type() != excepted_op){
			std::cout << "pacote recebido é: " << utils::pckt_type_to_name(p->op_type()) << std::endl;
			std::string expected_op_str(utils::pckt_type_to_name(excepted_op)); 
			throw ReceptionException("Pacote recebido não é " + expected_op_str);
		}
		return p;
	}
	
	FlatBufferBuilder* Serializer::build_ping(){
		builder.Clear();
		auto ping = Net::CreatePing(builder);
		auto packet = Net::CreatePacket(builder, Net::Operation_Ping, ping.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}

	FlatBufferBuilder* Serializer::build_listfiles(){
		builder.Clear();

		auto list_files = Net::CreateListFiles(builder);
		auto packet = Net::CreatePacket(builder, Net::Operation_ListFiles, list_files.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_exit(){
		//TODO:
		return &builder;
	}
	FlatBufferBuilder* Serializer::build_filemeta(std::string const& filename, std::string const& username, uint64_t size){
		builder.Clear();

		auto filemeta = Net::CreateFileMeta(builder, builder.CreateString(filename),builder.CreateString(username), size);
		auto packet = Net::CreatePacket(builder, Net::Operation_FileMeta, filemeta.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_sendfilerequest(std::string const& filename, uint64_t hash){
		builder.Clear();

		auto filemeta = Net::CreateSendFileRequest(builder, builder.CreateString(filename), hash);
		auto packet = Net::CreatePacket(builder, Net::Operation_SendFileRequest, filemeta.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_filedata(uint8_t* buff, int size){
		builder.Clear();

		auto filedata = Net::CreateFileData(builder, builder.CreateVector(buff, size));
		auto packet = Net::CreatePacket(builder, Net::Operation_FileData, filedata.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_connect(std::string const& username, Net::ChannelType type, uint64_t id){
		builder.Clear();

		auto connect = Net::CreateConnect(builder, id, type, builder.CreateString(username));
		auto packet = Net::CreatePacket(builder, Net::Operation_Connect, connect.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_download(std::string const& filename){
		builder.Clear();
		// dowload
		auto download = Net::CreateDownload(builder, builder.CreateString(filename));
		auto packet = Net::CreatePacket(builder, Net::Operation_Download, download.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_delete(std::string const& filename, std::string const& username){
		builder.Clear();

		auto del = Net::CreateDelete(builder, builder.CreateString(filename), builder.CreateString(username));
		auto packet = Net::CreatePacket(builder, Net::Operation_Delete, del.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}

		FlatBufferBuilder* Serializer::build_ip_information(std::string const& ip, std::string const& port){
		builder.Clear();

		auto ip_information = Net::CreateIpInformation(builder, builder.CreateString(ip), builder.CreateString(ip));
		auto packet = Net::CreatePacket(builder, Net::Operation_IpInformation, ip_information.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_response(Net::Status status, std::string const& msg, std::string *port){
		builder.Clear();
		flatbuffers::Offset<Net::Response> response;
		if (port == nullptr) response = Net::CreateResponse(builder, status, builder.CreateString(msg));
		else response = Net::CreateResponse(builder, status, builder.CreateString(msg), builder.CreateString(*port));
		auto packet = Net::CreatePacket(builder, Net::Operation_Response, response.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}

	FlatBufferBuilder* Serializer::build_election(const int valor){
		builder.Clear();

		auto election = Net::CreateElection(builder, valor);
		auto packet = Net::CreatePacket(builder, Net::Operation_Election, election.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;
	}
	FlatBufferBuilder* Serializer::build_coordinator(){
		builder.Clear();

		auto coordinator = Net::CreateCoordinator(builder);
		auto packet = Net::CreatePacket(builder, Net::Operation_Coordinator, coordinator.Union());
		builder.FinishSizePrefixed(packet);

		return &builder;

	}
	FlatBufferBuilder* build_relay_conection(std::string const& ip, std::string const& port);
}

/*
//throws `net::TransmissionException`, 
// `net::CloseConnectionException` and 
// `std::ios_base::failure`
/*
void Socket::send_file(std::string& filename, std::unique_ptr<std::ifstream> file){
	//open the file, get size 
	auto file = std::make_unique<std::ifstream>(filename, std::ios::binary | std::ios::ate);
	if(!file->is_open()){
		//TODO: except for this
		throw std::ios_base::failure("Falha em abrir o arquivo");
	}
	std::cout << "file size is " << size << std::endl;

	uint64_t size = file->tellg();

	//call send_filemeta (id, size, name)
	try{
		send_filemeta(size, filename);
	}catch(const std::exception& e){
		std::cerr << e.what() << '\n';
		//TODO: what do if fail? close connection?
	}
	//call send_filedata (id, size, ifstream);
	//if throws, pipe up
	file->seekg(0); 
	send_filedata(std::move(file));
}
void Socket::send_filedata(std::unique_ptr<std::ifstream> file){
	std::vector<uint8_t> buff(512, 0);
	while(!file->eof()){
		file->read((char*)buff.data(), buff.size());
		//NOTE: data_size -> uint64_t da merda?
		std::streamsize data_size = file->gcount();
		//got the chunk
		send_filedata_chunk(data_size, buff);
	}
}
//reads the whole packet, and returns the data associated with it
std::unique_ptr<BasePayload> Socket::read_operation(){
	builder.Clear();
	bzero(read_buff, PAYLOAD_HEADER_SIZE);	

	int read_bytes = recv(read_buff, PAYLOAD_HEADER_SIZE);
	if(read_bytes == 0){
		throw CloseConnectionException(); //closed connection
	}
	int tries = SOCKET_READ_ATTEMPTS;

	auto expected_msg_size = flatbuffers::GetSizePrefixedBufferLength(read_buff);
	
	//enquanto n ler todos os bytes esperados do pacote, append no buffers os bytes chegando
	while(read_bytes < expected_msg_size && tries > 0){
		read_bytes += recv(read_buff + read_bytes, expected_msg_size - read_bytes);
		tries--;
	}
	if(read_bytes != expected_msg_size){
		std::cout << "Lido: " << read_bytes << std::endl;
		std::cout << "Tries: " << tries << std::endl; 
		std::cout << "Esperado: " << expected_msg_size << std::endl; 
		throw ReceptionException("read_bytes != expected_msg_size"); //incomplete message
	}
	//NOTE: acima disso pode ser uma função
	// podendo criar um objeto novo para guardar o buffer

	auto msg = Net::GetSizePrefixedPacket(read_buff);

	switch (msg->op_type()){
	case Net::Operation_Connect: {
		auto payload = msg->op_as_Connect();
		return std::make_unique<ConnectPayload>(
			payload->username()->c_str()
		);
	} break;
	case Net::Operation_ListFiles: {
		return std::make_unique<ListFilesPayload>();
	} break;
	case Net::Operation_Download: {
		auto payload = msg->op_as_Download();
		return std::make_unique<DownloadPayload>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_Delete: {
		auto payload = msg->op_as_Delete();
		return std::make_unique<DeletePayload>(
			payload->filename()->c_str()
		);
	} break;
	case Net::Operation_Response: {
		auto payload = msg->op_as_Response();
		return std::make_unique<ResponsePayload>(
			payload->status(),
			payload->msg()->c_str()
		);
	} break;
	case Net::Operation_FileMeta: {
		auto payload = msg->op_as_FileMeta();
		return std::make_unique<FileMetaPayload>(
			payload->size(),
			payload->name()->c_str()
		);
	} break;
	case Net::Operation_FileData: {
		auto payload = msg->op_as_FileData();
		return std::make_unique<FileDataPayload>(
			payload->data()->data(),
			payload->data()->size()
		);
	}break;
	default:
		//didn't match any operation known
		throw ReceptionException("didn't match any operation known");
	}
}
void Socket::receive_filedata(std::unique_ptr<std::ofstream> file, uint64_t size){
	//Cria um arquivo temporário e escreve nele, para proteger o arquivo de uma perda de conexão
	//std::string filename_temp = filename + "_temp";
	//std::ofstream f(filename_temp, std::ios::binary);
	int read_bytes = 0;
	while (read_bytes < size){
		//TODO: check if it is a filedata payload
		auto uniq_packet = read_operation();
		auto packet = static_cast<FileDataPayload*>(uniq_packet.get()); 

		auto &buf =  packet->data;
		int buf_size = buf.size();
		read_bytes += buf_size;
		file->write((char*)buf.data(), buf_size);
		std::cout << "Lido: " << read_bytes << std::endl; 
	}
}*/