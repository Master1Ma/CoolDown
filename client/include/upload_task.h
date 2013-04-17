#ifndef UPLOAD_TASK_H
#define UPLOAD_TASK_H

#include <Poco/Task.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>

using Poco::Task;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;

namespace CoolDown{
    namespace Client{

        typedef SharedPtr<StreamSocket> SockPtr;

        class JobInfo;
        class UploadTask : public Task{
            public:
                UploadTask(JobInfo& jobInfo, const SockPtr& sock, int chunk_pos);
                ~UploadTask();

                void runTask();

            private:
                JobInfo& jobInfo_;
                SockPtr sock_;
                int chunk_pos_;

        };
    }
}

#endif
