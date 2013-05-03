#ifndef JOB_H
#define JOB_H

#include "error_code.h"
#include "job_info.h"
#include "chunk_selector.h"
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include <Poco/TaskManager.h>
#include <Poco/TaskNotification.h>
#include <Poco/SharedPtr.h>
#include <Poco/ThreadPool.h>

using Poco::Logger;
using Poco::Runnable;
using Poco::FastMutex;
using Poco::Condition;
using Poco::TaskManager;
using Poco::TaskFinishedNotification;
using Poco::TaskFailedNotification;
using Poco::SharedPtr;
using Poco::ThreadPool;

namespace ClientProto{
    class FileInfo;
}
namespace CoolDown{
    namespace Client{

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
                const JobInfo& ConstJobInfo() const;

                bool is_running() const;

                static void convert_bitmap_to_transport_format(const file_bitmap_ptr& bitmap, ClientProto::FileInfo* pInfo);
                static void conver_transport_format_bitmap(const ClientProto::FileInfo& info, file_bitmap_ptr& bitmap);

            private:

                retcode_t request_clients(const string& fileid);
                retcode_t shake_hand(const string& fileid, const string& clientid);
                void reinit_file_owner_info(const string& fileid);

                CoolClient& app_;
                JobInfoPtr jobInfoPtr_;
                class JobInfo& jobInfo_;
                LocalSockManager& sockManager_;
                ChunkSelector cs_;
                
                FastMutex max_payload_mutex_;
                Condition max_payload_cond_;

                FastMutex available_thread_mutex_;
                Condition available_thread_cond_;

                FastMutex idle_sock_mutex_;

                bool is_running_;
                ThreadPool tp_;
                TaskManager tm_;
                Logger& logger_;
        };
    }
}
#endif
