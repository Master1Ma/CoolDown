#include "job_info.h"
#include "client.h"
#include <Poco/Util/Application.h>

using Poco::Util::Application;

namespace CoolDown{
    namespace Client{

        JobInfo::JobInfo()
        :app_(dynamic_cast<CoolClient&>(Application::instance()) ),
         logger_(app_.logger())
        {
        }

        JobInfo::~JobInfo(){
        }

        string JobInfo::clientid() const{
            return app_.clientid();
        }

    }
}
