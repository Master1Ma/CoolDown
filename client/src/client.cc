#include "client.h"
#include "payload_type.h"
#include "netpack.h"
#include "tracker.pb.h"
#include "torrent.pb.h"
#include "verification.h"
#include <fstream>
#include <Poco/Logger.h>
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>
#include <Poco/Environment.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>

using std::ofstream;
using Poco::Logger;
using Poco::Exception;
using Poco::Util::Application;
using Poco::LocalDateTime;
using namespace TrackerProto;

namespace CoolDown{
    namespace Client{
            CoolClient::CoolClient(int argc, char* argv[])
            :Application(argc, argv){
            }

            void CoolClient::initialize(Application& self){
                loadConfiguration();
                Application::initialize(self);

                setLogger(Logger::get("ConsoleLogger"));
                poco_debug(logger(), "test ConsoleLogger in initialize.");
                this->init_error_ = false;
                this->clientid_ = Poco::Environment::nodeId();
                string msg;
                try{
                    sockManager_.assign( new LocalSockManager );
                    if( sockManager_.isNull() ){
                        msg = "Cannot create LocalSockManager.";
                        throw Exception(msg);
                    }
                }
                catch(Exception& e){
                    poco_error_f1(logger(), "Got exception in initialize : %s.", msg);
                    this->init_error_ = true;
                }
            }

            void CoolClient::uninitialize(){
                Application::uninitialize();
            }

            int CoolClient::main(const vector<string>& args){
                if( this->init_error_ ){
                    return Application::EXIT_TEMPFAIL;
                }
                if( args.size() != 2 ){
                    return Application::EXIT_USAGE;
                }
                retcode_t ret = this->make_torrent(args[0], args[1],
                        1 << 20, 0, "localhost:9977");
                poco_notice_f1(logger(), "make_torrent retcode : %d", (int)ret);
                /*
                string tracker_address("localhost");
                string fileid("1234567890");
                if( ERROR_OK == this->login_tracker(tracker_address) ){
                    this->publish_resource_to_tracker(tracker_address, fileid);
                    this->report_progress(tracker_address, fileid, 25);
                    ClientIdCollection c;
                    this->request_clients(tracker_address, fileid, 20, 90, c);
                }
                */
                return Application::EXIT_OK;
            }
            NetTaskManager& CoolClient::download_manager(){
                return this->downloadManager_;
            }
            NetTaskManager& CoolClient::upload_manager(){
                return this->uploadManager_;
            }

            retcode_t CoolClient::login_tracker(const string& tracker_address, int port){
                retcode_t ret = sockManager_->connect_tracker(tracker_address, port);
                if( ret != ERROR_OK ){
                    poco_warning_f3(logger(), "Cannot connect tracker, ret : %hd, addr : %s, port : %d.", ret, tracker_address, port);
                    return ret;
                }
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock( tracker_address ) );
                Login msg;
                msg.set_clientid( this->clientid() );
                SharedPtr<MessageReply> r;

                ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_LOGIN, &r);
                return ret;
            }

            retcode_t CoolClient::logout_tracker(const string& tracker_address, int port){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                if( sock.isNull() ){
                    return ERROR_NET_CONNECT;
                }
                Logout msg;
                msg.set_clientid( this->clientid() );
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_LOGOUT, &r);
                return ret;
            }

            retcode_t CoolClient::publish_resource_to_tracker(const string& tracker_address, const string& fileid){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                if( sock.isNull() ){
                    return ERROR_NET_CONNECT;
                }
                PublishResource msg;
                msg.set_clientid(this->clientid());
                msg.set_fileid(fileid);
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_PUBLISH_RESOURCE, &r);
                return ret;

            }

            retcode_t CoolClient::report_progress(const string& tracker_address, const string& fileid, int percentage){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                if( sock.isNull() ){
                    return ERROR_NET_CONNECT;
                }
                ReportProgress msg;
                msg.set_clientid( this->clientid() );
                msg.set_fileid( fileid );
                msg.set_percentage( percentage );
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>(sock, msg, PAYLOAD_REPORT_PROGRESS, &r);
                return ret;
            }

            retcode_t CoolClient::request_clients(const string& tracker_address, const string& fileid, int currentPercentage, 
                int needCount, const ClientIdCollection& clientids){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address ));
                if( sock.isNull() ){
                    return ERROR_NET_CONNECT;
                }
                QueryPeer msg;
                msg.set_fileid(fileid);
                msg.set_percentage(currentPercentage);
                msg.set_needcount(needCount);
                ClientIdCollection::const_iterator iter = clientids.begin();
                while( iter != clientids.end() ){
                    msg.add_ownedclients(*iter);
                    ++iter;
                }
                SharedPtr<QueryPeerReply> r;
                retcode_t ret = handle_reply_message< QueryPeerReply >( sock, msg, PAYLOAD_REQUEST_PEER, &r);
                poco_notice_f1(logger(), "Recv %d clients by QueryPeer.", r->info().size());
                return ret;
            }

            template<typename ReplyMessageType>
            retcode_t CoolClient::handle_reply_message(LocalSockManager::SockPtr& sock, 
                    const Message& msg, int payload_type, SharedPtr<ReplyMessageType>* out){

                    NetPack req(payload_type, msg);
                    poco_information_f1(logger(), "pack to send : \n%s", req.debug_string());
                    retcode_t ret = req.sendBy(*sock);
                    string error_msg("Unknown");
                    if( ret != ERROR_OK ){
                        error_msg = format("cannot send request, payload type=%d", payload_type);
                        goto err;
                    }
                    ret = req.receiveFrom(*sock);
                    if( ret != ERROR_OK){
                        error_msg = "Cannot receive comfirm message from tracker";
                        goto err;
                    }

                    *out = req.message().cast<ReplyMessageType>();
                    if( out->isNull() ){
                        error_msg = "Cannot cast retrun message to certain type";
                        goto err;
                    }

                    if( (*out)->returncode() != ERROR_OK ){
                        error_msg = "Invalid return code from tracker";
                        goto err;
                    }
                    return ERROR_OK;
    err:
                    poco_warning_f3(logger(), "in handle_reply_message: %s, ret:%d, remote addr:%s", 
                            error_msg, ret, sock->peerAddress().toString());
                    return ret;
                
            }
            retcode_t CoolClient::make_torrent(const Path& path, const Path& torrent_file_path, 
                    Int32 chunk_size, Int32 type, const string& tracker_address){

                string pathmsg( format("Call make_torrent with path : %s, torrent_file_path : %s", 
                                    path.toString(), torrent_file_path.toString()
                                )
                            );
                poco_notice_f4(logger(), "%s, chunk_size : %d, type : %d, tracker_address : %s", 
                        pathmsg, chunk_size, type, tracker_address);

                File f(path);
                if( !f.exists() ){
                    return ERROR_FILE_NOT_EXISTS;
                }

                //fill torrent info
                Torrent::Torrent torrent;
                torrent.set_type(type);
                torrent.set_createby(clientid());
                torrent.set_createtime(current_time());
                torrent.set_trackeraddress( tracker_address );
                Int64 total_size = 0;

                FileList files;
                if( f.isFile() ){
                    files.push_back(f);
                }else{
                    list_dir_recursive(f, &files);
                }
                FileList::iterator iter = files.begin();
                FileList::iterator end = files.end();

                while( iter != end ){
                    //Process one File
                    Path p(iter->path());
                    string file_check_sum = Verification::get_file_verification_code( iter->path() );
                    Int64 file_size = iter->getSize();
                    total_size += file_size;
                    Verification::ChecksumList checksums;
                    Verification::get_file_checksum_list(*iter, chunk_size, &checksums);
                    int last_chunk_size = file_size % chunk_size;

                    //file file info in Torrent
                    Torrent::File* aFile = torrent.add_file();
                    aFile->set_relativepath( p.parent().toString() );
                    aFile->set_filename( p.getFileName() );
                    aFile->set_size( file_size );
                    aFile->set_checksum( file_check_sum );
                    Verification::ChecksumList::iterator checksum_iter = checksums.begin();

                    //Process chunks in a File
                    while( checksum_iter != checksums.end() ){
                        int this_chunk_size = ( checksum_iter == checksums.end() -1 ) ? last_chunk_size : chunk_size;
                        //fill chunk info in File
                        Torrent::Chunk* aChunk = aFile->add_chunk();
                        aChunk->set_checksum( *checksum_iter );
                        aChunk->set_size( this_chunk_size );
                        ++checksum_iter;
                    }

                    ++iter;
                    //call make_torrent_progress_callback_ here
                }

                torrent.set_totalsize( total_size );

                ofstream ofs( torrent_file_path.toString().c_str() );
                if( !ofs ){
                    return ERROR_FILE_CANNOT_CREATE;
                }
                poco_assert( torrent.SerializeToOstream(&ofs) );
                ofs.close();

                return ERROR_OK;
            }

            string CoolClient::current_time() const{
                LocalDateTime now;
                return Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::HTTP_FORMAT);
            }

            string CoolClient::clientid() const{
                return this->clientid_;
            }

            void CoolClient::set_make_torrent_progress_callback(make_torrent_progress_callback_t callback){
                this->make_torrent_progress_callback_ = callback;
            }

            void CoolClient::list_dir_recursive(const File& file, FileList* pList){
                FileList files;
                file.list(files);
                for(int i = 0; i != files.size(); ++i){
                    File& file = files[i];
                    if( file.canRead() == false){
                        continue;
                    }
                    else if( file.isFile() ){
                        pList->push_back(file);
                    }
                    else if( file.isDirectory() ){
                        list_dir_recursive(file, pList);
                    }else{
                        //drop other files
                    }
                }
            }

    }
}
