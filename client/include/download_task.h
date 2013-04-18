#ifndef DOWNLOAD_TASK_H
#define DOWNLOAD_TASK_H

#include <string>
#include <Poco/Task.h>
#include <Poco/File.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>

using std::string;
using Poco::Task;
using Poco::File;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;

namespace CoolDown{
    namespace Client{
        typedef SharedPtr<StreamSocket> SockPtr;

        class TorrentFileInfo;
        struct DownloadInfo;
        class DownloadTask : public Task{
            public:
                DownloadTask(const TorrentFileInfo& info, DownloadInfo& downloadInfo, 
                        const string& clientid, const SockPtr& sock, int chunk_pos, const File& file);

                void runTask();
                int chunk_pos() const{
                    return this->chunk_pos_;
                }

                SockPtr sock() const{
                    return this->sock_;
                }

                string clientid() const{
                    return this->clientid_;
                }

                bool reported() const{
                    return this->reported_;
                }

                void set_reported(){
                    this->reported_ = true;
                }

                string fileid() const;

            private:
                const TorrentFileInfo& fileInfo_;
                DownloadInfo& downloadInfo_;
                string clientid_;
                SockPtr sock_;
                int chunk_pos_;
                string check_sum_;
                File file_;
                bool reported_;
        };
    }
}

#endif
