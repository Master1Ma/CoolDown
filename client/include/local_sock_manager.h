#ifndef LOCAL_SOCK_MANAGER_H
#define LOCAL_SOCK_MANAGER_H

#include "error_code.h"
#include <map>
#include <string>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/Logger.h>
#include <Poco/Mutex.h>

using std::map;
using std::string;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;
using Poco::Logger;
using Poco::FastMutex;

namespace CoolDown{
    namespace Client{

    class ClientInfo;

    class LocalSockManager{
        public:
            typedef SharedPtr<StreamSocket> SockPtr;
            typedef SharedPtr<LocalSockManager> LocalSockManagerPtr;
            LocalSockManager();
            ~LocalSockManager();

            retcode_t connect_tracker(const string& tracker_address, int port);
            retcode_t logout_tracker(const string& tracker_address, int port);
            retcode_t connect_client(const ClientInfo& info);
            retcode_t close_connection(const string& clientid);

            SockPtr get_sock(const string& clientid) const;
            SockPtr get_tracker_sock(const string& tracker_address) const;

        private:
            //map id -> sockptr
            //for tracker, id is the tracker_address
            void close_sock(SockPtr& sock);
            typedef map<string, SockPtr> sock_map_t;
            sock_map_t sock_map_;
            Logger& logger_;
            FastMutex mutex_;
        };
    }
}


#endif
