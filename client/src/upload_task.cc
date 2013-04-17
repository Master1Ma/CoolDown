#include "upload_task.h"
#include <Poco/SharedMemory.h>

using Poco::SharedMemory;

namespace CoolDown{
    namespace Client{

        UploadTask::UploadTask(JobInfo& jobInfo, const SockPtr& sock, int chunk_pos)
        :jobInfo_(jobInfo), sock_(sock), chunk_pos_(chunk_pos){
            
        }
        UploadTask::~UploadTask(){

        }

        void UploadTask::runTask(){
            SharedMemory sm(jobInfo_.localFileInfo.local_file, SharedMemory::AM_READ);
        }
    }
}
