#include "download_task.h"
#include "job_info.h"
#include "client.pb.h"
#include "netpack.h"
#include "payload_type.h"
#include "verification.h"
#include <Poco/Logger.h>
#include <Poco/Format.h>
#include <Poco/Exception.h>
#include <Poco/Buffer.h>
#include <Poco/SharedMemory.h>
#include <Poco/Util/Application.h>
#include <Poco/Bugcheck.h>

using Poco::Logger;
using Poco::format;
using Poco::Exception;
using Poco::Buffer;
using Poco::SharedMemory;
using Poco::Util::Application;
using namespace ClientProto;

namespace CoolDown{
    namespace Client{

        DownloadTask::DownloadTask(const TorrentFileInfo& info, DownloadInfo& downloadInfo, 
                const string& clientid, const SockPtr& sock, int chunk_pos, const File& file)
        :Task(format("%s:%d", sock->peerAddress().host().toString(), chunk_pos)), 
         fileInfo_(info), 
         downloadInfo_(downloadInfo),
         clientid_(clientid), 
         sock_(sock), 
         chunk_pos_(chunk_pos), 
         check_sum_(fileInfo_.chunk_checksum(chunk_pos_)), 
         file_(file), 
         reported_(false){
        }

        string DownloadTask::fileid() const{
            return fileInfo_.fileid();
        }

        void DownloadTask::runTask(){
            Logger& logger_ = Application::instance().logger();
            poco_trace(logger_, "Enter DownloadTask::runTask");
            UploadRequest req;
            req.set_clientid(clientid_);
            req.set_fileid(fileInfo_.fileid());
            req.set_chunknumber(chunk_pos_);


            NetPack pack( PAYLOAD_UPLOAD_REQUEST, req );
            retcode_t ret = pack.sendBy(*sock_);
            if( ERROR_OK != ret ){
                string msg( format("Error send upload request. ret : %d", (int)ret) );
                poco_notice(logger_, msg);
                throw Exception( msg );
            }
            ret = pack.receiveFrom(*sock_);
            if( ERROR_OK != ret ){
                string msg( format("Error recv upload reply. ret : %d", (int)ret) );
                poco_notice(logger_, msg);
                throw Exception( msg );
            }
            SharedPtr<UploadReply> reply = pack.message().cast<UploadReply>();
            if( reply.isNull() ){
                string msg( "Cannot cast recv message to UploadReply type." );
                poco_notice(logger_, msg);
                throw Exception( msg );
            }

            if( reply->returncode() != ERROR_OK ){
                string msg( format("remote client cannot handle upload request, returncode : %d", (int)reply->returncode()) );
                poco_notice(logger_, msg);
                throw Exception( msg );
            }

            //chunk_pos ranges from 0~chunk_count-1
            //bool isLastChunk = chunk_pos_ == ( fileInfo_.get_chunk_count() - 1 );
            poco_debug(logger_, "Upload Message exchange succeed, now goto the real download.");
            int chunk_size = fileInfo_.chunk_size(chunk_pos_);
            poco_assert( chunk_size != -1 );
            poco_debug_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);

            string content;
            int nRecv = 0;
            while( nRecv <= chunk_size){
                if( downloadInfo_.is_finished ){
                    throw Exception("Job Finished.");
                }

                if( downloadInfo_.is_download_paused ){
                    downloadInfo_.download_pause_cond.wait(downloadInfo_.download_pause_mutex);
                }
                if( downloadInfo_.bytes_download_this_second > downloadInfo_.upload_speed_limit ){
                    downloadInfo_.download_speed_limit_cond.wait(downloadInfo_.download_speed_limit_mutex);
                }else{
                    Buffer<char> recvBuffer(chunk_size);
                    int n = sock_->receiveBytes(recvBuffer.begin(), recvBuffer.size() );
                    downloadInfo_.bytes_download_this_second += n;
                    nRecv += n;
                    content.append( recvBuffer.begin(), n );
                }
            }
            poco_assert(content.length() == chunk_size );
            poco_assert(nRecv == chunk_size );

            string content_check_sum( Verification::get_verification_code(content) );

            if( content_check_sum != check_sum_){
                throw Exception(format("verify failed, original : %s, download_checksum : %s", check_sum_, content_check_sum));
            }

            SharedMemory sm(file_, SharedMemory::AM_WRITE);
            //64bits problem???
            memcpy(sm.begin() + fileInfo_.chunk_offset(chunk_pos_), content.data(), content.length() );
        }
    }
}
