#include "client.h"
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>

using Poco::Exception;
using Poco::Util::Application;

namespace CoolDown{
    namespace Client{

            void CoolClient::initialize(Application& self){
                Application::initialize(self);
            }

            void CoolClient::uninitialize(){
                Application::uninitialize();
            }

            int CoolClient::main(const vector<string>& args){
                return Application::EXIT_OK;
            }
    }
}
