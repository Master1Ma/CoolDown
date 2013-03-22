#include "tracker.h"
#include "netpack.h"
#include "tracker_connection_handler.h"

#include <iostream>
#include <exception>
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

Tracker::Tracker()
{
}
	
Tracker::~Tracker()
{
}

void Tracker::initialize(Application& self)
{
    loadConfiguration(); // load default configuration files, if present
    ServerApplication::initialize(self);
    logger().setLevel("debug");
}
		
void Tracker::uninitialize()
{
    ServerApplication::uninitialize();
}

int Tracker::main(const std::vector<std::string>& args)
{
    try{
        if( init_db_tables() ){
            return Application::EXIT_CANTCREAT;
        }

        unsigned short port = (unsigned short) config().getInt("TrackerServer.port", 9977);

        ServerSocket svs(port);
        SocketReactor reactor;
        SocketAcceptor<TrackerConnectionHandler> acceptor(svs, reactor);

        Thread thread;
        thread.start(reactor);

        waitForTerminationRequest();

        reactor.stop();
        thread.join();

    }catch(exception& e){
        std::cout << e.what() << std::endl;
    }
    return Application::EXIT_OK;
}

retcode_t Tracker::AddOnlineUser(const string& clientId, ClientPtr peer){
    if( clientMap_.end() != clientMap_.find(clientId) ){
        return ERROR_CLIENT_ALREADY_EXISTS;
    }

    FastMutex::ScopedLock lock( clientMapMutex_ );
    clientMap_[clientId] = peer;
    return ERROR_OK;
}

retcode_t Tracker::RemoveOnlineUser(const string& clientId){
    if( clientMap_.end() == clientMap_.find(clientId) ){
        return ERROR_CLIENT_NOT_FOUND;
    }
    FastMutex::ScopedLock lock( clientMapMutex_ );
    clientMap_.erase(clientId);
    return ERROR_OK;
}

retcode_t Tracker::RequestClients(const string& fileId, uint16_t percentage, 
        const ClientIdCollection& ownedClientIdList, ClientIdCollection* clients){
    return ERROR_OK;
}

retcode_t Tracker::ReportPercentage(const string& clientId, uint16_t percentage){
    return ERROR_OK;
}
retcode_t Tracker::PublishResource(const string& clientId, const string& fileId){
    return ERROR_OK;
}

int Tracker::init_db_tables(){

    using namespace Poco::Data;
    int ret = 0;

    try{
        Poco::Data::SQLite::Connector::registerConnector();

        string dbFilename = config().getString("Database.Filename", "tracker.db");
        string clientInfoTableName = config().getString("Database.ClientInfo.TableName", "ClientInfo");
        string fileOwnerTableName = config().getString("Database.FileOwner.TableName", "FileOwner");

        pSession = new Session("SQLite", dbFilename);
        if( NULL == pSession ){
            throw std::runtime_error("Cannot make new session!");
        }

        Session& session = *pSession;
        session << "CREATE TABLE IF NOT EXISTS " << clientInfoTableName << "("
                   "ClientId VARCHAR(40) PRIMARY KEY,"
                   "UploadTotal VARCHAR(20),"
                   "DownloadTotal VARCHAR(20),"
                   "CreateTime VARCHAR(40),"
                   "LastLoginTime VARCHAR(40)"
                   ");", now;

        session << "CREATE TABLE IF NOT EXISTS " << fileOwnerTableName << "("
                   "id INTEGER PRIMARY KEY autoincrement,"
                   "FileId VARCHAR(40),"
                   "NodeId VARCHAR(40),"
                   "Percentage INTEGER"
                   ");", now;

    }catch(exception& e){
        poco_error_f1(logger(), "Got exception while init db : %s", e.what() );
        ret = -1;
    }

    return ret;
}

int main(int argc, char* argv[]){
    Tracker tracker;
    return tracker.run(argc, argv);
}

