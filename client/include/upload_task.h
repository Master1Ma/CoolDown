#ifndef UPLOAD_TASK_H
#define UPLOAD_TASK_H

#include <Poco/Task.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/File.h>
#include <Poco/Types.h>

using Poco::Task;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;
using Poco::File;
using Poco::UInt64;

namespace CoolDown{
    namespace Client{

        typedef SharedPtr<StreamSocket> SockPtr;
        typedef SharedPtr<File> FilePtr;

        class UploadTask : public Task{
            public:
                UploadTask(const FilePtr& file, UInt64 offset, int chunk_size, StreamSocket& sock);
                ~UploadTask();

                void runTask();
                UInt64 offset() const{
                    return this->offset_;
                }

                int chunk_size() const{
                    return this->chunk_size_;
                }

            private:
                FilePtr file_;
                UInt64 offset_;
                int chunk_size_;
                StreamSocket& sock_;

        };
    }
}

#endif
