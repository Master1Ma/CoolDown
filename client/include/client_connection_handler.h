#ifndef CLIENT_CONNECTION_HANDLER_H
#define CLIENT_CONNECTION_HANDLER_H

#include "client.pb.h"
#include "error_code.h"
#include <string>
#include <Poco/Logger.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/NObserver.h>
#include <Poco/Data/Session.h>
#include <google/protobuf/message.h>

using std::string;
using Poco::Logger;
using Poco::Net::SocketReactor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::WritableNotification;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;
using Poco::NObserver;
using Poco::AutoPtr;
using google::protobuf::Message;

class NetPack;

using namespace ClientProto;

namespace CoolDown{
    namespace Client{

        class CoolClient;
        class UploadTask;
        //class NetTaskManager;

        class ClientConnectionHandler{
            public:
                ClientConnectionHandler(const StreamSocket& sock, SocketReactor& reactor);
                ~ClientConnectionHandler();

                void onReadable(const AutoPtr<ReadableNotification>& pNotification);
                void onShutdown(const AutoPtr<ShutdownNotification>& pNotification);
                void onWritable(const AutoPtr<WritableNotification>& pNotification);

            private:
                void Process(const NetPack& in, NetPack* out);

                retcode_t HandleUploadRequest(const SharedPtr<Message>& in, UploadReply* reply, UploadTask* &pTask);
                retcode_t HandleShakeHand(const SharedPtr<Message>& in, ShakeHand* reply);

                StreamSocket sock_;
                string peerAddress_;
                SocketReactor& reactor_;
                CoolClient& app_;
                Logger& logger_;
                //NetTaskManager& uploadTaskManager_;
                bool last_request_upload;
                UploadTask* pTask;
        };
    }
}

#endif
