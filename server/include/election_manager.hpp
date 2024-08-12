#pragma once

#include "sock.hpp"
#include "mutex.hpp"

#include <list>
#include <memory>

namespace net{

	class ElectionManager {
		public:
			static ElectionManager& getInstance(int valor = 0){
				static ElectionManager instance(valor);
				return instance;
			}

			void add_recv_socket(std::shared_ptr<Socket> socket);
			void add_send_socket(std::shared_ptr<Socket> socket);

			void stage_send_socket_to_remove(std::shared_ptr<Socket> socket);
			void remove_staged_send_socket();

			void remove_recv_socket(std::shared_ptr<Socket> socket);

			bool in_election(){
				auto lock = _in_election.lock();
				return lock.get();
			}
			void set_in_election(bool e){
				auto lock = _in_election.lock();
				*lock.raw() = e;
			}

		private:
			ElectionManager(int v): valor(v), _in_election(Mutex(false)){}

			const int valor;

			Mutex<bool> _in_election;
			std::shared_ptr<Socket> coordinator_socket;

			std::list<std::shared_ptr<Socket>> recv_sockets;
			std::list<std::shared_ptr<Socket>> send_sockets;

			std::optional<std::shared_ptr<Socket>> send_socket_to_remove = std::nullopt;
	};
}

