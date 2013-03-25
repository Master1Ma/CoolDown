#ifndef TRACKER_H
#define TRACKER_H

#include <vector>
#include <map>
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

using std::pair;
using std::vector;
using std::multimap;
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
    typedef vector< std::pair<int, ClientPtr> > ClientFileInfoCollection;
    typedef vector<string> ClientIdCollection;
	Tracker();
	~Tracker();

    Session& session(){
       return *pSession;
    };

    retcode_t AddOnlineUser(const string& clientId, ClientPtr peer);
    retcode_t RemoveOnlineUser(const string& clientId);
    retcode_t RequestClients(const string& fileId, int percentage, int needCount,
            const ClientIdCollection& ownedClientIdList, ClientFileInfoCollection* clients);

    retcode_t ReportProgress(const string& clientId, const string& fileId, int percentage);
    retcode_t PublishResource(const string& clientId, const string& fileId);

protected:
	void initialize(Application& self);
	void uninitialize();

	int main(const std::vector<std::string>& args);

	
private:
    typedef string ClientId;
    typedef string FileId;
    typedef int Percentage;
    typedef HashMap<ClientId, ClientPtr> client_map_t;
    typedef SharedPtr< map<Percentage, set<string> > >percentage_map_t;
    typedef HashMap<FileId, percentage_map_t > file_info_map_t;

    int init_db_tables();
    client_map_t clientMap_;
    //HashMap<FileId, multimap<Percentage, ClientPtr> > fileInfoMap_;
    file_info_map_t fileInfoMap_;


    FastMutex clientMapMutex_;
    FastMutex fileInfoMapMutex_;
    FastMutex dbMutex_;
    Poco::Data::Session* pSession;
};



#endif
