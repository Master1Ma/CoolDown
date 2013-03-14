#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

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

class TrackerConnectionHandler
{
    public:
        TrackerConnectionHandler(const StreamSocket& sock, SocketReactor& reactor);
        ~TrackerConnectionHandler();

        void onReadable(const AutoPtr<ReadableNotification>& pNotification);
        void onShutdown(const AutoPtr<ShutdownNotification>& pNotification);

    private:
        const static size_t BUF_LEN = 1 << 10;

        StreamSocket sock_;
        SocketReactor& reactor_;
        Logger& logger_;
        Session& session_;
};
#endif
