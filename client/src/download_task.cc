#include "download_task.h"
#include "job_info.h"
#include "client.pb.h"
#include "netpack.h"
#include "payload_type.h"
#include "verification.h"
#include <Poco/Format.h>
#include <Poco/Exception.h>
#include <Poco/Buffer.h>
#include <Poco/SharedMemory.h>

using Poco::format;
using Poco::Exception;
using Poco::Buffer;
using Poco::SharedMemory;
using namespace ClientProto;

namespace CoolDown{
    namespace Client{

        DownloadTask::DownloadTask(JobInfo& info, StreamSocket& sock, int chunk_pos, const string& check_sum, const File& file)
        :Task(format("%s:%d", sock.peerAddress().host().toString(), chunk_pos)), 
         info_(info), sock_(sock), chunk_pos_(chunk_pos), check_sum_(check_sum), file_(file){

        }

        void DownloadTask::runTask(){
            UploadRequest req;
            req.set_clientid(info_.clientid());
            req.set_fileid(info_.torrentInfo.fileid);
            req.set_chunknumber(chunk_pos_);

            NetPack pack( PAYLOAD_UPLOAD_REQUEST, req );
            retcode_t ret = pack.sendBy(sock_);
            if( ERROR_OK != ret ){
                throw Exception( format("Error send upload request. ret : %d", (int)ret) );
            }
            ret = pack.receiveFrom(sock_);
            if( ERROR_OK != ret ){
                throw Exception( format("Error recv upload reply. ret : %d", (int)ret) );
            }
            SharedPtr<UploadReply> reply = pack.message().cast<UploadReply>();
            if( reply.isNull() ){
                throw Exception( "Cannot cast recv message to UploadReply type." );
            }

            if( reply->returncode() != ERROR_OK ){
                throw Exception( format("remote client cannot handle upload request, returncode : %d", (int)reply->returncode()) );
            }

            //chunk_pos ranges from 0~chunk_count-1
            bool isLastChunk = chunk_pos_ == ( info_.torrentInfo.chunk_count - 1 );
            int chunk_size = 0;
            if( isLastChunk ){
                chunk_size = info_.torrentInfo.file_size - ( info_.torrentInfo.size_per_chunk * info_.torrentInfo.chunk_count - 1);
            }else{
                chunk_size = info_.torrentInfo.size_per_chunk;
            }

            //the size of last chunk must less-equal than the size of normal chunk
            poco_assert( chunk_size <= info_.torrentInfo.size_per_chunk );

            string content;
            int nRecv = 0;
            while( nRecv <= chunk_size){
                if( info_.downloadInfo.is_finished ){
                    throw Exception("Job Finished.");
                }

                if( info_.downloadInfo.is_download_paused ){
                    info_.download_pause_cond.wait(info_.download_pause_mutex);
                }
                if( info_.downloadInfo.bytes_download_this_second > info_.downloadInfo.upload_speed_limit ){
                    info_.download_speed_limit_cond.wait(info_.download_speed_limit_mutex);
                }else{
                    Buffer<char> recvBuffer(chunk_size);
                    int n = sock_.receiveBytes(recvBuffer.begin(), recvBuffer.size() );
                    info_.downloadInfo.bytes_download_this_second += n;
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
            //32bits vs 64bits problem???
            memcpy(sm.begin() + chunk_pos_ * info_.torrentInfo.size_per_chunk, content.data(), content.length() );
        }
    }
}
