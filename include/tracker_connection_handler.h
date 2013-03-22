#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "tracker.pb.h"
#include <Poco/Logger.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/NObserver.h>
#include <Poco/Data/Session.h>

using Poco::Logger;
using Poco::Net::SocketReactor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Data::Session;

class Tracker;
class NetPack;

using namespace TrackerProto;

class TrackerConnectionHandler
{
    public:
        TrackerConnectionHandler(const StreamSocket& sock, SocketReactor& reactor);
        ~TrackerConnectionHandler();

        void onReadable(const AutoPtr<ReadableNotification>& pNotification);
        void onShutdown(const AutoPtr<ShutdownNotification>& pNotification);

        void Process(const NetPack& in, NetPack* out);

    private:
        const static size_t BUF_LEN = 1 << 10;

        retcode_t HandleLogin(const NetPack& in, NetPack* out);
        retcode_t HandleLogOut(const NetPack& in, NetPack* out);
        retcode_t HandleRequestPeer(const NetPack& in, NetPack* out);
        retcode_t HandleReportProgress(const NetPack& in, NetPack* out);
        retcode_t HandlePublishResource(const NetPack& in, NetPack* out);

        retcode_t ParseProto(const string& name, Message* proto);
        /*
        */

        StreamSocket sock_;
        SocketReactor& reactor_;
        Logger& logger_;
        Session& session_;
        Tracker& app_;
};
#endif
