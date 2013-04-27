#ifndef JOB_INFO_COLLECTOR_H
#define JOB_INFO_COLLECTOR_H

#include <Poco/Runnable.h>

namespace CoolDown{
    namespace Client{

        class CoolClient;
        class JobInfoCollector : public Poco::Runnable{
            public:
                JobInfoCollector();
                ~JobInfoCollector();

                void run();

            private:
                CoolClient& app_;
        };
    }
}

#endif
