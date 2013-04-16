#ifndef UPLOAD_TASK_H
#define UPLOAD_TASK_H

#include <Poco/Task.h>

using Poco::Task;

class UploadTask : public Task{
    public:
        UploadTask();
        ~UploadTask();

    private:

};

#endif
