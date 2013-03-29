#include "local_sock_manager.h"
#include <Poco/Util/Application.h>

using Poco::Util::Application;

namespace CoolDown{
    namespace Client{
        typedef LocalSockManager::SockPtr SockPtr;
        LocalSockManager::LocalSockManager()
        :logger_(Application::instance().logger()){

        }

        LocalSockManager::~LocalSockManager(){
        }


        retcode_t LocalSockManager::login_tracker(const string& tracker_address, int port){
            return ERROR_OK;
        }

        retcode_t LocalSockManager::connect_client(const ClientInfo& info){
            return ERROR_OK;
        }
        retcode_t LocalSockManager::close_connection(const string& clientid){
            return ERROR_OK;
        }

        SockPtr LocalSockManager::get_sockget_sock(const string& clientid) const{
            return SockPtr();
        }
        SockPtr LocalSockManager::get_tracker_sock(const string& tracker_address) const{
            return SockPtr();
        }
    }
}
