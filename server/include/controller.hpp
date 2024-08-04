#pragma once

#include <pthread.h>
#include "payload.hpp"
#include "user_server.hpp"
#include "mutex.hpp"
#include <map>
#include <set>

namespace net{
	class UserSession {
		public:
			void add_data_packet(std::shared_ptr<net::Payload> payload, int id);
			std::shared_ptr<net::Payload> get_data_packet(int id);
			//void unlock_packet();

			bool is_files_synched(int id);
			void set_files_synched(int id);

			bool add_session(int id); 
			bool remove_session(int id);

			int get_session_connections_num(); 
			bool has_session(int id); 

			UserSession(std::string username);

			const std::string username;
		private:
			Mutex<std::map<int, std::queue<std::shared_ptr<net::Payload>>>> data_packets_map;
			Mutex<std::map<int, bool>> synched_files_at_start;
			Mutex<std::set<int>> session_ids; 
	};

	class Controller {
		public:
			bool add_session(std::string username, int id); 
			bool remove_session(const std::string& username, int id);

			bool is_files_synched(const std::string& username, int id);
			void set_files_synched(const std::string& username, int id);

			Controller(Controller const&) = delete;
			void operator=(Controller const&) = delete;

			static Controller& getInstance(){
				static Controller instance;
				return instance;
			}
		private:
			UserSession& get_user_session(const std::string& username);
			Controller() : users_sessions(Mutex(std::map<std::string, UserSession>())) {}
			Mutex<std::map<std::string, UserSession>> users_sessions;

	};
}