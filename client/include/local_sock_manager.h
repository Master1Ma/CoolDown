#ifndef LOCAL_SOCK_MANAGER_H
#define LOCAL_SOCK_MANAGER_H

#include "error_code.h"
#include <map>
#include <list>
#include <string>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>

using std::pair;
using std::map;
using std::list;
using std::string;
using Poco::Net::StreamSocket;
using Poco::Net::SocketAddress;
using Poco::SharedPtr;
using Poco::Logger;
using Poco::FastMutex;
using Poco::Condition;


namespace Poco{
            template <>
            class ReleasePolicy<StreamSocket>
            {
                public:
                     static void release(StreamSocket* pObj)
                     {
                         if( pObj ){
                             pObj->close();
                             delete pObj;
                         }
                     }
            };
}

namespace CoolDown{
    namespace Client{

    class ClientInfo;
    class IdleSockSelector;

    class LocalSockManager{
        public:

            enum SockStatus{
                IDLE = 1,
                USED = 2,
            };


            //typedef SharedPtr<StreamSocket, Poco::ReferenceCounter, SockReleasePolicy> SockPtr;
            typedef SharedPtr<StreamSocket> SockPtr;
            typedef SharedPtr<LocalSockManager> LocalSockManagerPtr;
            typedef SharedPtr<Condition> ConditionPtr;
            typedef list<pair<SockPtr, SockStatus> > SockList;
            typedef map<string, SockList> client_sock_map_t;
            typedef map<string, SockPtr> server_sock_map_t;


            LocalSockManager();
            ~LocalSockManager();

            retcode_t connect_tracker(const string& tracker_address, int port = 9977);
            retcode_t logout_tracker(const string& tracker_address, int port = 9977);
            retcode_t connect_client(const string& clientid, const string& ip, int port);
            retcode_t close_connection_to_tracker(const string& tracker_address);
            bool reach_connection_limit(const string& clientid);
            bool is_connected(const string& clientid);

            
            //SockPtr get_sock(const string& clientid);
            SockPtr get_tracker_sock(const string& tracker_address);
            SockPtr get_idle_client_sock(const string& clientid);
            void return_sock(const string& clientid, SockPtr sock);
            ConditionPtr get_sock_idel_condition(const string& clientid);
            

            double get_payload_percentage(const string& clientid);
            

            struct IdleSockSelector{
                bool operator()(const SockList::value_type& v){
                    return v.second == IDLE;
                }
            };

            struct FindSock{
                FindSock(const SockPtr& sock)
                    :sock_(sock){
                    }

                bool operator()(const SockList::value_type& v){
                    return v.first == sock_;
                }
                private:
                    SockPtr sock_;
            };

        private:
            SockPtr make_connection(const SocketAddress& sa);
            SockPtr make_connection(const string& ip, int port);
            
            const static size_t MAX_CONNECTION_PER_CLIENT = 10;
            server_sock_map_t server_sock_map_;
            client_sock_map_t client_sock_map_;
            Logger& logger_;
            FastMutex server_sock_map_mutex_;
            FastMutex client_sock_map_mutex_;
            FastMutex condition_map_mutex_;

            map<string, ConditionPtr> condition_map_;
        };

    }
}


#endif
