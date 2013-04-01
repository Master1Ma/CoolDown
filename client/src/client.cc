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
            CoolClient::CoolClient(int argc, char* argv[])
            :Application(argc, argv){
            }

            void CoolClient::initialize(Application& self){
                loadConfiguration();
                Application::initialize(self);

                setLogger(Logger::get("ConsoleLogger"));
                poco_debug(logger(), "test ConsoleLogger in initialize.");
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
                ClientIdCollection c;
                this->request_clients(tracker_address, fileid, 20, 90, c);
                return Application::EXIT_OK;
            }

            retcode_t CoolClient::login_tracker(const string& tracker_address, int port){
                retcode_t ret = sockManager_->connect_tracker(tracker_address, port);
                if( ret != ERROR_OK ){
                    poco_warning_f3(logger(), "Cannot connect tracker, ret : %hd, addr : %s, port : %d.", ret, tracker_address, port);
                    return ret;
                }
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock( tracker_address ) );
                Login msg;
                msg.set_clientid( this->clientid() );
                SharedPtr<MessageReply> r;

                ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_LOGIN, &r);
                return ret;
            }

            retcode_t CoolClient::logout_tracker(const string& tracker_address, int port){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                Logout msg;
                msg.set_clientid( this->clientid() );
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_LOGOUT, &r);
                return ret;
            }

            retcode_t CoolClient::publish_resource_to_tracker(const string& tracker_address, const string& fileid){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                PublishResource msg;
                msg.set_clientid(this->clientid());
                msg.set_fileid(fileid);
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_PUBLISH_RESOURCE, &r);
                return ret;

            }

            retcode_t CoolClient::report_progress(const string& tracker_address, const string& fileid, int percentage){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                ReportProgress msg;
                msg.set_clientid( this->clientid() );
                msg.set_fileid( fileid );
                msg.set_percentage( percentage );
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>(sock, msg, PAYLOAD_REPORT_PROGRESS, &r);
                return ret;
            }

            retcode_t CoolClient::request_clients(const string& tracker_address, const string& fileid, int currentPercentage, 
                int needCount, const ClientIdCollection& clientids){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address ));
                QueryPeer msg;
                msg.set_fileid(fileid);
                msg.set_percentage(currentPercentage);
                msg.set_needcount(needCount);
                ClientIdCollection::const_iterator iter = clientids.begin();
                while( iter != clientids.end() ){
                    msg.add_ownedclients(*iter);
                    ++iter;
                }
                SharedPtr<QueryPeerReply> r;
                retcode_t ret = handle_reply_message< QueryPeerReply >( sock, msg, PAYLOAD_REQUEST_PEER, &r);
                poco_notice_f1(logger(), "Recv %d clients by QueryPeer.", r->info().size());
                return ret;
            }

            template<typename ReplyMessageType>
            retcode_t CoolClient::handle_reply_message(LocalSockManager::SockPtr& sock, 
                    const Message& msg, int payload_type, SharedPtr<ReplyMessageType>* out){

                    NetPack req(payload_type, msg);
                    poco_information_f1(logger(), "pack to send : \n%s", req.debug_string());
                    retcode_t ret = req.sendBy(*sock);
                    string error_msg("Unknown");
                    if( ret != ERROR_OK ){
                        error_msg = format("cannot send request, payload type=%d", payload_type);
                        goto err;
                    }
                    ret = req.receiveFrom(*sock);
                    if( ret != ERROR_OK){
                        error_msg = "Cannot receive comfirm message from tracker";
                        goto err;
                    }

                    *out = req.message().cast<ReplyMessageType>();
                    if( out->isNull() ){
                        error_msg = "Cannot cast retrun message to certain type";
                        goto err;
                    }

                    if( (*out)->returncode() != ERROR_OK ){
                        error_msg = "Invalid return code from tracker";
                        goto err;
                    }
                    return ERROR_OK;
    err:
                    poco_warning_f3(logger(), "in handle_reply_message: %s, ret:%d, remote addr:%s", 
                            error_msg, ret, sock->peerAddress().toString());
                    return ret;
                
            }

            string CoolClient::clientid() const{
                return this->clientid_;
            }
    }
}
