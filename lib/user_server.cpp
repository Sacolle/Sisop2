#include "user_server.hpp"

std::string UserServer::get_username() {
    pthread_mutex_lock(&(this->mutex_username));
    std::string username_ = this->username;
    pthread_mutex_unlock(&(this->mutex_username));
    return username_;
}

void UserServer::set_username(std::string s) {
    pthread_mutex_lock(&(this->mutex_username));
    this->username = s;
    pthread_mutex_unlock(&(this->mutex_username));
}

int UserServer::get_session_connections_num() {
    pthread_mutex_lock(&(this->mutex_session_connection_num));
    int session_connections_ = this->session_connections;
    pthread_mutex_unlock(&(this->mutex_session_connection_num));
    return session_connections_;
}

bool UserServer::is_logged(int id) {
    pthread_mutex_lock(&(this->mutex_session_connection_num));
    for (int i = 0; i < session_ids.size(); i++){
        if (session_ids[i] == id) {
            pthread_mutex_unlock(&(this->mutex_session_connection_num));
            return true;
        }
    }
    pthread_mutex_unlock(&(this->mutex_session_connection_num));
    return false;
}

void UserServer::add_session(int id) {
    pthread_mutex_lock(&(this->mutex_session_connection_num));
    for (int i = 0; i < session_ids.size(); i++){
        if (session_ids[i] == id) {
            pthread_mutex_unlock(&(this->mutex_session_connection_num));
            return;
        }
    }
    session_connections++;
    session_ids.push_back(id);
    pthread_mutex_lock(&(this->mutex_synched_files_at_start));
    synched_files_at_start[id] = false; 
    pthread_mutex_unlock(&(this->mutex_synched_files_at_start));
    pthread_mutex_unlock(&(this->mutex_session_connection_num));
}

void UserServer::remove_session(int id){
    pthread_mutex_lock(&(this->mutex_session_connection_num));
    if (session_ids.empty()){
        for (auto it = session_ids.begin(); it != session_ids.end(); it++){
            if (*it == id) session_ids.erase(it);
        }
    }   
    this->session_connections--;
    data_packets_map.erase(id);
    pthread_mutex_unlock(&(this->mutex_session_connection_num));
}

void UserServer::add_data_packet(std::shared_ptr<net::Payload> payload, int id) {
    pthread_mutex_lock(&(this->mutex_data_packets));
    for (auto it = data_packets_map.begin(); it !=  data_packets_map.end(); it++){
        it->second.push(payload);
    }
    pthread_mutex_unlock(&(this->mutex_data_packets));
}

std::shared_ptr<net::Payload> UserServer::get_data_packet(int id) {
    pthread_mutex_lock(&(this->mutex_data_packets));
    std::queue<std::shared_ptr<net::Payload>>& queue = data_packets_map[id];     
    std::shared_ptr<net::Payload> payload_;
    if (!queue.empty()) {
        payload_ = std::move(queue.front());
        queue.pop();
    }
    else {
        payload_ = nullptr;
    }
    return payload_;
}

bool UserServer::is_ready(int id){
    pthread_mutex_lock(&(this->mutex_synched_files_at_start));
    bool ret = synched_files_at_start[id];
    pthread_mutex_unlock(&(this->mutex_synched_files_at_start));
    return ret; 
}

void UserServer::set_ready(int id){
    pthread_mutex_lock(&(this->mutex_synched_files_at_start));
    synched_files_at_start[id] = true;
    pthread_mutex_unlock(&(this->mutex_synched_files_at_start));
}

void UserServer::unlock_packet(){
    pthread_mutex_unlock(&(this->mutex_data_packets));
}
