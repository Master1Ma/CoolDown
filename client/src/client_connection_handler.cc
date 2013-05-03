#include "payload_type.h"
#include "netpack.h"
#include "client_connection_handler.h"
#include "client.h"
#include "upload_task.h"
#include "job.h"
#include <Poco/Util/Application.h>
#include <Poco/Format.h>
#include <Poco/Bugcheck.h>

using Poco::Util::Application;
using Poco::format;
namespace CoolDown{
    namespace Client{

        ClientConnectionHandler::ClientConnectionHandler(const StreamSocket& sock, SocketReactor& reactor)
        :sock_(sock), 
        peerAddress_(sock_.peerAddress().toString() ),
        reactor_(reactor),
        app_( dynamic_cast<CoolClient&>(Application::instance()) ),
        logger_( app_.logger() ){
            last_request_upload = false;
            pTask = NULL;
            poco_information_f1(logger_, "Connection from %s", peerAddress_);
            reactor_.addEventHandler(sock_, 
                                NObserver<ClientConnectionHandler, ReadableNotification>(*this, &ClientConnectionHandler::onReadable));

            reactor_.addEventHandler(sock_, 
                                NObserver<ClientConnectionHandler, ShutdownNotification>(*this, &ClientConnectionHandler::onShutdown));

        }

            ClientConnectionHandler::~ClientConnectionHandler(){
            poco_information_f1(logger_, "Disconnect from %s", peerAddress_);
            reactor_.removeEventHandler(sock_, 
                                NObserver<ClientConnectionHandler, ReadableNotification>(*this, &ClientConnectionHandler::onReadable));

            reactor_.removeEventHandler(sock_, 
                                NObserver<ClientConnectionHandler, ShutdownNotification>(*this, &ClientConnectionHandler::onShutdown));

        }

        void ClientConnectionHandler::onReadable(const AutoPtr<ReadableNotification>& pNotification){
            NetPack in, out;
            retcode_t ret = in.receiveFrom( sock_ );
            if( ret != ERROR_OK ){
                if( ret == ERROR_NET_GRACEFUL_SHUTDOWN ){
                    poco_notice(logger_, "Close connection by remote peer Gracefully.");
                }else{
                    poco_warning_f2(logger_, "Receive request netpack error, ret :%d, remote addr : %s.",
                            (int)ret, peerAddress_);
                }
                goto err;
            }

            this->Process(in, &out);
            poco_trace(logger_, "After Process in ClientConnectionHandler::onReadable.");
            ret = out.sendBy( sock_ );

            if( ret != ERROR_OK ){
                poco_warning_f2(logger_, "Send reply netpack error, ret : %d, remote addr : %s",
                        (int)ret, peerAddress_);
                if( this->last_request_upload == true ){
                    delete pTask;
                    pTask = NULL;
                }
                goto err;
            }else{
                poco_information_f1(logger_, "Finish process 1 request from %s", peerAddress_ );
                if( this->last_request_upload == true ){
                    //add task to uploadTaskManager and release ownership
                    poco_debug_f1(logger_, "add upload task to remote addr : '%s'", peerAddress_);
                    app_.upload_manager().start(pTask);
                    pTask = NULL;
                }
            }
            return;
err:
            delete this;
        }
        void ClientConnectionHandler::onShutdown(const AutoPtr<ShutdownNotification>& pNotification){
            poco_notice_f1(logger_, "Shutdown by remote peer, addr : %s", peerAddress_);
            delete this;
        }

        void ClientConnectionHandler::onWritable(const AutoPtr<WritableNotification>& pNotification){

        }

        void ClientConnectionHandler::Process(const NetPack& in, NetPack* out){
            SharedPtr<Message> req = in.message();
            retcode_t ret = ERROR_OK;
            ShakeHand sh;
            UploadReply ur;
            UploadTask* pTask = NULL;
            poco_debug_f1(logger_, "in ClientConnectionHandler, Processing request, type : %d", in.payloadtype() );
            switch(in.payloadtype()){
                case PAYLOAD_SHAKE_HAND:
                    ret = this->HandleShakeHand( req, &sh);
                    if( ret != ERROR_OK ){
                        poco_warning_f1(logger_, "HandleShakeHand failed with ret : %d", (int)ret);
                    }else{
                        poco_debug(logger_, "HandleShakeHand succeed!" );
                    }

                    out->set_message(PAYLOAD_SHAKE_HAND, sh);
                    this->last_request_upload = false;
                    break;
                case PAYLOAD_UPLOAD_REQUEST:
                    ret = this->HandleUploadRequest(req, &ur, pTask);
                    if( ret != ERROR_OK ){
                        poco_warning_f1(logger_, "HandleUploadRequest failed with ret : %d", (int)ret);
                    }else{
                        poco_trace(logger_, "HandleUploadRequest succeed!");
                        //since the handle func return ERROR_OK, no reason for pTask to be NULL
                        poco_assert( pTask != NULL );
                        poco_debug_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);
                        this->pTask = pTask;
                    }
                    out->set_message(PAYLOAD_UPLOAD_REPLY, ur);
                    this->last_request_upload = true;
                    break;
                default:
                    poco_warning_f2(logger_, "Unknown payload type : %d, remote addr : %s", 
                            (int)in.payloadtype(), peerAddress_);
                    break;
            }
        }

        retcode_t ClientConnectionHandler::HandleUploadRequest(const SharedPtr<Message>& in, UploadReply* reply, UploadTask* &pTask){
            SharedPtr<UploadRequest> req = in.cast<UploadRequest>();
            reply->set_returncode(ERROR_UNKNOWN);
            if( req.isNull() ){
                return ERROR_PROTO_TYPE_ERROR;
            }

            string clientid( req->clientid() );
            string fileid( req->fileid() );
            int chunk_pos = req->chunknumber();
            CoolClient::JobPtr pJob = app_.GetJobByFileid(fileid);
            if( pJob.isNull() ){
                poco_debug_f1(logger_, "We donot own this file '%s'", fileid);
                return ERROR_FILE_NOT_EXISTS;
            }

            poco_trace(logger_, "in HandleUploadRequest, get job succeed!");
            SharedPtr<File> file = pJob->MutableJobInfo()->localFileInfo.get_file(fileid);
            UInt64 offset =  pJob->MutableJobInfo()->torrentInfo.get_one_file_of_same_fileid(fileid)->chunk_offset(chunk_pos);
            int chunk_size = pJob->MutableJobInfo()->torrentInfo.get_one_file_of_same_fileid(fileid)->chunk_size(chunk_pos);

            poco_assert( file.isNull() == false );
            poco_assert( chunk_size > 0 );
            pTask = new UploadTask(pJob->MutableJobInfo()->downloadInfo, file, offset, chunk_size, this->sock_);
            poco_debug_f1(logger_, "Add new UploadTask for file '%s'", fileid);
            reply->set_returncode(ERROR_OK);
            return ERROR_OK;
        }

        retcode_t ClientConnectionHandler::HandleShakeHand(const SharedPtr<Message>& in, ShakeHand* reply){
            SharedPtr<ShakeHand> req = in.cast<ShakeHand>();
            if( req.isNull() ){
                return ERROR_PROTO_TYPE_ERROR;
            }
            string fileid( req->info().fileid() );

            reply->set_clientid( app_.clientid() );
            reply->mutable_info()->set_fileid(fileid);

            CoolClient::JobPtr pJob = app_.GetJobByFileid(fileid);
            FileInfo* pInfo = reply->mutable_info();
            if( pJob.isNull() ){
                poco_debug_f1(logger_, "peer request for file '%s' but we don't have that job running.", fileid);
                pInfo->set_hasfile(0);
                pInfo->set_percentage(0);
                pInfo->set_filebitcount(0);
            }else if( pJob->MutableJobInfo()->localFileInfo.has_file(fileid) == false ){
                poco_debug_f1(logger_, "peer request for file '%s' but we don't download that file", fileid);
                pInfo->set_hasfile(0);
                pInfo->set_percentage(0);
                pInfo->set_filebitcount(0);

            }else{
                double count = pJob->MutableJobInfo()->downloadInfo.bitmap_map[fileid]->count();
                double size = pJob->MutableJobInfo()->downloadInfo.bitmap_map[fileid]->size();
                double percentage = count / size;
                poco_debug_f2(logger_, "in ShakeHand, we have this file : '%s', percentage : '%f'", fileid, percentage);
                pInfo->set_hasfile(1);
                pInfo->set_percentage( pJob->MutableJobInfo()->downloadInfo.percentage_map[fileid] );
                Job::convert_bitmap_to_transport_format(pJob->MutableJobInfo()->downloadInfo.bitmap_map[fileid], pInfo);
            }
            return ERROR_OK;
        }


    }
}
