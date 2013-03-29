#ifndef CLIENT_H
#define CLIENT_H

#include <vector>
#include <string>
#include <Poco/Util/Application.h>

using std::vector;
using std::string;
using Poco::Util::Application;

namespace CoolDown{
    namespace Client{

            class CoolClient : public Application{
                public:
                    void initialize(Application& self);
                    void uninitialize();
                    int main(const vector<string>& args);
            };
    }
}

#endif
