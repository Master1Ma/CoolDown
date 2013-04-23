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
            switch(in.payloadtype()){
                case PAYLOAD_SHAKE_HAND:
                    ret = this->HandleShakeHand( req, &sh);
                    if( ret != ERROR_OK ){
                        poco_warning_f1(logger_, "HandleShakeHand failed with ret : %d", (int)ret);
                    }else{
                        poco_debug(logger_, "HandlehakeHand succeed!" );
                    }

                    out->set_message(PAYLOAD_SHAKE_HAND, sh);
                    this->last_request_upload = false;
                    break;
                case PAYLOAD_UPLOAD_REQUEST:
                    ret = this->HandleUploadRequest(req, &ur, pTask);
                    if( ret != ERROR_OK ){
                        poco_warning_f1(logger_, "HandleUploadRequest failed with ret : %d", (int)ret);
                    }else{
                        //since the handle func return ERROR_OK, no reason for pTask to be NULL
                        poco_assert( pTask != NULL );
                        this->pTask = pTask;
                    }
                    this->last_request_upload = true;
                    out->set_message(PAYLOAD_UPLOAD_REPLY, ur);
                    break;
                default:
                    poco_warning_f2(logger_, "Unknown payload type : %d, remote addr : %s", 
                            (int)in.payloadtype(), peerAddress_);
                    break;
            }
        }

        retcode_t ClientConnectionHandler::HandleUploadRequest(const SharedPtr<Message>& in, UploadReply* reply, UploadTask* pTask){
            SharedPtr<UploadRequest> req = in.cast<UploadRequest>();
            if( req.isNull() ){
                return ERROR_PROTO_TYPE_ERROR;
            }

            string clientid( req->clientid() );
            string fileid( req->fileid() );
            int chunk_pos = req->chunknumber();
            CoolClient::JobPtr pJob = app_.get_job(fileid);
            if( pJob.isNull() ){
                return ERROR_FILE_NOT_EXISTS;
            }

            SharedPtr<File> file = pJob->MutableJobInfo()->localFileInfo.get_file(fileid);
            UInt64 offset =  pJob->MutableJobInfo()->torrentInfo.get_file(fileid)->chunk_offset(chunk_pos);
            int chunk_size = pJob->MutableJobInfo()->torrentInfo.get_file(fileid)->chunk_size(chunk_pos);

            poco_assert( file.isNull() == false );
            poco_assert( chunk_size > 0 );
            pTask = new UploadTask(file, offset, chunk_size, this->sock_ );
            return ERROR_OK;
        }

        retcode_t ClientConnectionHandler::HandleShakeHand(const SharedPtr<Message>& in, ShakeHand* reply){
            return ERROR_OK;
        }


    }
}
