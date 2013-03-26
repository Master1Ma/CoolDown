#include "tracker_db_manager.h"
#include <string>
#include <Poco/Application.h>
#include <Poco/Exception.h>

using Poco::Application;
using Poco::Exception;

std::string connectionString = "user=root;password=123456;db=test;compress=true;auto-reconnect=true"

TrackerDBManager::TrackerDBManager()
:logger_(Application::instance().logger()),
session_(SessionFactory::instance().create("MySQL", connectionString)),
clientInfoTableName_("CLIENTINFO"),
fileOwnerTableName_("FILEOWNERINFO"){
        Poco::Data::MySQL::Connector::registerConnector();
        retcode_t ret = this->init_tables();
        if( ret != ERROR_OK ){
            poco_error_f1(logger_, "in TrackerDBManager, Init tables error, retcode : %d.", ret);
        }
}

TrackerDBManager::~TrackerDBManager(){
        Poco::Data::MySQL::Connector::unregisterConnector();
}

retcode_t TrackerDBManager::insert_or_update_login_info(const string& clientid, const string& ip, int port, const string loginTime){
    string sql;
    int init_upload_total = 0;
    int init_download_total = 0;
    format(sql, "INSERT INTO %s (CLIENTID, IP, MESSAGEPORT, UPLOADTOTAL, DOWNLOADTOTAL, CREATETIME, LASTLOGINTIME, ISONLINE) "
            "VALUES(?,?,?,?,?,?,?,1) "
            "ON DUPLICATE KEY UPDATE IP=VALUES(IP), MESSAGEPORT=VALUES(MESSAGEPORT), LOGINTIME=VALUES(LOGINTIME), ISONLINE=1");
    session_ << sql, use(clientid), use(ip), use(port), use(init_upload_total), use(init_download_total), use(loginTime), use(loginTime);
    return ERROR_OK;
}
retcode_t TrackerDBManager::update_logout_info(const string& clientid, Int64 downloadTotal, Int64 uploadTotal){
    return ERROR_OK;
}
retcode_t TrackerDBManager::search_client_info(const string& cientid, ClientInfo* pInfo){
    return ERROR_OK;
}
retcode_t TrackerDBManager::search_file_owner_info(const string& fileid, FileOwnerInfoPtrCollection* c){
    return ERROR_OK;
}
retcode_t TrackerDBManager::update_file_owner_info(const FileOwnerInfo& info){
    return ERROR_OK;
}

retcode_t TrackerDBManager::init_tables(){
    retcode_t ret = this->init_client_info_table();
    if( ret != ERROR_OK ){
        poco_error_f1(logger_, "in TrackerDBManager, Init client info table error, retcode : %d.", ret);
        return ret;
    }

    ret = this->init_file_owner_table();
    if( ret != ERROR_OK ){
        poco_error_f1(logger_, "in TrackerDBManager, Init file owner info table error, retcode : %d.", ret);
        return ret;
    }
}

retcode_t TrackerDBManager::init_client_info_table(){
    string clientInfoTableName("CLIENTINFO");
    try{
        session_ << "CREATE TABLE IF NOT EXISTS " << clientInfoTableName << "("
                   "CLIENTID VARCHAR(40) PRIMARY KEY,"
                   "IP VARCHAR(20),"
                   "MESSAGEPORT INTEGER,"
                   "UPLOADTOTAL BIGINT,"
                   "DOWNLOADTOTAL BIGINT,"
                   "CREATETIME VARCHAR(40),"
                   "LASTLOGINTIME VARCHAR(40),"
                   "ISONLINE INTEGER"
                   ");", now;
    }catch(Exception& e){
        poco_error_f1(logger_, "init client info table error, msg : %s", e.message());
        return ERROR_DB_INIT_TABLE_ERROR;
    }
    return ERROR_OK;
}

retcode_t TrackerDBManager::init_file_owner_info_table(){
    string fileOwnerTableName("FILEOWNERINFO");
    try{
        session_ << "CREATE TABLE IF NOT EXISTS " << fileOwnerTableName << "("
                   "id INTEGER PRIMARY KEY autoincrement,"
                   "FILEID VARCHAR(40),"
                   "NODEID VARCHAR(40),"
                   "PERCENTAGE INTEGER"
                   ");", now;
    }catch(Exception& e){
        poco_error_f1(logger_, "init file owner info table error, msg : %s", e.message());
        return ERROR_DB_INIT_TABLE_ERROR;
    }
    return ERROR_OK;
}
