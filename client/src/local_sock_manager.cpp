#include "local_sock_manager.h"
#include <Poco/Util/Application.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>

using Poco::Util::Application;
using Poco::Net::SocketAddress;
using Poco::Exception;

namespace CoolDown{
    namespace Client{
        typedef LocalSockManager::SockPtr SockPtr;
        LocalSockManager::LocalSockManager()
        :logger_(Application::instance().logger()){

        }

        LocalSockManager::~LocalSockManager(){
        }


        retcode_t LocalSockManager::connect_tracker(const string& tracker_address, int port){
            SockPtr tracker_sock(new StreamSocket);
            try{
                tracker_sock->connect( SocketAddress(tracker_address, port) );
            }catch(Exception& e){
                poco_warning_f2(logger_, "Cannot connect to tracker, address : %s, port : %d.", tracker_address, port);
                return ERROR_NET_CONNECT;
            }
            sock_map_[tracker_address] = tracker_sock;
            return ERROR_OK;
        }

        retcode_t LocalSockManager::logout_tracker(const string& tracker_address, int port){
            SockPtr sock = this->get_tracker_sock(tracker_address);
            if( sock.isNull() ){
                return ERROR_NET_CONNECT;
            }
            this->close_sock( sock );
            return ERROR_OK;
        }

        retcode_t LocalSockManager::connect_client(const ClientInfo& info){
            return ERROR_OK;
        }
        retcode_t LocalSockManager::close_connection(const string& clientid){
            return ERROR_OK;
        }

        SockPtr LocalSockManager::get_sock(const string& clientid) const{
            sock_map_t::const_iterator iter = sock_map_.find(clientid);
            if( sock_map_.end() == iter ){
                poco_warning_f1(logger_, "get sock of not connected client, clientid : %s.", clientid);
                return SockPtr();
            }else{
                return iter->second;
            }
        }
        SockPtr LocalSockManager::get_tracker_sock(const string& tracker_address) const{
            return this->get_sock(tracker_address);
        }

        void LocalSockManager::close_sock(SockPtr& sock){
            sock->close();
        }

    }
}
