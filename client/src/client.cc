#include "client.h"
#include "job.h"
#include "payload_type.h"
#include "netpack.h"
#include "tracker.pb.h"
#include "torrent.pb.h"
#include "verification.h"
#include "client.pb.h"
#include "client_connection_handler.h"
#include "job_info_collector.h"

#include <set>
#include <algorithm>
#include <fstream>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <Poco/Logger.h>
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>
#include <Poco/Environment.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>
#include <Poco/Bugcheck.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/SocketAcceptor.h>
#include <Poco/Thread.h>


using std::set;
using std::find;
using std::ifstream;
using std::ofstream;
using Poco::Logger;
using Poco::Exception;
using Poco::Util::Application;
using Poco::LocalDateTime;
using Poco::Net::ServerSocket;
using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;
using Poco::Thread;
using namespace TrackerProto;
using namespace ClientProto;

namespace CoolDown{
    namespace Client{

            namespace {
                void sock_guard(LocalSockManager::LocalSockManagerPtr& sockManager, const string& clientid, SockPtr* sock){
                    sockManager->return_sock(clientid, *sock);
                }

                //bool find_info_by_relative_path_and_name(const TorrentFileInfoPtr& info, const string& relative_path, const string& filename){
                //    return info->relative_path() == relative_path && info->filename() == filename;
                //}
            }

            CoolClient::CoolClient()
            :jobThreads_("JobThreadPool"),
            uploadManager_(logger()){
            }

            void CoolClient::initialize(Application& self){
                loadConfiguration();
                ServerApplication::initialize(self);

                setLogger(Logger::get("ConsoleLogger"));

                LOCAL_PORT = (unsigned short) config().getInt("client.message_port", 9025);
                this->init_error_ = false;
                this->clientid_ = Verification::get_verification_code( Poco::Environment::nodeId() );
                string msg;
                try{
                    job_index_ = 0;
                    sockManager_.assign( new LocalSockManager );
                    if( sockManager_.isNull() ){
                        msg = "Cannot create LocalSockManager.";
                        throw Exception(msg);
                    }
                }
                catch(Exception& e){
                    poco_error_f1(logger(), "Got exception in initialize : %s", msg);
                    this->init_error_ = true;
                }
            }

            void CoolClient::uninitialize(){
                ServerApplication::uninitialize();
            }

            int CoolClient::main(const vector<string>& args){
                if( this->init_error_ ){
                    return Application::EXIT_TEMPFAIL;
                }

                string tracker_ip("127.0.0.1");
                string tracker_address( format("%s:%d", tracker_ip, (int)CoolClient::TRACKER_PORT) );
                poco_debug_f1(logger(), "run application with %d", (int)args.size());
                Thread job_info_collector_thread;
                job_info_collector_thread.start( *(new JobInfoCollector) );

                //Timer info_collector_timer(0, 1000);
                //info_collector_timer.start(TimerCallback<CoolClient>(*this, &CoolClient::onJobInfoCollectorWakeUp));

                if( args.size() == 1 ){

                    this->clientid_ = "0123456789012345678901234567890123456789";
                    if( ERROR_OK != this->login_tracker(tracker_ip, CoolClient::TRACKER_PORT) ){
                        poco_warning_f1(logger(), "cannot login tracker : %s", tracker_address);
                    }

                    int handle = -1;
                    Torrent::Torrent torrent;
                    retcode_t ret = this->parse_torrent( args[0], &torrent );
                    poco_debug_f1(logger(), "parse_torrent retcode : %d", (int)ret);

                    ret = this->add_job(torrent, "/tmp/", &handle);
                    poco_debug_f1(logger(), "add_job retcode : %d", (int)ret);
                    if( ret != ERROR_OK ){
                    }else{
                        ret = this->start_job(handle);
                        poco_debug_f1(logger(), "start_job retcode : %d", (int)ret);
                        if( ret != ERROR_OK ){
                        }else{
                            jobThreads_.joinAll();
                            poco_trace(logger(), "all jobThreads_ return.");
                        }
                    }
                    this->logout_tracker(tracker_ip, TRACKER_PORT);

                }else if( args.size() == 2 ){
                    LOCAL_PORT = 9024;
                    if( ERROR_OK != this->login_tracker(tracker_ip, CoolClient::TRACKER_PORT) ){
                        poco_warning_f1(logger(), "cannot login tracker : %s", tracker_address);
                    }

                    retcode_t ret = this->make_torrent(args[0], args[1],
                            1 << 20, 0, tracker_address);
                    poco_debug_f1(logger(), "make_torrent retcode : %d", (int)ret);

                    if( ret != ERROR_OK){
                        return Application::EXIT_TEMPFAIL;
                    }else{
                        Torrent::Torrent torrent;
                        retcode_t ret = this->parse_torrent( args[1], &torrent );
                        poco_debug_f1(logger(), "parse_torrent retcode : %d", (int)ret);

                        for(int i = 0; i != torrent.file().size(); ++i){
                            string fileid( torrent.file().Get(i).checksum() );
                            retcode_t publish_ret = this->publish_resource_to_tracker(tracker_address, fileid );
                            poco_debug_f2(logger(), "publish resource '%s' return code %d.", fileid, (int)publish_ret);
                        }

                        int handle = -1;

                        ret = this->add_job(torrent, "/", &handle);
                        poco_debug_f1(logger(), "add_job retcode : %d", (int)ret);
                        JobPtr pJob = this->get_job(handle);
                        poco_assert( pJob.isNull() == false );
                        set<string> same_fileid;
                        for(int pos = 0; pos != torrent.file().size(); ++pos){
                            const Torrent::File& file = torrent.file().Get(pos);
                            string fileid( file.checksum() );
                            string relative_path( file.relativepath() );
                            string filename( file.filename() );
                            Int64 filesize( file.size() );

                            retcode_t ret = pJob->MutableJobInfo()->localFileInfo.add_file(fileid,
                                                                                           relative_path,
                                                                                           filename,
                                                                                           filesize);
                            poco_debug_f2(logger(), "add '%s' to localFileInfo returns %d", filename, (int)ret);

                            if( same_fileid.find(fileid) != same_fileid.end() ){
                                continue;
                            }else{
                                same_fileid.insert(fileid);
                                pJob->MutableJobInfo()->downloadInfo.bitmap_map[fileid]->flip();
                            }

                        }

                        poco_debug_f1(logger(), "client listen on port : %d", LOCAL_PORT);
                        ServerSocket svs(LOCAL_PORT);
                        svs.setReusePort(false);

                        SocketReactor reactor;
                        SocketAcceptor<ClientConnectionHandler> acceptor(svs, reactor);


                        Thread thread;
                        thread.start(reactor);

                        waitForTerminationRequest();

                        reactor.stop();
                        thread.join();
                        this->logout_tracker(tracker_ip, TRACKER_PORT);
                    }
                }else{
                    poco_notice(logger(), "Usage : \n"
                            "(1)Download File : Client TorrentFile\n" 
                            "(2)Publish File and upload : Client LocalFile TorrentFile");
                }





                return Application::EXIT_OK;
            }

            //NetTaskManager& CoolClient::download_manager(){
            //    return this->downloadManager_;
            //}

            NetTaskManager& CoolClient::upload_manager(){
                return this->uploadManager_;
            }

            retcode_t CoolClient::login_tracker(const string& tracker_address, int port){
                retcode_t ret = sockManager_->connect_tracker(tracker_address, port);
                if( ret != ERROR_OK ){
                    poco_warning_f3(logger(), "Cannot connect tracker, ret : %hd, addr : %s, port : %d.", ret, tracker_address, port);
                    return ret;
                }
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock( format("%s:%d", tracker_address, port)) );
                poco_assert( sock.isNull() == false );

                Login msg;
                msg.set_clientid( this->clientid() );
                msg.set_messageport( LOCAL_PORT );
                SharedPtr<MessageReply> r;

                ret = handle_reply_message<MessageReply>( sock, msg, PAYLOAD_LOGIN, &r);
                return ret;
            }

            retcode_t CoolClient::logout_tracker(const string& tracker_address, int port){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock( format("%s:%d", tracker_address, port)) );
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
                    return ERROR_NET_NOT_CONNECTED;
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
                int needCount, const ClientIdCollection& clientids, FileOwnerInfoPtrList* pInfoList){
                poco_assert( pInfoList != NULL );
                poco_assert( currentPercentage >= 0 );
                poco_assert( needCount > 0 );

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
                poco_notice_f2(logger(), "Recv %d clients by QueryPeer. fileid = '%s'", r->info().size(), fileid);
                if( ret != ERROR_OK || r->returncode() != ERROR_OK ){
                    poco_warning_f2(logger(), "CoolClient::request_clients error. func ret : %d, request ret : %d",
                            (int)ret, (int)r->returncode() );
                    return ERROR_UNKNOWN;
                }

                int peer_count = r->info().size();
                for( int pos = 0; pos != peer_count; ++pos ){
                    const PeerFileInfo& peer = r->info().Get(pos);
                    pInfoList->push_back( new FileOwnerInfo(
                                peer.client().clientid(), 
                                peer.client().ip(), 
                                peer.client().messageport(),
                                peer.percentage()) );
                }
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
                        error_msg = format("Invalid message return code %d from tracker", int((*out)->returncode()) );
                        goto err;
                    }
                    return ERROR_OK;
    err:
                    poco_warning_f3(logger(), "in handle_reply_message: %s, ret:%d, remote addr:%s", 
                            error_msg, (int)ret, sock->peerAddress().toString());
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

            //communicate with client
            retcode_t CoolClient::shake_hand(const ShakeHand& self, ShakeHand& peer){
                string peer_clientid( peer.clientid() );
                poco_trace_f1(logger(), "assert if connect to '%s'", peer_clientid);
                poco_assert( sockManager_->is_connected(peer_clientid) );
                poco_trace_f2(logger(), "pass assert at file : %s, line : %d", string(__FILE__), __LINE__ - 1);

                LocalSockManager::SockPtr sock( sockManager_->get_idle_client_sock(peer_clientid) );
                poco_assert( sock.isNull() == false );
                poco_trace_f2(logger(), "pass assert at file : %s, line : %d", string(__FILE__), __LINE__ - 1);

                //use guard to return_sock automatically 
                typedef shared_ptr<LocalSockManager::SockPtr> SockGuard;
                SockGuard guard(&sock, boost::bind(sock_guard, sockManager_, peer_clientid, _1) );

                NetPack req( PAYLOAD_SHAKE_HAND, self );
                retcode_t ret = req.sendBy( *sock );
                if( ret != ERROR_OK ){
                    poco_warning(logger(), "send shake_hand error.");
                    return ret;
                }
                NetPack res;
                ret = res.receiveFrom( *sock );
                if( ret != ERROR_OK ){
                    poco_warning(logger(), "receive shake_hand error.");
                    return ret;
                }

                peer = *(res.message().cast<ShakeHand>());
                return ERROR_OK;
            }

            retcode_t CoolClient::add_job(const Torrent::Torrent& torrent, const string& top_path, int* internal_handle){
                SharedPtr<JobInfo> info( new JobInfo( torrent, top_path ) );
                int this_job_index = job_index_;
                jobs_[job_index_] = JobPtr( new Job(info, *(this->sockManager_), logger()) );
                ++job_index_;
                *internal_handle = this_job_index;
                return ERROR_OK;
            }

            retcode_t CoolClient::start_job(int handle){
                JobMap::iterator iter = jobs_.find(handle);
                if( iter == jobs_.end() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                this->resume_download(handle);
                jobThreads_.start(*(iter->second));
                return ERROR_OK;
            }

            retcode_t CoolClient::pause_download(int handle){
                JobMap::iterator iter = jobs_.find(handle);
                if( iter == jobs_.end() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                iter->second->MutableJobInfo()->downloadInfo.is_download_paused = true;
                return ERROR_OK;
            }

            retcode_t CoolClient::resume_download(int handle){
                JobMap::iterator iter = jobs_.find(handle);
                if( iter == jobs_.end() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                iter->second->MutableJobInfo()->downloadInfo.is_download_paused = false;
                return ERROR_OK;
            }

            CoolClient::JobPtr CoolClient::get_job(int handle){
                if( jobs_.find(handle) == jobs_.end() ){
                    return JobPtr();
                }else{
                    return jobs_[handle];
                }
            }

            CoolClient::JobPtr CoolClient::get_job(const string& fileid){
                BOOST_FOREACH(JobMap::value_type& p, jobs_){
                    const vector<string>& fileidlist = p.second->ConstJobInfo().fileidlist();
                    if( fileidlist.end() != find(fileidlist.begin(), fileidlist.end(), fileid) ){
                        return p.second;
                    }
                }
                return JobPtr(NULL);
            }

            void CoolClient::onJobInfoCollectorWakeUp(Timer& timer){
                BOOST_FOREACH(JobMap::value_type& p, jobs_){
                    int handle = p.first;
                    JobInfoPtr pInfo = p.second->MutableJobInfo();
                    UInt64 bytes_upload_this_second = pInfo->downloadInfo.bytes_upload_this_second;
                    UInt64 bytes_download_this_second = pInfo->downloadInfo.bytes_download_this_second;
                    string upload_speed, download_speed;
                    format_speed(bytes_upload_this_second, &upload_speed);
                    format_speed(bytes_download_this_second, &download_speed);

                    poco_notice_f3(logger(), "Job handle : %d, upload speed : %s, download speed : %s",
                            handle, upload_speed, download_speed);

                    pInfo->downloadInfo.bytes_upload_this_second = 0;
                    pInfo->downloadInfo.bytes_download_this_second = 0;
                    pInfo->downloadInfo.download_speed_limit_cond.broadcast();
                }
            }

            retcode_t CoolClient::parse_torrent(const Path& torrent_file_path, Torrent::Torrent* pTorrent){
                ifstream ifs(torrent_file_path.toString().c_str() );
                poco_assert(pTorrent != NULL);
                pTorrent->Clear();
                if( pTorrent->ParseFromIstream(&ifs) == false){
                    poco_warning_f1(logger(), "cannot parse torrent from file '%s'.", torrent_file_path.toString());
                    return ERROR_FILE_PARSE_ERROR;
                }
                return ERROR_OK;
            }

            string CoolClient::current_time() const{
                LocalDateTime now;
                return Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::HTTP_FORMAT);
            }

            void CoolClient::format_speed(UInt64 speed, string* formatted_speed){
                if( speed > (1 << 20) ){
                    // MB level
                    format(*formatted_speed, "%.2f MB/s", (double)speed / (1 << 20) );
                }else if ( speed > (1 << 10) ){
                    format(*formatted_speed, "%.2f KB/s", (double)speed / (1 << 10) );
                }else{
                    format(*formatted_speed, "%Lu B/s", speed);
                }
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
