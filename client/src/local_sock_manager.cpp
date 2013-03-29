#include "local_sock_manager.h"
#include <Poco/Util/Application.h>

using Poco::Util::Application;

LocalSockManager::LocalSockManager()
:logger_(Application::instance().logger()){

}

LocalSockManager::~LocalSockManager(){
}


retcode_t LocalSockManager::login_tracker(const string& tracker_address, int port){
}

retcode_t LocalSockManager::connect_client(const ClientInfo& info){
}
retcode_t LocalSockManager::close_connection(const string& clientid){
}

SockPtr LocalSockManager::get_sockget_sock(const string& clientid) const{
}
SockPtr LocalSockManager::get_tracker_sock(const string& tracker_address) const{
}
