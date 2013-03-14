#include "netpack.h"
#include <Poco/Net/SocketStream.h>
#include <Poco/Buffer.h>
#include <Poco/Util/Application.h>
#include <Poco/ByteOrder.h>

using Poco::Net::SocketStream;
using Poco::Buffer;
using Poco::Util::Application;

NetPack::NetPack()
        :payloadType_(0),
         logger_(Application::instance().logger()),
         headerLengthBuf_(sizeof(Int32)){
}

NetPack::NetPack(int payloadType, const string& payload)
        :payloadType_(payloadType),
        payload_(payload),
        logger_(Application::instance().logger()),
        headerLengthBuf_(sizeof(Int32)){

        header_.set_payloadtype(payloadType_);
        header_.set_payloadlength( payload_.length() );
}

int NetPack::sendBy(NetPack::SockType& sock) const{

    int ret = this->sendHeaderLength(sock);
    if( ret ){
        return -1;
    }

    int bytesSend;
    string headerContent;
    if( false == header_.SerializeToString( &headerContent ) ){
        poco_warning(logger_, "Cannot Serialize netpack header to string.");
        return -2;
    }

    bytesSend = sock.sendBytes( headerContent.data(), headerContent.length() );
    poco_debug_f1(logger_, "while send header : %d bytes send.", bytesSend);

    bytesSend = sock.sendBytes( payload_.data(), payload_.length() );
    if( bytesSend != payload_.length() ){
        return -3;
    }
    poco_debug_f1(logger_, "while send payload : %d bytes send.", bytesSend);
    poco_debug(logger_,"send payload succeed!");

    return 0;
}

int NetPack::receiveFrom(NetPack::SockType& sock){
    this->clear();
    Int32 headerLength;
    int ret = this->receiveHeaderLength(sock, &headerLength);
    if( ret ){
        return -1;
    }

    int bytesReceived;
    Buffer<char> headerBuf(headerLength);

    bytesReceived = sock.receiveBytes( headerBuf.begin(), headerBuf.size() );
    poco_debug_f1(logger_, "while receive header : %d bytes received.", bytesReceived);
    if( false == header_.ParseFromString( string(headerBuf.begin(), bytesReceived) ) ){
        poco_warning_f1(logger_, "Cannot Parse netpack_header from %s", sock.peerAddress().toString());
        return -2;
    }

    Buffer<char> buf( header_.payloadlength() );
    bytesReceived = sock.receiveBytes( buf.begin(), buf.size() );
    if( bytesReceived != header_.payloadlength() ){
        return -3;
    }
    poco_debug_f1(logger_, "while receive payload : %d bytes received.", bytesReceived);
    poco_debug(logger_,"receive payload succeed!");

    payload_.assign(buf.begin(), buf.size() );
    this->payloadType_ = header_.payloadtype();
    return 0;
}


int NetPack::sendHeaderLength(SockType& sock) const{
    Int32 headerLength = Poco::ByteOrder::toNetwork( header_.ByteSize() );
    if( sizeof(Int32) == sock.sendBytes(&headerLength, sizeof(Int32)) ){
        return 0;
    }
    return -1;
}

int NetPack::receiveHeaderLength(SockType& sock, Int32* headerLength){
    Buffer<char>& buf = this->headerLengthBuf_;
    if( sizeof(Int32) != sock.receiveBytes(buf.begin(), buf.size() ) ){
        return -1;
    }
    *headerLength = Poco::ByteOrder::fromNetwork( *(reinterpret_cast<Int32*>(buf.begin())) );
    return 0;
}
