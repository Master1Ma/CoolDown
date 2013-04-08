#ifndef JOB_H
#define JOB_H

#include "chunk_selector.h"
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include <Poco/TaskManager.h>
#include <Poco/TaskNotification.h>
using Poco::Logger;
using Poco::Runnable;
using Poco::FastMutex;
using Poco::Condition;
using Poco::TaskManager;
using Poco::TaskFinishedNotification;
using Poco::TaskFailedNotification;


namespace CoolDown{
    namespace Client{

        class JobInfo;
        class LocalSockManager;

        class Job : public Runnable{
            public:
                Job(JobInfo& info, LocalSockManager& m, Logger& logger);
                ~Job();
                void run();

                void onFinished(TaskFinishedNotification* pNf);
                void onFailed(TaskFailedNotification* pNf);

            private:
                JobInfo& jobInfo_;
                LocalSockManager& sockManager_;
                ChunkSelector cs_;
                Logger& logger_;
                
                FastMutex max_payload_mutex_;
                Condition max_payload_cond_;
                TaskManager tm_;
                
        };
    }
}
#endif
