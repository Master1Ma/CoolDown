#ifndef JOB_H
#define JOB_H

#include "error_code.h"
#include "chunk_selector.h"
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include <Poco/TaskManager.h>
#include <Poco/TaskNotification.h>
#include <Poco/SharedPtr.h>

using Poco::Logger;
using Poco::Runnable;
using Poco::FastMutex;
using Poco::Condition;
using Poco::TaskManager;
using Poco::TaskFinishedNotification;
using Poco::TaskFailedNotification;
using Poco::SharedPtr;

namespace CoolDown{
    namespace Client{

        class JobInfo;
        class LocalSockManager;
        class CoolClient;
        typedef SharedPtr<JobInfo> JobInfoPtr;

        class Job : public Runnable{
            public:
                Job(const JobInfoPtr& info, LocalSockManager& m, Logger& logger);
                ~Job();
                void run();

                void onFinished(TaskFinishedNotification* pNf);
                void onFailed(TaskFailedNotification* pNf);
                JobInfoPtr MutableJobInfo();
                const JobInfo& JobInfo() const;

            private:

                retcode_t request_clients(const string& fileid);
                retcode_t shake_hand(const string& fileid, const string& clientid);

                CoolClient& app_;
                JobInfoPtr jobInfoPtr_;
                class JobInfo& jobInfo_;
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
