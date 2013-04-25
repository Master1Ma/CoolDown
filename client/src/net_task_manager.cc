#include "net_task_manager.h"
#include "upload_task.h"
#include <Poco/Observer.h>
#include <Poco/Bugcheck.h>

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
            UploadTask* pTask = dynamic_cast<UploadTask*>( pNf->task() );
            poco_assert( pTask != NULL );
            poco_debug_f1(logger_, "UploadTask Finished! %s", pTask->DebugString());
        }

        void NetTaskManager::onTaskFailed(TaskFailedNotification* pNf){
            UploadTask* pTask = dynamic_cast<UploadTask*>( pNf->task() );
            poco_assert( pTask != NULL );
            poco_warning_f2(logger_, "UploadTask Failed! %s\nreason : %s", pTask->DebugString(), pNf->reason().displayText());
        }
    }
}
