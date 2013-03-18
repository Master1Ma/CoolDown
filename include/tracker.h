#ifndef TRACKER_H
#define TRACKER_H

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

#include "netpack.h"
#include "tracker_connection_handler.h"

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

using std::exception;


class Tracker: public Poco::Util::ServerApplication
{
public:
	Tracker();
	~Tracker();

    Session& session(){
       return *pSession;
    };

    /*
    retcode_t update_user_info(const string& nodeId, SharedPtr<NodeInfo> pInfo);
    retcode_t find_user_info();
    */

protected:
	void initialize(Application& self);
	void uninitialize();

	int main(const std::vector<std::string>& args);

	
private:
    int init_db_tables();
    Poco::Data::Session* pSession;
};



#endif
