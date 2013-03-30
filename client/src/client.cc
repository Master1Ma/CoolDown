#include "client.h"
#include "payload_type.h"
#include "netpack.h"
#include "tracker.pb.h"
#include <Poco/Logger.h>
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>
#include <Poco/Environment.h>

using Poco::Logger;
using Poco::Exception;
using Poco::Util::Application;
using namespace TrackerProto;

namespace CoolDown{
    namespace Client{

            void CoolClient::initialize(Application& self){
                this->init_error_ = false;
                this->clientid_ = Poco::Environment::nodeId();
                string msg;
                try{
                    sockManager_.assign( new LocalSockManager );
                    if( sockManager_.isNull() ){
                        msg = "Cannot create LocalSockManager.";
                        throw Exception(msg);
                    }
                }
                catch(Exception& e){
                    poco_error_f1(logger(), "Got exception in initialize : %s.", msg);
                    this->init_error_ = true;
                }
                Application::initialize(self);
            }

            void CoolClient::uninitialize(){
                Application::uninitialize();
            }

            int CoolClient::main(const vector<string>& args){
                if( this->init_error_ ){
                    return Application::EXIT_TEMPFAIL;
                }
                string tracker_address("localhost");
                string fileid("1234567890");
                this->login_tracker(tracker_address);
                this->publish_resource_to_tracker(tracker_address, fileid);
                this->report_progress(tracker_address, fileid, 25);
                return Application::EXIT_OK;
            }

            retcode_t CoolClient::login_tracker(const string& tracker_address, int port){
                retcode_t ret = sockManager_->login_tracker(tracker_address, port);
                if( ret != ERROR_OK ){
                    poco_warning_f3(logger(), "Cannot login tracker, ret : %hd, addr : %s, port : %d.", ret, tracker_address, port);
                }
                return ret;
            }

            retcode_t CoolClient::logout_tracker(const string& tracker_address, int port){
                retcode_t ret = sockManager_->logout_tracker(tracker_address, port);
                if( ret != ERROR_OK ){
                    poco_warning_f3(logger(), "Cannot logout tracker, ret : %d, addr : %s, port : %d.", ret, tracker_address, port);
                }
                return ret;
            }

            retcode_t CoolClient::publish_resource_to_tracker(const string& tracker_address, const string& fileid){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                PublishResource msg;
                msg.set_clientid(this->clientid());
                msg.set_fileid(fileid);
                NetPack req(PAYLOAD_PUBLISH_RESOURCE, msg);
                retcode_t ret = req.sendBy(*sock);
                SharedPtr<MessageReply> r;
                string error_msg("Unknown");
                if( ret != ERROR_OK ){
                    error_msg = "Cannot publish resource";
                    goto err;
                }
                ret = req.receiveFrom(*sock);
                if( ret != ERROR_OK){
                    error_msg = "Cannot receive comfirm message from tracker";
                    goto err;
                }

                r = req.message().cast<MessageReply>();
                if( r.isNull() ){
                    error_msg = "Cannot cast retrun message to MessageReply type";
                    goto err;
                }

                if( r->returncode() != ERROR_OK ){
                    error_msg = "Invalid return code from tracker";
                    goto err;
                }
err:
                poco_warning_f4(logger(), "in publish_resource_to_tracker: %s, ret:%d, tracker addr:%s, fileid:%s.", error_msg, ret, tracker_address, fileid);
                return ret;

            }
            retcode_t CoolClient::report_progress(const string& tracker_address, const string& fileid, int percentage){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                ReportProgress msg;
                msg.set_clientid( this->clientid() );
                msg.set_fileid( fileid );
                msg.set_percentage( percentage );
                NetPack req(PAYLOAD_REPORT_PROGRESS, msg);
                retcode_t ret = req.sendBy(*sock);
                SharedPtr<MessageReply> r;
                string error_msg("Unknown");
                if( ret != ERROR_OK ){
                    error_msg = "Cannot report progress";
                    goto err;
                }
                ret = req.receiveFrom(*sock);
                if( ret != ERROR_OK){
                    error_msg = "Cannot receive comfirm message from tracker";
                    goto err;
                }

                r = req.message().cast<MessageReply>();
                if( r.isNull() ){
                    error_msg = "Cannot cast retrun message to MessageReply type";
                    goto err;
                }

                if( r->returncode() != ERROR_OK ){
                    error_msg = "Invalid return code from tracker";
                    goto err;
                }
err:
                poco_warning_f4(logger(), "in report_progress: %s, ret:%d, tracker addr:%s, fileid:%s", 
                        error_msg, ret, tracker_address, fileid);
                return ret;
            }
            retcode_t CoolClient::request_clients(const string& tracker_address, const string& fileid, int currentPercentage, 
                int needCount, const ClientIdCollection& clientids){
                return ERROR_OK;
            }

            string CoolClient::clientid() const{
                return this->clientid_;
            }
    }
}
