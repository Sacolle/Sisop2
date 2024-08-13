#include "payload.hpp"
#include "exceptions.hpp"

#include <iostream>
#include <cstdio>
#include <filesystem>
#include <sys/stat.h>


#include "utils.hpp"
#include "defines.hpp"
#include "komihash.h"

namespace net {
	void SyncFile::open_recv(const std::string& _filename, const std::string& dir_name){
		filename = dir_name + "/" + _filename;
		tmp_filename = dir_name + "/" +
			utils::generate_random_alphanumeric_string(RANDOM_NAME_SIZE) + 
			utils::get_file_extension(_filename) + 
			".tmp";

		const auto options = std::ios::out | std::ios::binary | std::ios::trunc;

		file.open(tmp_filename, options);
		if(!file.is_open()){
			throw std::ios_base::failure("Falha em abrir o arquivo");
		}
		file.seekg(0);
		// std::cout << "arquivo aberto corretamente" << std::endl;
	}

	ssize_t SyncFile::open_read(const std::string& _filename, const std::string& dir_name){
		filename = dir_name + "/" + _filename;
		return _open_read();
	}
	ssize_t SyncFile::open_read(const std::string& _filename){
		filename = _filename;
		return _open_read();
	}

	bool SyncFile::open_read_if_exists(const std::string& _filename, const std::string& dir_name){
		filename =  dir_name + "/" + _filename;

		const auto options = std::ios::in | std::ios::binary | std::ios::ate;

		file.open(filename, options);
		if(!file.is_open()){
			return false;
		}
		file.seekg(0);
		return true;
	}

	ssize_t SyncFile::_open_read(){
		const auto options = std::ios::in | std::ios::binary | std::ios::ate;

		file.open(filename, options);
		if(!file.is_open()){
			throw std::ios_base::failure("Falha em abrir o arquivo");
		}
		const auto file_size = file.tellg();
		file.seekg(0);
		return file_size;
	}

	ssize_t SyncFile::read(const std::vector<uint8_t>& buff){
		file.read((char*) buff.data(), buff.size());
		return file.gcount();
	}
	void SyncFile::write(const uint8_t* buff, const size_t size){
		file.write((char*) buff, size);
	}
	void SyncFile::finish_and_rename(){
		//TODO: if it has a lock, release it
		file.close();
		if(std::rename(tmp_filename.c_str(), filename.c_str())){
			throw std::ios_base::failure("failed to rename file");
		}
	}
	void SyncFile::finish(){
		//TODO: if it has a lock, release it
		file.close();
	}

	void Payload::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto response = pckt->op_as_Response();
		if(response->status() != Net::Status_Ok){
			//TODO: precisa de uma exceção para quando os pacotes são enviados corretamente
			//mas ocorre um erro mesmo assim
			throw TransmissionException();
		}else{
			// std::cout << "Resposta: " << response->msg()->c_str() << std::endl;  
		}
	}

	//se falha, envia um response err
	Upload::Upload(const char* filename): 
		filename(filename), size(0), Payload(Net::Operation_FileMeta){}
	Upload::Upload(const char* filename, uint64_t file_size):
		filename(filename), size(file_size), Payload(Net::Operation_FileMeta){}

	//read the file and sends the packets
	//`throws` in `socket->send_checked`
	void Upload::send(Serializer& serde, std::shared_ptr<Socket> socket){
		//open the file efectivelly
		if (is_server){  
			size = file.open_read(filename, utils::get_sync_dir_path(socket->get_username()));
		} else {
			size = file.open_read(filename);
		}

		auto filemeta_pckt = serde.build_filemeta(utils::filename_without_path(filename), size);
		socket->send_checked(filemeta_pckt);

		std::vector<uint8_t> buff(READFILE_BUFFSIZE, 0);
		while(!file.eof()){
			auto data_size = file.read(buff);
			//got the chunk
			auto chunk_pckt = serde.build_filedata(buff.data(), data_size);
			socket->send_checked(chunk_pckt);
		}
		file.finish();
	}
	//receives the packets and writes to file
	void Upload::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		
		file.open_recv(utils::filename_without_path(filename), utils::get_sync_dir_path(socket->get_username()));

		//already received the filemeta and builded this upload obj, having the corret size
		//receive the following pckts
		uint64_t read_bytes = 0;
		while (read_bytes < size){
			auto buff = socket->read_full_pckt();
			auto pckt = serde.parse_expect(buff, Net::Operation_FileData);

			auto filedata = pckt->op_as_FileData();
			auto data = filedata->data();
			const uint64_t data_size = data->size();

			read_bytes += data_size;
			file.write(data->data(), data_size);
			//std::cout << "Lido: " << read_bytes << std::endl; 
		}
		//read the file, send an ok
		file.finish_and_rename();

		std::string msg("Arquivo recebido corretamente");
		auto response = serde.build_response(Net::Status_Ok, msg);
		socket->send_checked(response); //if fails, bubble up
	}

	SendFileRequest::SendFileRequest(const char* filename): 
		filename(filename), hash(0), Payload(Net::Operation_SendFileRequest){}
	SendFileRequest::SendFileRequest(const char* filename, uint64_t hash): 
		filename(filename), hash(hash), Payload(Net::Operation_SendFileRequest){}

	//read the file and sends the packets 
	void SendFileRequest::send(Serializer& serde, std::shared_ptr<Socket> socket){
	 	ssize_t size = file.open_read(filename);

		std::vector<uint8_t> buff(READFILE_BUFFSIZE, 0);
		komihash_stream_t ctx;
		komihash_stream_init(&ctx, HASHSEED);
		while(!file.eof()){
			auto data_size = file.read(buff);
			komihash_stream_update(&ctx, buff.data(), data_size);
		}
		file.finish();

		hash = komihash_stream_final(&ctx);
		//std::cout << "computed hash: " << hash << std::endl;

		auto sendfilerequest_pckt = serde.build_sendfilerequest(utils::filename_without_path(filename), hash);
		socket->send_checked(sendfilerequest_pckt);
	}
	//receives the packets and writes to file
	void SendFileRequest::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		bool exists = file.open_read_if_exists(filename, utils::get_sync_dir_path(socket->get_username()));
		if(!exists){
			std::string msg("O servidor não possui esse arquivo");
			auto response = serde.build_response(Net::Status_Ok, msg);
			socket->send_checked(response);
			return;
		}
		
		std::vector<uint8_t> buff(READFILE_BUFFSIZE, 0);
		komihash_stream_t ctx;
		komihash_stream_init(&ctx, HASHSEED);
		while(!file.eof()){
			auto data_size = file.read(buff);
			komihash_stream_update(&ctx, buff.data(), data_size);
		}
		file.finish();

		uint64_t recv_file_hash = komihash_stream_final(&ctx);
		//std::cout << "received hash: " << hash << std::endl;
		//std::cout << "computed hash: " << recv_file_hash << std::endl;

		if(hash == recv_file_hash){
			std::string msg("O servidor já possui esse mesmo arquivo nesse mesmo estado.");
			auto response = serde.build_response(Net::Status_SameFile, msg);
			socket->send_checked(response);
		}else{
			std::string msg("O servidor não possui essa versão atualizada do arquivo");
			auto response = serde.build_response(Net::Status_Ok, msg);
			socket->send_checked(response);
		}
	}
	//awaits for ok or err pkct
	void SendFileRequest::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto response = pckt->op_as_Response();
		switch (response->status()){
		case Net::Status_Ok:{
			// std::cout << "Resposta: " << response->msg()->c_str() << std::endl;  
			net::Upload upload_file(filename.c_str());
			upload_file.send(serde, socket);
			upload_file.await_response(serde, socket);
			return;
		} break;
		case Net::Status_SameFile:
			// std::cout << "Resposta: " << response->msg()->c_str() << std::endl;  
			return;
			break;
		default:
			throw TransmissionException();
			break;
		}
	}

	//opens the respective file 
	//can fail, so need to send a response if it does so
	Download::Download(const char* filename): 
		filename(filename), Payload(Net::Operation_Download){}
	//sends the name of the file to be downloaded
	void Download::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_download(filename);
		socket->send_checked(pckt);
	}

	//opens the file (if it has), and sends the meta + chunks of data
	void Download::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		size = file.open_read(utils::filename_without_path(filename), utils::get_sync_dir_path(socket->get_username()));
		auto filemeta_pckt = serde.build_filemeta(filename, size);
		socket->send_checked(filemeta_pckt);

		std::vector<uint8_t> buff(READFILE_BUFFSIZE, 0);
		while(!file.eof()){
			auto data_size = file.read(buff);
			//got the chunk
			auto chunk_pckt = serde.build_filedata(buff.data(), data_size);
			socket->send_checked(chunk_pckt);
		}
		file.finish();
	}

	//awaits for the file and saves it
	void Download::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		//TODO: do also a timeout sistem
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_FileMeta);
		auto filemeta = pckt->op_as_FileMeta();

		file.open_recv(filemeta->name()->str(), ".");

		const auto file_size = filemeta->size();
		uint64_t read_bytes = 0;

		while (read_bytes < file_size){
			auto buff = socket->read_full_pckt();
			auto pckt = serde.parse_expect(buff, Net::Operation_FileData);
			auto filedata = pckt->op_as_FileData();
			auto data = filedata->data();
			const uint64_t data_size = data->size();

			read_bytes += data_size;
			file.write(data->data(), data_size);
			//std::cout << "Lido: " << read_bytes << std::endl; 
		}

		file.finish_and_rename();
	}
	//awaits for ok or err pkct
	//gatters the info
	Connect::Connect(const char* username, const Net::ChannelType channel_type, uint64_t id): 
		username(username), 
		channel_type(channel_type),
		id(id), 
		Payload(Net::Operation_Connect){}

	//builds the pckt and sends
	void Connect::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_connect(username, channel_type, id);
		socket->send_checked(pckt);

		socket->set_connection_info(username, id, channel_type);
	}

	//sends all the files associated with the username and then an response at the end
	void Connect::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		socket->set_connection_info(username, id, channel_type);
		//TODO: send all files associated with the username
		std::string msg;
		if (this->valid_connection){
			msg += "Conectado corretamente ao user ";
			msg += username;
			msg += " com id único ";
			msg += std::to_string(id);
		} else {
			msg += "Connection failed!";
		}
		std::string port(PORT_DATA); 
		FlatBufferBuilder*  pckt;
		if (this->command_connection) pckt = serde.build_response(this->valid_connection ? Net::Status_Ok : Net::Status_Error, msg, &port);
		else pckt = serde.build_response(this->valid_connection ? Net::Status_Ok : Net::Status_Error, msg);
		socket->send_checked(pckt);
	}

	//awaits for all the files
	void Connect::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto response = pckt->op_as_Response();
		if(response->status() != Net::Status_Ok){
			std::cerr << response->msg()->c_str() << std::endl; 
			throw InvalidConnectionException(response->msg()->c_str());
		}else {
			std::cout << "Conexão estabelecida corretamente:\n\tUsername: " 
				<< username << "\n\t" 
				<< "Id: " << id << "\n\t"
				<< "Resposta do servidor: " << response->msg()->c_str() << std::endl;  
		}
		if (response->port()) {
			port = std::string(strdup(response->port()->c_str()));
		}
	}

	ListFiles::ListFiles(): Payload(Net::Operation_ListFiles){}

	//builds the pckt and sends
	void ListFiles::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_listfiles();
		socket->send_checked(pckt);
	}

	//reads the username folder and returns a response with the name of the files there
	void ListFiles::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		std::string msg = "List of server files: \n";  
		try{
			std::string path = utils::get_sync_dir_path(socket->get_username());
			for (const auto &entry : std::filesystem::directory_iterator(path)) {
				struct stat sb;
				if (lstat(entry.path().string().c_str(), &sb) == -1) {
               		perror("lstat");
					auto pckt = serde.build_response(Net::Status_Error, "Falha em ler os arquivos da pasta");
					socket->send_checked(pckt);
					return;
           		}
				msg += "name: " + entry.path().filename().string(); 
				msg += "\nctime: " + std::string(ctime(&sb.st_ctime));
           		msg += "atime: " +  std::string(ctime(&sb.st_atime));
				msg += "mtime: " +  std::string(ctime(&sb.st_mtime));
				msg += "\n"; 
			}
		}
		catch(const std::ios_base::failure& e){
			std::cerr << e.what() << '\n';
			auto pckt = serde.build_response(Net::Status_Error, "Falha em ler os arquivos da pasta");
			socket->send_checked(pckt);
			return;
		}
		
		auto pckt = serde.build_response(Net::Status_Ok, msg);
		socket->send_checked(pckt);
	}

	void ListFiles::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = socket->read_full_pckt();
		auto ping = serde.parse_expect(pckt, Net::Operation_Response)->op_as_Response();
		std::cout << ping->msg()->c_str() << std::endl;
	}


	Exit::Exit(): Payload(Net::Operation_ListFiles){}

	//builds the pckt and sends
	void Exit::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_exit();
		socket->send_checked(pckt);
	}

	// sends the reply and closes the connection
	void Exit::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		std::string msg("Pode fechar");
		auto pckt = serde.build_response(Net::Status_Ok, msg);
		socket->send_checked(pckt);
		//TODO: close stuff
	}


	Delete::Delete(const char* filename): filename(filename), Payload(Net::Operation_Delete){}

	//sends the name of the file to be deleted
	void Delete::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_delete(filename);
		socket->send_checked(pckt);
	}

	//does the operation, returns response depending on the result
	void Delete::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		try{
			//TODO: close stuff
			std::string file_to_remove(utils::get_sync_dir_path(socket->get_username()));
			file_to_remove += "/";
			file_to_remove += filename;
			remove(file_to_remove.c_str());

			std::string msg(filename);
			msg += " deletado.";
			auto pckt = serde.build_response(Net::Status_Ok, msg);
			socket->send_checked(pckt); //se falha em enviar aqui, não catch
		}
		catch(const std::ios_base::failure& e){ //exception caso falhe em abrir arquivo
			auto pckt = serde.build_response(Net::Status_Error, e.what());
			socket->send_checked(pckt);
		}
	}

	// --- ping ---
	Ping::Ping(): Payload(Net::Operation_Ping){}

	//builds the pckt and sends
	void Ping::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_ping();
		socket->send_checked(pckt);
	}

	// sends the reply and closes the connection
	void Ping::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_ping();
		socket->send_checked(pckt);
	}
	//awaits for an ok
	void Ping::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = socket->read_full_pckt();
		auto ping = serde.parse_expect(pckt, Net::Operation_Ping);
		std::cout << "got ping back" << std::endl;
	}

	RedefineServer::RedefineServer(const char* ip, const char* port): ip(ip), port(port), Payload(Net::Operation_RedefineServer){}

	//sends the name of the file to be deleted
	void RedefineServer::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_redefine_server(ip, port);
		socket->send_checked(pckt);
	}

	//só manda um ok
	void RedefineServer::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_response(Net::Status::Status_Ok, std::string(""));
		socket->send_checked(pckt);
	}

	Election::Election(const int my_peso, const int other_peso):
		my_peso(my_peso), other_peso(other_peso), Payload(Net::Operation_Election){}

	//builds the pckt and sends
	void Election::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_election(my_peso);
		socket->send_checked(pckt);
	}
	//reads the username folder and returns a response with the name of the files there
	void Election::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_response(
			other_peso > my_peso ? Net::Status_Yes : Net::Status_No,
			std::string(""));
		socket->send_checked(pckt);
	}
	//awaits for the response
	void Election::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto res = pckt->op_as_Response();
		if(res->status() == Net::Status_Yes){
			//se recebeu um sim, n precisa fazer nada, pois por default assume-se q 
			//a eleição começa sem respostas
		} else if(res->status() == Net::Status_No){
			//se recebeu que alguem é maior q o seu valor, seta que houve uma resposta
			response = true;
		}else {
			//TODO: precisa de uma exceção para quando os pacotes são enviados corretamente
			//mas ocorre um erro mesmo assim
			throw TransmissionException();
		}
	}

	Coordinator::Coordinator():Payload(Net::Operation_Coordinator){}

	//builds the pckt and sends
	void Coordinator::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_coordinator();
		socket->send_checked(pckt);
	}
	//reads the username folder and returns a response with the name of the files there
	void Coordinator::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_response(Net::Status::Status_Ok, std::string(""));
		socket->send_checked(pckt);
	}

}

