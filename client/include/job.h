#ifndef JOB_H
#define JOB_H

#include <Poco/Logger.h>
#include <Poco/Runnable.h>
using Poco::Logger;
using Poco::Runnable;


namespace CoolDown{
    namespace Client{

        class JobInfo;
        class LocalSockManager;

        class Job : public Runnable{
            public:
                Job(JobInfo& info, LocalSockManager& m, Logger& logger);
                ~Job();
                void run();

                //void onFailed()

            private:
                JobInfo& info_;
                LocalSockManager& sockManager_;
                Logger& logger_;
                
        };
    }
}
#endif
