#ifndef NET_TASK_MANAGER_H
#define NET_TASK_MANAGER_H

#include <Poco/TaskManager.h>

using Poco::TaskManager;

namespace CoolDown{
    namespace Client{

        class NetTaskManager : public TaskManager{
            public:
                NetTaskManager
        };

    }
}

#endif
