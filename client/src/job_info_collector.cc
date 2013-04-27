#include "job_info_collector.h"
#include "client.h"

#include <Poco/Util/Application.h>
#include <Poco/Timer.h>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>

using Poco::Timer;
using Poco::TimerCallback;
using Poco::Condition;
using Poco::FastMutex;

namespace CoolDown{
    namespace Client{

        JobInfoCollector::JobInfoCollector()
        :app_( dynamic_cast<CoolClient&>(Application::instance()) ){
        }

        JobInfoCollector::~JobInfoCollector(){
        }

        void JobInfoCollector::run(){
            Timer timer(0, 1000);
            timer.start(TimerCallback<CoolClient>(app_, &CoolClient::onJobInfoCollectorWakeUp));
            Condition& cond = app_.GetInfoCollectorCond();
            FastMutex mutex;
            cond.wait(mutex);
        }

    }
}
