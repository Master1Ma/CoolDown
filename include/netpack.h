#ifndef NETPACK_H
#define NETPACK_H
#include <string>

#include <Poco/Net/StreamSocket.h>
#include <Poco/Logger.h>
#include <Poco/Format.h>
#include <Poco/Types.h>
#include <Poco/Buffer.h>
#include "netpack_header.pb.h"
#include "error_code.h"

using std::string;
using Poco::Net::StreamSocket;
using Poco::Logger;
using Poco::format;
using Poco::Int32;
using Poco::Buffer;

class NetPack{
    public:
        typedef StreamSocket SockType;
        NetPack();
        NetPack(int payloadType, const string& payload);

        retcode_t sendBy(SockType& sock) const;
        retcode_t receiveFrom(SockType& sock);


        void clear(){
            this->payloadType_ = 0;
            this->payload_.clear();
            this->header_.Clear();
        };

        void set_payloadtype(int t){
            this->payloadType_ = t;
        }

        void set_payload(const string& payload){
            this->payload_ = payload;
        }

        int payloadtype() const{
            return this->payloadType_;
        };

        string payload() const {
            return this->payload_;
        };

        string debug_string() const{
            return format("type : %d\nlength : %d\npayload : %s", this->payloadType_, header_.payloadlength(), this->payload_);
        }

    private:
        retcode_t sendHeaderLength(SockType& sock) const;
        retcode_t receiveHeaderLength(SockType& sock, Int32* headerLength);

        int payloadType_;
        string payload_;
        Buffer<char> headerLengthBuf_;

        NetPackHeader header_;
        Logger& logger_;
};

#endif
