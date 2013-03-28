#include "client.h"
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>

using Poco::Exception;
using Poco::Util::Application;

void Client::initialize(Application& self){
    Application::initialize(self);
}

void Client::uninitialize(){
    Application::uninitialize();
}

int Client::main(const vector<string>& args){
    return Application::EXIT_OK;
}
