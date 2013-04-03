#include "local_sock_manager.h"
#include <Poco/Util/Application.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>
#include <boost/phoenix.hpp>

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
            SockPtr tracker_sock = this->make_connection(tracker_address, port);
            if( tracker_sock.isNull() ){
                poco_warning_f2(logger_, "Cannot connect to tracker, address : %s, port : %d.", tracker_address, port);
                return ERROR_NET_CONNECT;
            }
            FastMutex::ScopedLock lock(server_sock_map_mutex_);
            server_sock_map_[tracker_address] = tracker_sock;
            return ERROR_OK;
        }

        retcode_t LocalSockManager::logout_tracker(const string& tracker_address, int port){
            SockPtr sock = this->get_tracker_sock(tracker_address);
            if( sock.isNull() ){
                return ERROR_NET_NOT_CONNECTED;
            }
            FastMutex::ScopedLock lock(server_sock_map_mutex_);
            server_sock_map_.erase(tracker_address);
            return ERROR_OK;
        }

        retcode_t LocalSockManager::connect_client(const string& clientid, const string& ip, int port){
            FastMutex::ScopedLock lock(client_sock_map_mutex_);

            client_sock_map_t::iterator iter = client_sock_map_.find(clientid);
            if( iter != client_sock_map_.end() && iter->second.size() > MAX_CONNECTION_PER_CLIENT ){
                return ERROR_NET_CONNECTION_LIMIT;
            }
            SockPtr sock = this->make_connection(ip, port);
            if( sock.isNull() ){
                return ERROR_NET_CONNECT;
            }
            client_sock_map_[clientid].push_back(std::make_pair(sock, IDLE) );

            return ERROR_OK;
        }
        /*
        retcode_t LocalSockManager::close_connection_to_tracker(const string& tracker_address){
            server_sock_map_t::iterator iter = server_sock_map_.find( tracker_address );
            return ERROR_UNKNOWN;
        }
        */

        bool LocalSockManager::reach_connection_limit(const string& clientid){
            FastMutex::ScopedLock lock(client_sock_map_mutex_);
            client_sock_map_t::iterator iter = client_sock_map_.find(clientid);
            if( iter == client_sock_map_.end() || iter->second.size() < MAX_CONNECTION_PER_CLIENT ){
                return false;
            }
            return true;
        }

        SockPtr LocalSockManager::make_connection(const string& ip, int port){
            SockPtr sock;
            try{
                SockPtr tmp(new StreamSocket);
                tmp->connect( SocketAddress(ip, port) );
                sock = tmp;
            }catch(Exception& e){
            }
            return sock;
        }

        SockPtr LocalSockManager::get_idle_client_sock(const string& clientid){

            FastMutex::ScopedLock lock(client_sock_map_mutex_);

            client_sock_map_t::iterator listIter = client_sock_map_.find(clientid);
            if( listIter == client_sock_map_.end() ){
                poco_warning_f1(logger_, "get idle sock of unconnected client, client id :%s", clientid);
                return SockPtr();
            }
            SockList& sockList = listIter->second;

            SockList::iterator sockIter = find_if(sockList.begin(), sockList.end(), 
                    IdleSockSelector());

            if( sockList.end() == sockIter){
                return SockPtr();
            }else{
                return sockIter->first;
            }

        }

        SockPtr LocalSockManager::get_tracker_sock(const string& tracker_address){
            server_sock_map_t::const_iterator iter = server_sock_map_.find(tracker_address);
            if( server_sock_map_.end() == iter ){
                poco_warning_f1(logger_, "get sock of not connected traker, tracker addr : %s.", tracker_address);
                return SockPtr();
            }else{
                return iter->second;
            }
        }

    }
}
