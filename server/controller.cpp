#include "controller.hpp"
#include <exceptions.hpp>

namespace net{

	UserSession::UserSession(std::string username): 
		username(username), 
		synched_files_at_start(Mutex(new std::map<int, bool>())),
		session_ids(Mutex(new std::set<int>())){}

	void UserSession::set_files_synched(int id){
		auto files_synched = synched_files_at_start.lock(); 
		files_synched.get()[id] = true; 
	}

	bool UserSession::is_files_synched(int id){
		auto files_synched = synched_files_at_start.lock(); 
		std::cout << "is_file"  << files_synched.get()[id] << std::endl; // TODO debug remove
		return files_synched.get()[id];
	}

	int UserSession::get_session_connections_num(){
		auto ids = session_ids.lock();
		return ids->size();
	}
	bool UserSession::has_session(int id){
		auto ids = session_ids.lock();
		return ids->count(id) == 1;
	}

	bool UserSession::remove_session(int id){
		{
			auto ids = session_ids.lock();
			if (ids->count(id) == 0) {
				return false; 
			}
			ids->erase(id);
		}
		{
			auto ids = synched_files_at_start.lock();
			ids->erase(id);
		}
		{
			auto ids = data_packets_map.lock();
			ids->erase(id);
		}
	}

	bool UserSession::add_session(int id){
		auto ids = session_ids.lock();
		// Trying to add session already logged
		if (ids->count(id)){
			return false; 
		}
		// limit of sessions breached, one user must have at maximum 2 connections
		if (ids->size() > 1) {
			return false; 
		}

		ids->insert(id); 
		data_packets_map.get()[id] = std::queue<std::queue<std::shared_ptr<net::Payload>>>(); 
		synched_files_at_start.get()[id] = false; 
		return true;
	}

	bool Controller::add_session(std::string username, int id){
		auto user_sessions = users_sessions.lock(); 
		if (user_sessions->count(username) == 0){
			user_sessions->try_emplace(username, username);
		}
		auto& user_session =  user_sessions.get()[username]; 
		// Already logged, return true
		if (user_session.has_session(id)) {
			return true;
		}
		return user_session.add_session(id); 
	}
	
	bool Controller::remove_session(const std::string& username, int id){
		auto user_sessions = users_sessions.lock();
		if (user_sessions->count(username) == 0){
			return false; 
		}
		auto& user_session =  user_sessions.get()[username]; 
		bool ret = user_session.remove_session(id); 
		if (user_session.get_session_connections_num() == 0){
			user_sessions->erase(username); 
		}
		return ret; 
	}

	
}