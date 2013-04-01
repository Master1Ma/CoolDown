#ifndef CLIENT_H
#define CLIENT_H

#include "error_code.h"
#include "local_sock_manager.h"
#include <vector>
#include <string>
#include <Poco/Util/Application.h>
#include <google/protobuf/message.h>
using google::protobuf::Message;

using std::vector;
using std::string;
using Poco::Util::Application;

namespace CoolDown{
    namespace Client{

            class CoolClient : public Application{
                public:
                    CoolClient(int argc, char* argv[]);
                    const static unsigned int TRACKER_PORT = 9977;
                    typedef vector<string> ClientIdCollection;
                    typedef LocalSockManager::LocalSockManagerPtr LocalSockManagerPtr;
                    void initialize(Application& self);
                    void uninitialize();
                    int main(const vector<string>& args);

                    retcode_t login_tracker(const string& tracker_address, int port = TRACKER_PORT);
                    retcode_t logout_tracker(const string& tracker_address, int port = TRACKER_PORT);
                    retcode_t publish_resource_to_tracker(const string& tracker_address, const string& fileid);
                    retcode_t report_progress(const string& tracker_address, const string& fileid, int percentage);
                    retcode_t request_clients(const string& tracker_address, const string& fileid, int currentPercentage, 
                                          int needCount, const ClientIdCollection& clientids);

                    string clientid() const;

                private:

                    template<typename ReplyMessageType>
                    retcode_t handle_reply_message(LocalSockManager::SockPtr& sock, 
                    const Message& msg, int payload_type, SharedPtr<ReplyMessageType>* out);

                    bool init_error_;
                    string clientid_;
                    LocalSockManagerPtr sockManager_;
            };
    }
}

#endif
