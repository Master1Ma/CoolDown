#include "client.h"
#include "job.h"
#include "payload_type.h"
#include "netpack.h"
#include "verification.h"
#include "client_connection_handler.h"
#include "job_info_collector.h"
#include "tracker.pb.h"
#include "torrent.pb.h"
#include "client.pb.h"
#include "job_history.pb.h"

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
#include <Poco/RunnableAdapter.h>


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
using Poco::RunnableAdapter;
using namespace TrackerProto;
using namespace ClientProto;
using namespace JobHistory;

namespace CoolDown{
    namespace Client{

            namespace {
                void sock_guard(LocalSockManager::LocalSockManagerPtr& sockManager, const string& clientid, SockPtr* sock){
                    sockManager->return_sock(clientid, *sock);
                }
            }

            CoolClient::CoolClient()
            :jobThreads_(),
            uploadManager_(logger()){
            }

            void CoolClient::initialize(Application& self){
                loadConfiguration();
                ServerApplication::initialize(self);

				Logger& logger_ = Logger::get("ConsoleLogger");
                setLogger(logger_);
                LOCAL_PORT = (unsigned short) config().getInt("client.message_port", 9025);
                this->clientid_ = Verification::get_verification_code( Poco::Environment::nodeId() );
                string default_history_file_path = format("%s%c%s", Path::current(), Path::separator(), string("history") );
                this->history_file_path_ = config().getString("client.history_path", default_history_file_path);
                this->exiting_ = false;
                this->init_error_ = false;
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
                exiting_ = true;
                this->SaveJobHistory( history_file_path_ );
                while( this->job_info_collector_thread_.tryJoin( 1000 ) == false ){
                    this->jobInfoCollectorTerminateCond_.signal();
                }
                while( this->report_progress_thread_.tryJoin( 1000 ) == false ){
                    this->reportProgressCond_.signal();
                }
                ServerApplication::uninitialize();
            }

            int CoolClient::main(const vector<string>& args){
                if( this->init_error_ ){
                    return Application::EXIT_TEMPFAIL;
                }

                string tracker_ip("127.0.0.1");
                string tracker_address( format("%s:%d", tracker_ip, (int)CoolClient::TRACKER_PORT) );
                poco_debug_f1(logger(), "run application with %d", (int)args.size());
                this->job_info_collector_thread_.setOSPriority( Thread::getMaxOSPriority() );
                this->job_info_collector_thread_.start( *(new JobInfoCollector) );

                Poco::RunnableAdapter<CoolClient> reportProgressRunnable( *this, &CoolClient::ReportProgressRoutine );
                this->report_progress_thread_.start( reportProgressRunnable );

                if( args.size() == 1 ){
                    this->history_file_path_ = "/tmp/download.history";
                    this->clientid_ = "0123456789012345678901234567890123456789";
                    if( ERROR_OK != this->LoginTracker(tracker_ip, CoolClient::TRACKER_PORT) ){
                        poco_warning_f1(logger(), "cannot login tracker : %s", tracker_address);
                        return Application::EXIT_TEMPFAIL;
                    }

                    this->ReloadJobHistory(this->history_file_path_);

                    int handle = -1;
                    Torrent::Torrent torrent;
                    retcode_t ret = this->ParseTorrent(args[0], &torrent);
                    if( ret != ERROR_OK ){
                        return ret;
                    }
                    if( this->HasThisTorrent(torrent.torrentid()) ){

                    }else{
                        FileIdentityInfoList needs;
                        for(int i = 0; i != torrent.file().size(); ++i){
                            const Torrent::File& file = torrent.file().Get(i);
                            needs.push_back(FileIdentityInfoList::value_type(file.relativepath(), file.filename()) );
                        }

                        ret = this->AddNewDownloadJob( args[0], torrent, needs, "/tmp/", &handle);
                        poco_debug_f1(logger(), "AddNewJob retcode : %d", (int)ret);
                        if( ret != ERROR_OK ){

                        }else{
                            ret = this->StartJob(handle);
                            poco_debug_f1(logger(), "start_job retcode : %d", (int)ret);
                            if( ret != ERROR_OK ){

                            }else{
                                //Thread::sleep(2);
                                //this->RemoveJob(handle);
                                jobThreads_.joinAll();
                                poco_trace(logger(), "all jobThreads_ return.");
                            }
                        }
                    }
                    this->LogoutTracker(tracker_ip, TRACKER_PORT);

                }else if( args.size() == 2 ){
                    LOCAL_PORT = 9024;
                    this->history_file_path_ = "/tmp/upload.history";
                    if( ERROR_OK != this->LoginTracker(tracker_ip, CoolClient::TRACKER_PORT) ){
                        poco_warning_f1(logger(), "cannot login tracker : %s", tracker_address);
                    }

                    retcode_t ret = this->ReloadJobHistory(history_file_path_);
                    poco_debug_f1(logger(), "ReloadJobHistory return : %d", (int)ret);

                    string resource_path( args[0] );
                    Path parent( Path(resource_path).parent().toString() );
                    string top_path( parent.toString() );
                    string torrent_path( args[1] );

                    ret = this->MakeTorrent(resource_path, torrent_path, 1 << 20, 1, tracker_address);
                    poco_debug_f1(logger(), "MakeTorrent return : %d", (int)ret);

                    Torrent::Torrent torrent;
                    ret = this->ParseTorrent(torrent_path, &torrent);
                    poco_debug_f1(logger(), "ParseTorrent returns %d", (int)ret);
                    if( ERROR_OK != ret ){
                        return Application::EXIT_TEMPFAIL;
                    }

                    int handle;
                    ret = this->AddNewUploadJob( torrent_path, top_path, torrent, &handle);
                    poco_debug_f1(logger(), "AddNewUploadJob returns %d", (int)ret);

                    if( ret == ERROR_OK ){
                        ret = this->ResumeJob(handle);
                        poco_debug_f1(logger(), "ResumeJob returns %d", (int)ret);
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
                    this->LogoutTracker(tracker_ip, TRACKER_PORT);
                }else{
                    //poco_notice(logger(), "Usage : \n"
                    //        "(1)Download File : Client TorrentFile\n" 
                    //        "(2)Publish File and upload : Client LocalFile TorrentFile");
                }


                return Application::EXIT_OK;
            }

            NetTaskManager& CoolClient::upload_manager(){
                return this->uploadManager_;
            }

            retcode_t CoolClient::LoginTracker(const string& tracker_address, int port){
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

            retcode_t CoolClient::LogoutTracker(const string& tracker_address, int port){
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

            retcode_t CoolClient::PublishResourceToTracker(const string& tracker_address, const string& fileid){
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

            retcode_t CoolClient::ReportProgress(const string& tracker_address, const string& fileid, int percentage){
                LocalSockManager::SockPtr sock( sockManager_->get_tracker_sock(tracker_address) );
                if( sock.isNull() ){
                    return ERROR_NET_CONNECT;
                }
                TrackerProto::ReportProgress msg;
                msg.set_clientid( this->clientid() );
                msg.set_fileid( fileid );
                msg.set_percentage( percentage );
                SharedPtr<MessageReply> r;
                retcode_t ret = handle_reply_message<MessageReply>(sock, msg, PAYLOAD_REPORT_PROGRESS, &r);
                return ret;
            }

            retcode_t CoolClient::RequestClients(const string& tracker_address, const string& fileid, int currentPercentage, 
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
                    poco_warning_f2(logger(), "CoolClient::RequestClients error. func ret : %d, request ret : %d",
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
            retcode_t CoolClient::MakeTorrent(const Path& path, const Path& torrent_file_path, 
                    Int32 chunk_size, Int32 type, const string& tracker_address){

                string pathmsg( format("Call MakeTorrent with path : %s, torrent_file_path : %s", 
                                    path.toString(), torrent_file_path.toString()
                                )
                            );
                poco_notice_f4(logger(), "%s, chunk_size : %d, type : %d, tracker_address : %s", 
                        pathmsg, chunk_size, type, tracker_address);

                File f(path);
                string top_path = Path(f.path()).parent().toString();
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


                string torrent_id_source;
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
                    string abs_path( p.parent().toString() );
                    pair<string::iterator, string::iterator> pa = mismatch(top_path.begin(), top_path.end(), abs_path.begin() );
                    string relative_path( pa.second, abs_path.end() );
                    aFile->set_relativepath( relative_path );
                    aFile->set_filename( p.getFileName() );
                    aFile->set_size( file_size );
                    aFile->set_checksum( file_check_sum );

                    torrent_id_source.append( p.parent().toString() ).append( p.getFileName() ).append( file_check_sum );

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
                string torrent_id = Verification::get_verification_code( torrent_id_source );
                torrent.set_totalsize( total_size );
                torrent.set_torrentid( torrent_id );

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

            //retcode_t CoolClient::add_job(const Torrent::Torrent& torrent, const string& top_path, int* internal_handle){
            //    FileIdentityInfoList needs;
            //    for(int i = 0; i != torrent.file().size(); ++i){
            //        const Torrent::File& file = torrent.file().Get(i);
            //        needs.push_back(FileIdentityInfoList::value_type(file.relativepath(), file.filename()) );
            //    }
            //    SharedPtr<JobInfo> info( new JobInfo( torrent, top_path, needs ) );
            //    int this_job_index = job_index_;
            //    FastMutex::ScopedLock lock(mutex_);
            //    jobs_[job_index_] = JobPtr( new Job(info, *(this->sockManager_), logger()) );
            //    ++job_index_;
            //    *internal_handle = this_job_index;
            //    return ERROR_OK;
            //}

            retcode_t CoolClient::StartJob(int handle){
                FastMutex::ScopedLock lock(mutex_);
                retcode_t ret = this->ResumeJobWithoutLock(handle);
                if( ret != ERROR_OK ){
                    return ret;
                }
                JobPtr pJob = this->GetJobWithoutLock(handle);
                poco_assert( pJob.isNull() == false );
                if( pJob->is_running() ){
                    return ERROR_JOB_ALREADY_STARTED;
                }
                jobThreads_.start(*pJob);
                return ERROR_OK;
            }

            retcode_t CoolClient::StopJob(int handle){
                FastMutex::ScopedLock lock(mutex_);
                JobPtr pJob = this->GetJobWithoutLock(handle);
                if( pJob.isNull() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                pJob->MutableJobInfo()->downloadInfo.is_stopped = true;
                return ERROR_OK;
            }

            retcode_t CoolClient::PauseJobWithoutLock(int handle){
                JobPtr pJob = this->GetJobWithoutLock(handle);
                if( pJob.isNull() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                pJob->MutableJobInfo()->downloadInfo.is_download_paused = true;
                return ERROR_OK;
            }

            retcode_t CoolClient::PauseJob(int handle){
                FastMutex::ScopedLock lock(mutex_);
                return this->PauseJobWithoutLock(handle);
            }

            retcode_t CoolClient::ResumeJob(int handle){
                FastMutex::ScopedLock lock(mutex_);
                return this->ResumeJobWithoutLock(handle);
            }

            retcode_t CoolClient::RemoveJob(int handle){
                JobPtr pJob = this->GetJobByHandle(handle);
                if( pJob.isNull() ){
                    return ERROR_JOB_NOT_EXISTS;
                }
                pJob->MutableJobInfo()->downloadInfo.is_job_removed = true;
                pJob->MutableJobInfo()->downloadInfo.download_pause_cond.broadcast();
                pJob->MutableJobInfo()->downloadInfo.download_speed_limit_cond.broadcast();
                this->jobs_.erase(handle);

                //since we never roll back the handle, we never destructor the job until application exit;
                this->removed_jobs_[handle] = pJob;
                return ERROR_OK;
            }

            retcode_t CoolClient::ResumeJobWithoutLock(int handle){
                JobPtr pJob = this->GetJobWithoutLock(handle);
                if( pJob.isNull() ){
                    return ERROR_JOB_NOT_EXISTS;
                }

                pJob->MutableJobInfo()->downloadInfo.is_stopped = false;
                pJob->MutableJobInfo()->downloadInfo.is_download_paused = false;
                pJob->MutableJobInfo()->downloadInfo.download_pause_cond.broadcast();
                pJob->MutableJobInfo()->downloadInfo.job_stopped_cond.broadcast();
                return ERROR_OK;
            }

            CoolClient::JobPtr CoolClient::GetJobWithoutLock(int handle){
                JobPtr res(NULL);
                if( jobs_.find(handle) != jobs_.end() ){
                    res = jobs_[handle];
                }
                return res;
            }

            CoolClient::JobPtr CoolClient::GetJobByHandle(int handle){
                FastMutex::ScopedLock lock(mutex_);
                return this->GetJobWithoutLock(handle);
            }

            CoolClient::JobPtr CoolClient::GetJobByFileid(const string& fileid){
                FastMutex::ScopedLock lock(mutex_);
                BOOST_FOREACH(JobMap::value_type& p, jobs_){
                    const vector<string>& fileidlist = p.second->ConstJobInfo().fileidlist();
                    if( fileidlist.end() != find(fileidlist.begin(), fileidlist.end(), fileid) ){
                        return p.second;
                    }
                }
                return JobPtr(NULL);
            }


            bool CoolClient::HasThisTorrent(const string& torrent_id){
                return torrent_ids_.end() != torrent_ids_.find( torrent_id );
            }

            void CoolClient::RegisterTorrent(const string& torrent_id){
                poco_debug_f1(logger(),"register torrent_id : %s", torrent_id);
                this->torrent_ids_.insert( torrent_id );
            }

            retcode_t CoolClient::SaveJobHistory(const string& filename){
                ofstream ofs(filename.c_str());
                if( !ofs ){
                    poco_warning_f1(logger(), "Cannot open history file : %s for write.", filename);
                    return ERROR_FILE_NOT_EXISTS;
                }

                JobHistory::History history;
                FastMutex::ScopedLock lock(mutex_);

                BOOST_FOREACH(JobMap::value_type& p, jobs_){
                    int handle = p.first;
                    JobPtr pJob = p.second;
                    torrent_path_map_t::iterator iter = torrent_path_map_.find(handle);
                    poco_assert( iter != torrent_path_map_.end() );
                    string torrent_path( iter->second );
                    string top_path( pJob->MutableJobInfo()->localFileInfo.top_path() );

                    JobHistory::JobHistoryInfo* oneJob = history.add_jobinfo();
                    oneJob->set_torrentpath( torrent_path );
                    oneJob->set_localtoppath( top_path );
                    oneJob->set_uploadtotal( pJob->MutableJobInfo()->downloadInfo.upload_total );
                    oneJob->set_downloadtotal( pJob->MutableJobInfo()->downloadInfo.download_total );

                    const TorrentInfo::file_map_t& file_map = pJob->MutableJobInfo()->torrentInfo.get_file_map();
                    BOOST_FOREACH(const TorrentInfo::file_map_t::value_type& p, file_map){
                        string fileid( p.first );
                        file_bitmap_ptr pBitmap = pJob->MutableJobInfo()->downloadInfo.bitmap_map[fileid];

                        BOOST_FOREACH(const TorrentFileInfoPtr& file_info_ptr, p.second){
                            JobHistory::FileInfo* oneFile = oneJob->add_file();
                            oneFile->set_fileid( fileid );
                            oneFile->set_relativepath( file_info_ptr->relative_path() );
                            oneFile->set_filename( file_info_ptr->filename() );
                            oneFile->set_filebitcount( pBitmap->size() );
                            to_block_range( *pBitmap, google::protobuf::RepeatedFieldBackInserter(oneFile->mutable_filebit()) );
                            //double percentage = pBitmap->count() / pBitmap->size();
                            //poco_debug_f2(logger(), "in SaveJobHistory, file '%s', percentage = '%f'", oneFile->fileid(), percentage);
                        }
                    }
                }

                if( false == history.SerializeToOstream(&ofs) ){
                    poco_warning_f1(logger(), "Cannot save job history to file : %s", filename);
                    return ERROR_PROTO_SERIALIZE_ERROR;
                }
                return ERROR_OK;
            }

            retcode_t CoolClient::ReloadJobHistory(const string& filename){
                ifstream ifs(filename.c_str());
                if( !ifs ){
                    poco_warning_f1(logger(), "Cannot load job history file : %s", filename);
                    return ERROR_FILE_NOT_EXISTS;
                }
                JobHistory::History history;
                if ( false == history.ParseFromIstream(&ifs) ){
                    poco_warning_f1(logger(), "Cannot parse JobHistoryInfo from %s", filename);
                    return ERROR_PROTO_PARSE_ERROR;
                }

                for(int i = 0; i != history.jobinfo().size(); ++i){
                    const JobHistoryInfo& oneHistory = history.jobinfo().Get(i);
                    Torrent::Torrent torrent;
                    if( ERROR_OK != this->ParseTorrent(oneHistory.torrentpath(), &torrent) ){
                        continue;
                    }
                    if( ERROR_OK != this->ReloadOneJob(torrent, oneHistory) ){
                        poco_warning(logger(), "Error while reload one job in ReloadHistoryDownload.");
                        continue;
                    }
                }
                return ERROR_OK;
            }

            retcode_t CoolClient::ReloadOneJob(const Torrent::Torrent& torrent, const JobHistoryInfo& history){
                poco_assert( this->HasThisTorrent(torrent.torrentid()) == false );
                this->RegisterTorrent( torrent.torrentid() );
                FileIdentityInfoList needs;
                for(int i = 0; i != history.file().size(); ++i){
                    const JobHistory::FileInfo& oneFile = history.file().Get(i);
                    needs.push_back( FileIdentityInfoList::value_type(oneFile.relativepath(), oneFile.filename()) );
                }

                SharedPtr<JobInfo> info( new JobInfo( torrent, history.localtoppath(), needs) );
                info->downloadInfo.upload_total = history.uploadtotal();
                info->downloadInfo.download_total = history.downloadtotal();
                for(int i = 0; i != history.file().size(); ++i){
                    const JobHistory::FileInfo& oneFile = history.file().Get(i);
                    file_bitmap_ptr& pBitmap = info->downloadInfo.bitmap_map[ oneFile.fileid() ];
                    pBitmap.assign( new file_bitmap_t( oneFile.filebit().begin(), oneFile.filebit().end() ) );
                    pBitmap->resize( oneFile.filebitcount() );
                    info->downloadInfo.percentage_map[ oneFile.fileid() ] = pBitmap->count() / pBitmap->size();
                    //double percentage = pBitmap->count() / pBitmap->size();
                    //poco_debug_f2(logger(), "in ReloadOneJob, file '%s', percentage = '%f'", oneFile.fileid(), percentage);
                }
                info->downloadInfo.is_job_removed = false;
                info->downloadInfo.is_stopped = false;
                info->downloadInfo.is_download_paused = true;


                int handle;
                retcode_t ret = this->AddNewJob(info, history.torrentpath(), &handle);
                if( ret != ERROR_OK ){
                    return ret;
                }
                return ERROR_OK;
            }

            retcode_t CoolClient::AddNewJob(const SharedPtr<JobInfo>& info, const string& torrent_path, int* handle){
                int this_job_index = job_index_;
                
                FastMutex::ScopedLock lock(mutex_);
                jobs_[job_index_] = JobPtr( new Job(info, *(this->sockManager_), logger()) );
                poco_debug_f1(logger(), "add Job to jobs_, torrent_id : %s", info->torrentInfo.torrentid());
                ++job_index_;
                *handle = this_job_index;
                torrent_path_map_[this_job_index] = torrent_path;
                return ERROR_OK;
            }

            retcode_t CoolClient::AddNewDownloadJob(const string& torrent_path, const Torrent::Torrent& torrent, 
                    const FileIdentityInfoList& needs, const string& top_path, int* handle){

                string torrent_id( torrent.torrentid() );
                if( this->HasThisTorrent(torrent_id) ){
                    return ERROR_JOB_EXISTS;
                }
                poco_debug_f1(logger(), "in  AddNewDownloadJob, pass unique check of torrent_id : %s", torrent_id);
                this->RegisterTorrent( torrent_id );

                SharedPtr<JobInfo> info( new JobInfo( torrent, top_path, needs) );
                return this->AddNewJob(info, torrent_path, handle);
            }

            retcode_t CoolClient::AddNewUploadJob(const string& torrent_path, const string& top_path, 
                    const Torrent::Torrent& torrent, int* handle){

                string torrent_id( torrent.torrentid() );
                if( this->HasThisTorrent(torrent_id) ){
                    return ERROR_JOB_EXISTS;
                }
                poco_debug_f1(logger(), "in  AddNewUploadJob, pass unique check of torrent_id : %s", torrent_id);
                this->RegisterTorrent( torrent_id );

                FileIdentityInfoList needs;
                for(int i = 0; i != torrent.file().size(); ++i){
                    const Torrent::File& file = torrent.file().Get(i);
                    needs.push_back(FileIdentityInfoList::value_type(file.relativepath(), file.filename()) );
                }
                SharedPtr<JobInfo> info( new JobInfo( torrent, top_path, needs ) );

                set<string> same_fileid;
                for(int pos = 0; pos != torrent.file().size(); ++pos){
                    const Torrent::File& file = torrent.file().Get(pos);
                    string fileid( file.checksum() );
                    if( same_fileid.find(fileid) != same_fileid.end() ){
                        continue;
                    }else{
                        same_fileid.insert(fileid);
                        info->downloadInfo.bitmap_map[fileid]->set();
                        retcode_t publish_ret = this->PublishResourceToTracker(torrent.trackeraddress(), fileid);
                        poco_debug_f2(logger(), "Publish '%s' to tracker return %d", fileid, (int)publish_ret);
                    }
                }
                return this->AddNewJob(info, torrent_path, handle);
            }

            void CoolClient::onJobInfoCollectorWakeUp(Timer& timer){
                FastMutex::ScopedLock lock(mutex_);
                progress_info_list_t progress_to_report;
                BOOST_FOREACH(JobMap::value_type& p, jobs_){
                    int handle = p.first;
                    JobInfoPtr pInfo = p.second->MutableJobInfo();
                    UInt64 bytes_upload_this_second = pInfo->downloadInfo.bytes_upload_this_second;
                    UInt64 bytes_download_this_second = pInfo->downloadInfo.bytes_download_this_second;
                    string upload_speed, download_speed;
                    format_speed(bytes_upload_this_second, &upload_speed);
                    format_speed(bytes_download_this_second, &download_speed);

                    pInfo->downloadInfo.time_to_next_report -= 1;
                    bool need_to_report;
                    if( pInfo->downloadInfo.time_to_next_report == 0 
                            && p.second->is_running() == true
                            ){

                        need_to_report = true;
                        pInfo->downloadInfo.time_to_next_report = DownloadInfo::report_period;
                    }else{
                        need_to_report = false;
                    }

                    poco_notice_f3(logger(), "Job handle : %d, upload speed : %s, download speed : %s",
                            handle, upload_speed, download_speed);

                    string tracker_address( pInfo->torrentInfo.tracker_address());
                    BOOST_FOREACH(DownloadInfo::file_bitmap_map_t::value_type& p, pInfo->downloadInfo.bitmap_map){
                        string fileid(p.first);
                        int percentage = (double)p.second->count() / p.second->size() * 100;
                        pInfo->downloadInfo.percentage_map[fileid] = percentage;
                        if( need_to_report ){
                            ProgressInfo oneInfo = { tracker_address, fileid, percentage };
                            progress_to_report.push_back( oneInfo );
                        }
                    }

                    pInfo->downloadInfo.upload_total += bytes_upload_this_second;
                    pInfo->downloadInfo.download_total += bytes_download_this_second;
                    pInfo->downloadInfo.bytes_upload_this_second = 0;
                    pInfo->downloadInfo.bytes_download_this_second = 0;
                    pInfo->downloadInfo.download_speed_limit_cond.broadcast();
                }

                if( progress_to_report.size() != 0 ){
                    {
                        FastMutex::ScopedLock lock(this->progress_info_mutex_);
                        this->progress_info_to_report_.swap(progress_to_report);
                    }
                    this->reportProgressCond_.signal();

                }
            }

            void CoolClient::ReportProgressRoutine(){
                while(1){
                    if( this->progress_info_to_report_.size() == 0 && exiting_ == false){
                        FastMutex mutex;
                        this->reportProgressCond_.wait(mutex);
                    }else if( exiting_ == true ){
                        break;
                    }else{
                        progress_info_list_t progress_info_to_report;
                        {
                            FastMutex::ScopedLock lock(this->progress_info_mutex_);
                            progress_info_to_report.swap(this->progress_info_to_report_);
                        }
                        BOOST_FOREACH(const progress_info_list_t::value_type& oneInfo, progress_info_to_report){
                            retcode_t ret = this->ReportProgress(oneInfo.tracker_address, oneInfo.fileid, oneInfo.percentage);
                            poco_debug_f3(logger(), "Report progress of file '%s', percentage '%d', return %d",
                                    oneInfo.fileid, oneInfo.percentage, (int)ret);
                        }

                    }
                }
            }

            retcode_t CoolClient::ParseTorrent(const Path& torrent_file_path, Torrent::Torrent* pTorrent){
                ifstream ifs(torrent_file_path.toString().c_str() );
                if( !ifs ){
                    poco_debug_f1(logger(), "Cannot open %s in ParseTorrent.", torrent_file_path.toString());
                    return ERROR_FILE_NOT_EXISTS;
                }
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
