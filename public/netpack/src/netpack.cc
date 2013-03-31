#include "netpack.h"
#include <Poco/Net/SocketStream.h>
#include <Poco/Buffer.h>
#include <Poco/Util/Application.h>
#include <Poco/ByteOrder.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

using Poco::Net::SocketStream;
using Poco::Buffer;
using Poco::Util::Application;

NetPack::NetPack()
        :payloadType_(0),
         logger_(Application::instance().logger()),
         headerLengthBuf_(sizeof(Int32)){
}

NetPack::NetPack(int payloadType, const Message& msg)
        :payloadType_(payloadType),
        messageName_( msg.GetDescriptor()->full_name() ),
        logger_(Application::instance().logger()),
        headerLengthBuf_(sizeof(Int32)){

        if( false == msg.SerializeToString(&this->payload_) ){
            poco_error_f1(logger_, "Cannot serialize message to string, name : %s.", messageName_);
        }

        header_.set_payloadtype(payloadType_);
        header_.set_payloadlength( payload_.length() );
        header_.set_messagename(messageName_);
}

retcode_t NetPack::sendBy(NetPack::SockType& sock) const{

    retcode_t ret = this->sendHeaderLength(sock);
    if( ret != ERROR_OK ){
        return ERROR_PACK_SEND_HEADER_LENGTH;
    }

    int bytesSend;
    string headerContent;
    if( false == header_.SerializeToString( &headerContent ) ){
        poco_warning(logger_, "Cannot Serialize netpack header to string.");
        return ERROR_PACK_PROTO_SERIALIZE;
    }

    bytesSend = sock.sendBytes( headerContent.data(), headerContent.length() );
    poco_debug_f1(logger_, "while send header : %d bytes send.", bytesSend);

    bytesSend = sock.sendBytes( payload_.data(), payload_.length() );
    if( bytesSend != payload_.length() ){
        return ERROR_PACK_SEND_PAYLOAD;
    }
    poco_debug_f1(logger_, "while send payload : %d bytes send.", bytesSend);
    poco_debug(logger_,"send payload succeed!");

    return ERROR_OK;
}

retcode_t NetPack::receiveFrom(NetPack::SockType& sock){
    this->clear();
    Int32 headerLength;
    retcode_t ret = this->receiveHeaderLength(sock, &headerLength);
    if( ret != ERROR_OK ){
        return ret;
    }

    int bytesReceived;
    Buffer<char> headerBuf(headerLength);

    bytesReceived = sock.receiveBytes( headerBuf.begin(), headerBuf.size() );
    poco_debug_f1(logger_, "while receive header : %d bytes received.", bytesReceived);
    if( false == header_.ParseFromString( string(headerBuf.begin(), bytesReceived) ) ){
        poco_warning_f1(logger_, "Cannot Parse netpack_header from %s", sock.peerAddress().toString());
        return ERROR_PACK_PROTO_PARSE;
    }

    Buffer<char> buf( header_.payloadlength() );
    bytesReceived = sock.receiveBytes( buf.begin(), buf.size() );
    if( bytesReceived != header_.payloadlength() ){
        return ERROR_PACK_RECV_PAYLOAD;
    }
    poco_debug_f1(logger_, "while receive payload : %d bytes received.", bytesReceived);
    poco_debug(logger_,"receive payload succeed!");

    payload_.assign(buf.begin(), buf.size() );
    this->messageName_ = header_.messagename();
    this->payloadType_ = header_.payloadtype();
    return ERROR_OK;
}

SharedPtr<Message> NetPack::message() const{

    using namespace google::protobuf;
    Message* message = NULL;
    const Descriptor* descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(this->messageName_);
    if (descriptor)
    {
        const Message* prototype = MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
            message = prototype->New();
        }
    }
    return SharedPtr<Message>(message);
}

void NetPack::set_message(int payloadType, const Message& msg){
    this->clear();
    this->payloadType_ = payloadType;
    this->messageName_ = msg.GetDescriptor()->full_name();

    if( false == msg.SerializeToString( &this->payload_ ) ){
            poco_error_f1(logger_, "Cannot serialize message to string, name : %s.", messageName_);
    }

    this->header_.set_payloadtype( payloadType );
    this->header_.set_payloadlength( this->payload_.length() );
    this->header_.set_messagename( this->messageName_ );

}

retcode_t NetPack::sendHeaderLength(SockType& sock) const{
    Int32 headerLength = Poco::ByteOrder::toNetwork( header_.ByteSize() );
    if( sizeof(Int32) == sock.sendBytes(&headerLength, sizeof(Int32)) ){
        return ERROR_OK;
    }
    return ERROR_PACK_SEND_HEADER_LENGTH;
}

retcode_t NetPack::receiveHeaderLength(SockType& sock, Int32* headerLength){
    Buffer<char>& buf = this->headerLengthBuf_;
    int nCount = sock.receiveBytes(buf.begin(), buf.size() );
    if( nCount != sizeof(Int32) ){
        if( nCount == 0 ){
            //graceful shotdown by client
            return ERROR_NET_GRACEFUL_SHUTDOWN;
        }else{
            //unknown error
            return ERROR_PACK_RECV_HEADER_LENGTH;
        }
    }
    *headerLength = Poco::ByteOrder::fromNetwork( *(reinterpret_cast<Int32*>(buf.begin())) );
    return ERROR_OK;
}
