#include "upload_task.h"
#include "job_info.h"
#include "verification.h"
#include <Poco/Logger.h>
#include <Poco/Util/Application.h>
#include <Poco/SharedMemory.h>
#include <Poco/Format.h>
#include <Poco/Exception.h>

using Poco::Logger;
using Poco::Util::Application;
using Poco::SharedMemory;
using Poco::format;
using Poco::Exception;

namespace CoolDown{
    namespace Client{

        UploadTask::UploadTask(DownloadInfo& downloadInfo, const FilePtr& file, UInt64 offset, int chunk_size, StreamSocket& sock)
        :Task( format("Upload task to %s", sock.peerAddress().host().toString()) ), 
        peerAddress_(sock.peerAddress().host().toString()),
        downloadInfo_(downloadInfo),
        file_(file),
        offset_(offset),
        chunk_size_(chunk_size),
        sock_(sock)
        {
            
        }
        UploadTask::~UploadTask(){

        }

        void UploadTask::runTask(){
            Logger& logger_ = Application::instance().logger();
            poco_trace(logger_, "enter UploadTask::runTask");
            string content;
            {
                SharedMemory sm(*file_, SharedMemory::AM_READ);
                content = string( sm.begin() + offset_, chunk_size_ );
            }
            poco_assert( content.size() == chunk_size_ );
            poco_debug_f2(logger_, "going to send %d bytes to '%s'", chunk_size_, this->peerAddress_);
            int nSend = 0;
            while( nSend < chunk_size_ ){
                if( downloadInfo_.is_stopped){
                    throw Exception("UploadTask is stopped by setting is_stoped.");
                }else if ( downloadInfo_.is_job_removed ){
                    throw Exception("UploadTask is stopped by setting is_job_removed.");
                }
                int send_this_time = sock_.sendBytes( content.data() + nSend , chunk_size_ - nSend);
                poco_debug_f2( logger_, "in upload task, send %d bytes this time, %d bytes to send.", send_this_time, chunk_size_ - send_this_time );
                if( send_this_time <= 0 ){
                    poco_warning_f1(logger_, "bytes send this time is %d", send_this_time );
                    throw Exception("send bytes <= 0");
                }
                nSend += send_this_time;
                downloadInfo_.bytes_upload_this_second += send_this_time;
            }
            if( nSend != chunk_size_ ){
                throw Exception( format("%s, chunk_size is %d bytes but only send %d", name(), chunk_size_, nSend) );
            }

            string vc = Verification::get_verification_code(content);
            poco_debug_f4(logger_, "UploadTask succeed, \nlocal file path : %s\n offset : %Lu\nchunk_size : %d\nvc : %s", file_->path(), offset_, chunk_size_, vc);
            poco_debug_f2(logger_, "send %d bytes to '%s' succeed.", nSend, peerAddress_);
        }
    }
}
