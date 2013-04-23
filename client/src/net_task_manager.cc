#include "net_task_manager.h"
#include <Poco/Observer.h>

using Poco::Observer;

namespace CoolDown{
    namespace Client{
        NetTaskManager::NetTaskManager(Logger& logger)
        :TaskManager(),
        logger_(logger){
            this->addObserver(
                    Observer<NetTaskManager, TaskFinishedNotification>
                    (*this, &NetTaskManager::onTaskFinished)
                    );
            this->addObserver(
                    Observer<NetTaskManager, TaskFailedNotification>
                    (*this, &NetTaskManager::onTaskFailed)
                    );
        }

        NetTaskManager::~NetTaskManager(){
        }

        void NetTaskManager::onTaskFinished(TaskFinishedNotification* pNf){
            poco_warning(logger_, "UploadTask Finished!");
        }

        void NetTaskManager::onTaskFailed(TaskFailedNotification* pNf){
            poco_warning_f1(logger_, "UploadTask Failed! reason : %s", pNf->reason().displayText());
        }
    }
}
