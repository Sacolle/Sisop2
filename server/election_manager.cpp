#include "election_manager.hpp"

namespace net{
	void ElectionManager::add_recv_socket(std::shared_ptr<net::Socket> socket){
		recv_sockets.push_back(socket);
	}
	void ElectionManager::add_send_socket(std::shared_ptr<net::Socket> socket){
		send_sockets.push_back(socket);
	}

	void ElectionManager::stage_send_socket_to_remove(std::shared_ptr<Socket> socket){
		send_socket_to_remove = std::make_optional(socket);
	}

	void ElectionManager::remove_staged_send_socket(){
		if(send_socket_to_remove.has_value()){
			send_sockets.remove(send_socket_to_remove.value());
		}
		send_socket_to_remove = std::nullopt;
	}

	void ElectionManager::remove_recv_socket(std::shared_ptr<Socket> socket){
		recv_sockets.remove(socket);
	}
}