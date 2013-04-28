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
            SockPtr tracker_sock = this->make_connection(tracker_address, port);
            if( tracker_sock.isNull() ){
                poco_warning_f2(logger_, "Cannot connect to tracker, address : %s, port : %d.", tracker_address, port);
                return ERROR_NET_CONNECT;
            }
            FastMutex::ScopedLock lock(server_sock_map_mutex_);
            poco_debug_f1(logger_, "connect to tracker succeed, key : %s", tracker_sock->peerAddress().toString());
            server_sock_map_[tracker_sock->peerAddress().toString()] = tracker_sock;
            return ERROR_OK;
        }

        retcode_t LocalSockManager::logout_tracker(const string& tracker_address, int port){
            SockPtr sock = this->get_tracker_sock(tracker_address);
            if( sock.isNull() ){
                return ERROR_NET_NOT_CONNECTED;
            }
            FastMutex::ScopedLock lock(server_sock_map_mutex_);
            server_sock_map_.erase(sock->peerAddress().toString());
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

            poco_debug_f3(logger_, "connect to client succeed, id : %s, ip : %s, port : %d",
                    clientid, ip, port);
            client_sock_map_[clientid].push_back(std::make_pair(sock, IDLE) );

            FastMutex::ScopedLock cond_lock(condition_map_mutex_);
            if( condition_map_.find( clientid ) == condition_map_.end() ){
                condition_map_[clientid] = new Condition;
            }

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
            return this->make_connection(SocketAddress(ip, port));
        }

        SockPtr LocalSockManager::make_connection(const SocketAddress& sa){
            SockPtr sock;
            try{
                SockPtr tmp(new StreamSocket);
                tmp->connect( sa );
                sock = tmp;
            }catch(Exception& e){
            }
            return sock;
        }

        SockPtr LocalSockManager::get_idle_client_sock(const string& clientid){

            FastMutex::ScopedLock lock(client_sock_map_mutex_);


            client_sock_map_t::iterator listIter = client_sock_map_.find(clientid);
            if( listIter == client_sock_map_.end() ){
            //if( this->is_connected( clientid ) == false ){
                poco_warning_f1(logger_, "get idle sock of unconnected client, client id : '%s'", clientid);
                return SockPtr();
            }
            SockList& sockList = client_sock_map_[clientid];

            SockList::iterator sockIter = find_if(sockList.begin(), sockList.end(), 
                    IdleSockSelector());

            if( sockList.end() == sockIter){
                if( sockList.size() > MAX_CONNECTION_PER_CLIENT ){
                    poco_notice_f1(logger_, "connection to %s exceed limit!", clientid);
                    return SockPtr();
                }else if( sockList.size() == 0 ){
                    poco_notice(logger_, "the sock list of %s has no sock at all!");
                    return SockPtr();
                }else{
                    SockPtr prototype = sockList.begin()->first;
                    poco_notice_f1(logger_, "int get_idle_client_sock, Try make_connection to %s.", prototype->peerAddress().host().toString());
                    poco_notice(logger_, "make_connection may block...");
                    SockPtr sock = make_connection(prototype->peerAddress());
                    client_sock_map_[clientid].push_back( std::make_pair(sock, USED) );
                    return sock;
                }
            
            }else{
                sockIter->second = USED;
                return sockIter->first;
            }

        }

        void LocalSockManager::return_sock(const string& clientid, SockPtr sock){
            FastMutex::ScopedLock lock(client_sock_map_mutex_);
            client_sock_map_t::iterator listIter = client_sock_map_.find(clientid);
            if( listIter == client_sock_map_.end() ){
                poco_notice_f1(logger_, "in LocalSockManager::return_sock, cannot find clientid : %s", clientid);
                return;
            }
            SockList& sockList = listIter->second;
            poco_debug_f2(logger_, "sockList of client '%s' length : %d", clientid, (int)sockList.size() );
            SockList::iterator iter = find_if(sockList.begin(), sockList.end(), FindSock(sock) );
            if( iter == sockList.end() ){
                poco_notice_f2(logger_, "return sock that doesnot belong to client, clientid:%s, peer addr:%s",
                        clientid, sock->peerAddress().toString());
                return;
            }
            iter->second = IDLE;
            FastMutex::ScopedLock cond_lock(condition_map_mutex_);
            poco_assert( condition_map_.find(clientid) != condition_map_.end() );
            poco_debug_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);
            condition_map_[clientid]->signal();
        }

        LocalSockManager::ConditionPtr LocalSockManager::get_sock_idel_condition(const string& clientid){
            FastMutex::ScopedLock cond_lock(condition_map_mutex_);
            poco_assert( condition_map_.find(clientid) != condition_map_.end() );
            poco_debug_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);
            return condition_map_[clientid];
        }

        double LocalSockManager::get_payload_percentage(const string& clientid){
            FastMutex::ScopedLock lock(client_sock_map_mutex_);
            client_sock_map_t::iterator listIter = client_sock_map_.find(clientid);

            if( listIter == client_sock_map_.end() ){
                return (double)100;
            }else{
                return (double)listIter->second.size() / MAX_CONNECTION_PER_CLIENT;
            }
        }

        bool LocalSockManager::is_connected(const string& clientid){
            FastMutex::ScopedLock lock(client_sock_map_mutex_);
            return client_sock_map_.end() != client_sock_map_.find(clientid);
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
