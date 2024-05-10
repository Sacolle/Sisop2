#include "payload.hpp"

#include <iostream>

#include "utils.hpp"

namespace net {
	void Payload::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto response = pckt->op_as_Response();
		if(response->status() != Net::Status_Ok){
			//TODO: precisa de uma exceção para quando os pacotes são enviados corretamente
			//mas ocorre um erro mesmo assim
			throw TransmissionException();
		}else{
			std::cout << "Resposta: " << response->msg()->c_str() << std::endl;  
		}
	}

	//se falha, envia um response err
	Upload::Upload(const char* filename, const uint64_t file_size): 
		filename(filename),
		Payload(Net::Operation_FileMeta){

		std::cout << "Abrindo o arquivo: " << filename << std::endl;

		const auto options = file_size == 0 ? 
			//if there is no file_size, it means its opening an exiting file to be read 
			std::ios::in | std::ios::binary | std::ios::ate : 
			//if there is a file_size, it's been built from a receiving packet
			std::ios::out | std::ios::binary | std::ios::trunc;

		file.open(filename, options);
		if(!file.is_open()){
			throw std::ios_base::failure("Falha em abrir o arquivo");
		}
		//sets the internal file_size
		if(file_size == 0){
			size = file.tellg();
		}else{
			size = file_size;
		}
		file.seekg(0);
	}

	//read the file and sends the packets
	//`throws` in `socket->send_checked`
	void Upload::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto filemeta_pckt = serde.build_filemeta(utils::filename_without_path(filename), size);
		socket->send_checked(filemeta_pckt);

		std::vector<uint8_t> buff(512, 0);
		while(!file.eof()){
			file.read((char*)buff.data(), buff.size());
			//NOTE: data_size -> uint64_t da merda?
			std::streamsize data_size = file.gcount();
			//got the chunk
			auto chunk_pckt = serde.build_filedata(buff.data(), data_size);
			socket->send_checked(chunk_pckt);
		}

	}
	//receives the packets and writes to file
	void Upload::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		//already received the filemeta and builded this upload obj, having the corret size
		//receive the following pckts
		//TODO: do also a timeout sistem
		uint64_t read_bytes = 0;
		while (read_bytes < size){
			auto buff = socket->read_full_pckt();
			auto pckt = serde.parse_expect(buff, Net::Operation_FileData);

			auto filedata = pckt->op_as_FileData();
			auto data = filedata->data();
			const uint64_t data_size = data->size();

			read_bytes += data_size;
			file.write((char*)data->data(), data_size);
			std::cout << "Lido: " << read_bytes << std::endl; 
		}
		//TODO: rename the file and stuff
		//read the file, send an ok
		std::string msg("Arquivo recebido corretamente");
		auto response = serde.build_response(Net::Status_Ok, msg);
		socket->send_checked(response); //if fails, bubble up
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
	}

	//sends all the files associated with the username and then an response at the end
	void Connect::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		//TODO: send all files associated with the username
		std::string msg("Server recebeu conectado: ");
		msg += username;
		auto pckt = serde.build_response(Net::Status_Ok, msg);
		socket->send_checked(pckt);
	}

	//awaits for all the files
	void Connect::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_Response);
		auto response = pckt->op_as_Response();
		if(response->status() != Net::Status_Ok){
			//TODO: precisa de uma exceção para quando os pacotes são enviados corretamente
			//mas ocorre um erro mesmo assim
			throw TransmissionException();
		}else{
			std::cout << "Conexão estabelecida corretamente:\n\tUsername: " 
				<< username << "\n\t" 
				<< "Id: " << id << "\n\t"
				<< "Resposta do servidor: " << response->msg()->c_str() << std::endl;  
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
		std::string msg("Nomes dos arquivos do cliente:");
		//TODO: ler os arquivos
		try{
			/* code */
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

	//opens the respective file 
	//can fail, so need to send a response if it does so
	Download::Download(const char* filename, const bool clean_file): 
		filename(filename), 
		Payload(Net::Operation_Download){

		const auto options = clean_file ? 
			//if clean_file == true, its the file to put the packets in
			std::ios::out | std::ios::binary | std::ios::trunc :
			//if clean == false, its the file to pull the chunks from 
			std::ios::in | std::ios::binary | std::ios::ate;

		file.open(filename, options);
		if(!file.is_open()){
			throw std::ios_base::failure("Falha em abrir o arquivo");
		}
		//sets the internal file_size
		size = file.tellg();
		file.seekg(0);
	}

	//sends the name of the file to be downloaded
	void Download::send(Serializer& serde, std::shared_ptr<Socket> socket){
		auto pckt = serde.build_download(filename);
		socket->send_checked(pckt);
	}

	//opens the file (if it has), and sends the meta + chunks of data
	void Download::reply(Serializer& serde, std::shared_ptr<Socket> socket){
		auto filemeta_pckt = serde.build_filemeta(filename, size);
		socket->send_checked(filemeta_pckt);

		file.seekg(0);
		std::vector<uint8_t> buff(512, 0);
		while(!file.eof()){
			file.read((char*)buff.data(), buff.size());
			//NOTE: data_size -> uint64_t da merda?
			std::streamsize data_size = file.gcount();
			//got the chunk
			auto chunk_pckt = serde.build_filedata(buff.data(), data_size);
			socket->send_checked(chunk_pckt);
		}
	}

	//awaits for the file and saves it
	void Download::await_response(Serializer& serde, std::shared_ptr<Socket> socket){
		//TODO: do also a timeout sistem
		auto buff = socket->read_full_pckt();
		auto pckt = serde.parse_expect(buff, Net::Operation_FileMeta);
		auto filemeta = pckt->op_as_FileMeta();

		const auto file_size = filemeta->size();
		uint64_t read_bytes = 0;

		while (read_bytes < file_size){
			auto buff = socket->read_full_pckt();
			auto pckt = serde.parse_expect(buff, Net::Operation_FileData);
			auto filedata = pckt->op_as_FileData();
			auto data = filedata->data();
			const uint64_t data_size = data->size();

			read_bytes += data_size;
			file.write((char*)data->data(), data_size);
			std::cout << "Lido: " << read_bytes << std::endl; 
		}

		//TODO: rename
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

}

