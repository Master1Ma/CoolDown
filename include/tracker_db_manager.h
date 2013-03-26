#ifndef TRACKER_DB_MANAGER
#define TRACKER_DB_MANAGER

#include "error_code.h"
#include "client_info.h"
#include "file_owner_info.h"
#include <vector>
#include <string>
#include <Poco/Types.h>
#include <Poco/SharedPtr.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Logger.h>

using std::vector;
using std::string;
using Poco::SharedPtr;
using Poco::Session;
using Poco::Logger;

class TrackerDBManager{
    public:
        typedef SharedPtr<FileOwnerInfo> FileOwnerInfoPtr;
        typedef vector<FileOwnerInfoPtr> FileOwnerInfoPtrCollection;

        TrackerDBManager();
        ~TrackerDBManager();

        retcode_t insert_or_update_login_info(const string& clientid, const string& ip, int port, 
                const string loginTime);
        retcode_t update_logout_info(const string& clientid, Int64 downloadTotal, Int64 uploadTotal);
        retcode_t search_client_info(const string& cientid, ClientInfo* pInfo);
        retcode_t search_file_owner_info(const string& fileid, FileOwnerInfoPtrCollection* c);
        retcode_t update_file_owner_info(const FileOwnerInfo& info);

    private:
        retcode_t init_tables();

        retcode_t init_client_info_table();
        retcode_t init_file_owner_info_table();

        Logger& logger_;
        Session session_;

        string clientInfoTableName_;
        string fileOwnerTableName_;
};

#endif
