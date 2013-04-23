#ifndef NET_TASK_MANAGER_H
#define NET_TASK_MANAGER_H

#include <Poco/Logger.h>
#include <Poco/TaskManager.h>
#include <Poco/TaskNotification.h>

using Poco::Logger;
using Poco::TaskManager;
using Poco::TaskFinishedNotification;
using Poco::TaskFailedNotification;

namespace CoolDown{
    namespace Client{

        class NetTaskManager : public TaskManager{
            public:
                NetTaskManager(Logger& logger);
                ~NetTaskManager();

                void onTaskFinished(TaskFinishedNotification* pNf);
                void onTaskFailed(TaskFailedNotification* pNf);

            private:
                Logger& logger_;
        };

    }
}

#endif
