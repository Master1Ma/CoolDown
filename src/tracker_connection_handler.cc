#include "tracker_connection_handler.h"
#include "netpack.h"
#include "tracker.h"
#include "payload_type.h"
#include <Poco/Util/Application.h>
#include <Poco/Logger.h>
#include <google/protobuf/descriptor.h>

using Poco::Util::Application;

TrackerConnectionHandler::TrackerConnectionHandler(const StreamSocket& sock, SocketReactor& reactor):
    sock_(sock),
    reactor_(reactor),
    logger_( Application::instance().logger() ),
    app_( dynamic_cast<Tracker&>(Application::instance()) ),
    session_( app_.session() )
{
    poco_information_f1(logger_, "Connection from %s", sock_.peerAddress().toString() );

    reactor_.addEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ReadableNotification>(*this, &TrackerConnectionHandler::onReadable));

    reactor_.addEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ShutdownNotification>(*this, &TrackerConnectionHandler::onShutdown));
}

TrackerConnectionHandler::~TrackerConnectionHandler()
{
    poco_information_f1(logger_, "Disconnect from %s", sock_.peerAddress().toString() );

    reactor_.removeEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ReadableNotification>(*this, &TrackerConnectionHandler::onReadable));

    reactor_.removeEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ShutdownNotification>(*this, &TrackerConnectionHandler::onShutdown));
}

void TrackerConnectionHandler::onReadable(const AutoPtr<ReadableNotification>& pNotification)
{

    NetPack pack;
    NetPack retMsg;

    int ret = pack.receiveFrom( sock_ );

    if( ret ){
        poco_warning_f2(logger_, "pack.receiveFrom error! ret : %d, remote addr : %s.", 
                ret, sock_.peerAddress().toString());
        goto err;
    }

    poco_notice_f1(logger_, "header : \n%s", pack.debug_string() );

    this->Process(pack, &retMsg);

    ret = retMsg.sendBy( sock_ );
    if( ret ){
        poco_warning_f2( logger_, "pack.sendBy error ! ret : %d, remote addr : %s.", 
                ret, sock_.peerAddress().toString() );
        goto err;
    }

err:
    delete this;
    return;
}

void TrackerConnectionHandler::onShutdown(const AutoPtr<ShutdownNotification>& pNotification)
{
    poco_information_f1(logger_, "shutdown connection from %s.", sock_.peerAddress().toString());
    delete this;
}

void TrackerConnectionHandler::Process(const NetPack& in, NetPack* out){
    SharedPtr<Message> m = in.message();
    MessageReply mr;
    QueryPeerReply qpr;

    switch( in.payloadtype() ){
        case PAYLOAD_LOGIN: 
            this->HandleLogin(m, &mr);
             break;
        case PAYLOAD_LOGOUT: 
            this->HandleLogOut(m, &mr);
             break;
        case PAYLOAD_REQUEST_PEER:
             this->HandleRequestPeer(m, &qpr);
             break;
        case PAYLOAD_REPORT_PROGRESS:
            this->HandleReportProgress(m, &mr);
             break;
        case PAYLOAD_PUBLISH_RESOURCE:
            this->HandlePublishResource(m, &mr);
             break;
        default:
             poco_warning_f2(logger_, "Unknown PayloadType : %d , remote addr : %s.", in.payloadtype(), sock_.peerAddress().toString() );
             break;
    }

    if( in.payloadtype() != PAYLOAD_REQUEST_PEER ){
        out->set_message( PAYLOAD_MESSAGE_REPLY, mr );
    }else{
        out->set_message( PAYLOAD_REQUEST_PEER_REPLAY, qpr);
    }
}

retcode_t TrackerConnectionHandler::HandleLogin(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<Login> loginProto = in.cast<Login>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode(ERROR_UNKNOWN);

    if( loginProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    string ip = loginProto->has_loginip() ? loginProto->loginip() : sock_.peerAddress().toString();
    Tracker::ClientPtr client( new ClientInfo(loginProto->clientid(), ip, loginProto->messageport()) );
    ret = app_.AddOnlineUser(loginProto->clientid(), client);

    if( ret != ERROR_OK ){
        poco_warning_f3(logger_,"Cannot add online user, client id : %s, client ip : %s, retCode : %d.", 
                loginProto->clientid(), ip, ret);
        return ret;
    }

    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleLogOut(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<Logout> logoutProto = in.cast<Logout>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode(ERROR_UNKNOWN);

    if( logoutProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }
    ret = app_.RemoveOnlineUser(logoutProto->clientid());
    if( ret != ERROR_OK ){
        poco_warning_f3(logger_,"Cannot remove online user, client id : %s, client ip : %s, retCode : %d.", 
                logoutProto->clientid(), sock_.peerAddress().toString(), ret);
        return ret;
    }
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleRequestPeer(SharedPtr<Message> in, QueryPeerReply* out){
SharedPtr<QueryPeer> queryProto = in.cast<QueryPeer>();
    retcode_t ret = ERROR_UNKNOWN;

    if( queryProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    out->set_fileid( queryProto->fileid() );

    Tracker::ClientIdCollection idCollection( queryProto->ownedclients().begin(), queryProto->ownedclients().end() );
    Tracker::ClientFileInfoCollection fileInfoCollection;

    ret = app_.RequestClients(queryProto->fileid(), queryProto->percentage(), queryProto->needcount(), 
            idCollection, &fileInfoCollection); 
    if( ret != ERROR_OK ){
        out->set_count(-1);
        poco_warning_f4(logger_,"Cannot request clients, file id : %s, percentage : %d, need count : %d, client ip : %s", 
                queryProto->fileid(), queryProto->percentage(), queryProto->needcount(), sock_.peerAddress().toString());
        return ret;
    }

    out->set_count( fileInfoCollection.size() );
    for(int i = 0; i != fileInfoCollection.size(); ++i){
        std::pair<int, Tracker::ClientPtr>& v = fileInfoCollection[i];
        PeerFileInfo* info = out->add_info();
        Peer* p = info->mutable_client();
        p->set_clientid( v.second->clientid() );
        p->set_ip( v.second->ip() );
        p->set_messageport( v.second->messageport() );
        info->set_percentage( v.first );
    }

    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleReportProgress(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<ReportProgress> reportProto = in.cast<ReportProgress>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode( ERROR_UNKNOWN );

    if( reportProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    ret = app_.ReportProgress(reportProto->fileid(), reportProto->clientid(), reportProto->percentage());
    if( ret != ERROR_OK ){
        return ret;
    }

    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandlePublishResource(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<PublishResource> publishProto = in.cast<PublishResource>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode( ERROR_UNKNOWN );

    if( publishProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    ret = app_.PublishResource( publishProto->clientid(),  publishProto->fileid() );
    if( ret != ERROR_OK ){
        return ret;
    }

    out->set_returncode( ERROR_OK );
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::ParseProto(const string& name, Message* &proto){
    using namespace google::protobuf;
    proto = NULL;
    const Descriptor* descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(name);
    if (descriptor)
    {
        const Message* prototype = MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
          proto = prototype->New();
        }
    }
    return ERROR_OK;
}
