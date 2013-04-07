#ifndef DOWNLOAD_TASK_H
#define DOWNLOAD_TASK_H

#include <string>
#include <Poco/Task.h>
#include <Poco/File.h>
#include <Poco/Net/StreamSocket.h>

using std::string;
using Poco::Task;
using Poco::File;
using Poco::Net::StreamSocket;

namespace CoolDown{
    namespace Client{

        class JobInfo;
        class DownloadTask : public Task{
            public:
                DownloadTask(JobInfo& info, StreamSocket& sock, int chunk_pos, const string& check_sum, const File& file);
                void runTask();

            private:
                JobInfo& info_;
                StreamSocket& sock_;
                int chunk_pos_;
                string check_sum_;
                File file_;
        };
    }
}

#endif
