#include "controller.hpp"
#include <exceptions.hpp>

namespace net{

	UserSession::UserSession(std::string username): 
		username(username), 
		synched_files_at_start(Mutex(std::map<int, bool>())),
		session_ids(Mutex(std::set<int>())),
		data_packets_map(Mutex(std::map<int, std::queue<std::shared_ptr<net::Payload>>>())){}

	void UserSession::set_files_synched(int id){
		auto files_synched = synched_files_at_start.lock(); 
		if (files_synched->count(id) == 0) {
			throw std::runtime_error("Session " +  std::to_string(id) +  " not logged for user " +  username); 
		}
		files_synched.get()[id] = true; 
	}

	bool UserSession::is_files_synched(int id){
		auto files_synched = synched_files_at_start.lock(); 
		if (files_synched->count(id) == 0) {
			throw std::runtime_error("Session " +  std::to_string(id) +  " not logged for user " +  username); 
		}
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
		data_packets_map.lock().get()[id] = std::queue<std::shared_ptr<net::Payload>>(); 
		synched_files_at_start.lock().get()[id] = false; 
		return true;
	}

	bool Controller::add_session(std::string username, int id){
		auto user_sessions = users_sessions.lock(); 
		if (user_sessions->count(username) == 0){
			user_sessions->try_emplace(username, username);
		}
		UserSession& user_session =  user_sessions->at(username); 
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
		auto& user_session =  user_sessions->at(username); 
		bool ret = user_session.remove_session(id); 
		if (user_session.get_session_connections_num() == 0){
			user_sessions->erase(username); 
		}
		return ret; 
	}

	UserSession& Controller::get_user_session(const std::string& username){
		auto user_sessions = users_sessions.lock();
		if (user_sessions->count(username) == 0){
			throw std::runtime_error("User " + username + " does not have any session logged"); 
		}
		return user_sessions->at(username); 
	}


	bool Controller::is_files_synched(const std::string& username, int id){
		return get_user_session(username).is_files_synched(id);
	}

	void Controller::set_files_synched(const std::string& username, int id){
		get_user_session(username).set_files_synched(id); 
	}
}