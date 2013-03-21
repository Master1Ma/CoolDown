#ifndef TRACKER_H
#define TRACKER_H

#include <vector>
#include "Poco/Logger.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Exception.h"
#include "Poco/Thread.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/SQLite/Connector.h"
#include <Poco/HashMap.h>
#include <Poco/Mutex.h>
#include <Poco/SharedPtr.h>

#include "netpack.h"
#include "client_info.h"
#include "tracker_connection_handler.h"

using std::vector;
using std::pair;
using Poco::Logger;
using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Thread;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Data::Session;
using Poco::HashMap;
using Poco::FastMutex;
using Poco::SharedPtr;

using std::exception;

namespace Poco{
    template<>
    class ReleasePolicy<ClientInfo>
    {
        public:
            static void release(ClientInfo* pObj){
                delete pObj;
            }
    };
}

class Tracker: public Poco::Util::ServerApplication
{
public:
    typedef SharedPtr<ClientInfo> ClientPtr;
    typedef vector< std::pair<ClientPtr, uint16_t> > ClientFileInfoCollection;
    typedef vector<string> ClientIdCollection;
	Tracker();
	~Tracker();

    Session& session(){
       return *pSession;
    };

    /*
    retcode_t update_user_info(const string& nodeId, SharedPtr<NodeInfo> pInfo);
    retcode_t find_user_info();
    */

    retcode_t AddOnlineUser(const string& clientId, ClientPtr peer);
    retcode_t RemoveOnlineUser(const string& clientId);
    retcode_t RequestClients(const string& fileId, uint16_t percentage, 
            const ClientIdCollection& ownedClientIdList, ClientIdCollection& clients);

    retcode_t ReportPercentage(const string& clientId, uint16_t percentage);
    retcode_t PublishResource(const string& clientId, const string& fileId);

protected:
	void initialize(Application& self);
	void uninitialize();

	int main(const std::vector<std::string>& args);

	
private:
    typedef string ClientId;

    int init_db_tables();
    HashMap<ClientId, ClientPtr> clientMap_;

    FastMutex clientMapMutex_;
    Poco::Data::Session* pSession;
};



#endif
