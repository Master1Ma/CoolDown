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

        class JobInfo;
        class DownloadTask : public Task{
            public:
                DownloadTask(JobInfo& info, const string& clientid, const SockPtr& sock, int chunk_pos, const string& check_sum, const File& file);
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

            private:
                JobInfo& jobInfo_;
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
