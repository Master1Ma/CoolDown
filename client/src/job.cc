#include "job.h"
#include "local_sock_manager.h"
#include "download_task.h"
#include "client.h"
#include "client.pb.h"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <Poco/Util/Application.h>
#include <Poco/Observer.h>
#include <Poco/Path.h>
#include <Poco/File.h>
//#include <Poco/Bugcheck.h>

using std::vector;
using std::min_element;
using Poco::Util::Application;
using Poco::Observer;
using Poco::Path;
using Poco::File;
using namespace ClientProto;

namespace CoolDown{
    namespace Client{

        Job::Job(const JobInfoPtr& info, LocalSockManager& m, Logger& logger)
        :app_( dynamic_cast<CoolClient&>( Application::instance() )),
        jobInfoPtr_(info),
        jobInfo_(*jobInfoPtr_), 
        sockManager_(m), 
        cs_(jobInfo_, sockManager_), 
        logger_(logger){
            tm_.addObserver(
                    Observer<Job, TaskFinishedNotification>
                    (*this, &Job::onFinished)
                    );
            tm_.addObserver(
                    Observer<Job, TaskFailedNotification>
                    (*this, &Job::onFailed)
                    );
        }

        Job::~Job(){
            tm_.removeObserver(
                    Observer<Job, TaskFinishedNotification>
                    (*this, &Job::onFinished)
                    );
            tm_.removeObserver(
                    Observer<Job, TaskFailedNotification>
                    (*this, &Job::onFailed)
                    );
        }

        JobInfoPtr Job::MutableJobInfo(){
            return this->jobInfoPtr_;
        }
        const JobInfo& Job::JobInfo() const{
            return this->jobInfo_;
        }

        static void Job::convert_bitmap_to_transport_format(const file_bitmap_ptr& bitmap, ClientProto::FileInfo* pInfo){
            
        }
        static void Job::conver_transport_format_bitmap(const ClientProto::FileInfo& info, file_bitmap_ptr& bitmap){
        }
        
        void Job::onFinished(TaskFinishedNotification* pNf){
            DownloadTask* pTask = dynamic_cast<DownloadTask*>( pNf->task() );
            if( pTask == NULL ){
                poco_error(logger_, "cannot cast Task* to DownloadTask*, usually means fatal error.");
                return;
            }else if( pTask->reported() ){
                //this task has been reported by onFailed, do nothing;
            }else{
                cs_.report_success_chunk( pTask->chunk_pos() );
            }
            //stuff all task must do
            pTask->set_reported();
            sockManager_.return_sock(pTask->clientid(), pTask->sock() );
            max_payload_cond_.signal();
        }

        void Job::onFailed(TaskFailedNotification* pNf){
            DownloadTask* pTask = dynamic_cast<DownloadTask*>( pNf->task() );
            if( pTask == NULL ){
                poco_error(logger_, "cannot cast Task* to DownloadTask*, usually means fatal error.");
                return;
            }
            poco_notice_f2(logger_,"task failed, name : %s, reason : %s", pTask->name(), pNf->reason().displayText());
            cs_.report_failed_chunk( pTask->chunk_pos(), pTask->fileid() );
            pTask->set_reported();
        }

        void Job::run(){
            cs_.init_queue();
            while(1){
                const static int WAIT_TIMEOUT = 1000;
                poco_debug(logger_, "enter Job::run()");
                //see if the Job(upload&download) has been shutdown
                if( jobInfo_.downloadInfo.is_finished ){
                    break;
                }
                //File file(jobInfo_.localFileInfo.local_file)
                //see if the download has been paused
                if( jobInfo_.downloadInfo.is_download_paused ){
                    poco_debug(logger_, "download paused, going to wait the download_pause_cond.");
                    jobInfo_.downloadInfo.download_pause_cond.wait(jobInfo_.downloadInfo.download_pause_mutex, WAIT_TIMEOUT);
                }else{
                    ChunkInfoPtr chunk_info = cs_.get_chunk();
                    if( chunk_info.isNull() ){
                        poco_debug(logger_, "Download succeed.");
                        break;
                    }
                    vector<double> payloads;
                    int client_count = chunk_info->clientLists.size();
                    if( client_count == 0 ){
                        poco_notice(logger_, "Current no client has this chunk.");
                        break;
                    }

                    poco_debug_f1(logger_, "Download chunk from %d clients", client_count);
                    payloads.reserve( client_count );

                    for(int i = 0; i != chunk_info->clientLists.size(); ++i){
                        FileOwnerInfoPtr ownerInfo = chunk_info->clientLists[i];
                        payloads.push_back( sockManager_.get_payload_percentage(ownerInfo->clientid) );
                    }

                    vector<double>::iterator iter = min_element(payloads.begin(), payloads.end());
                    int index = iter - payloads.begin();
                    poco_debug_f1(logger_, "Choose the index %d file owner.", index);

                    //see if the minimal payload is 100%
                    if( abs(*iter - 1) < 1e-6 ){
                        poco_debug(logger_, "Even the lowest payload client is 100% payload.");
                        try{
                            max_payload_cond_.wait(max_payload_mutex_, WAIT_TIMEOUT);
                            poco_notice(logger_, "wake up from max_payload_cond_");
                        }catch(Poco::TimeoutException& e){
                            poco_notice(logger_, "Wait max_payload_cond_ timeout!");
                        }
                        continue;
                    }else{
                        string clientid( chunk_info->clientLists[index]->clientid );
                        LocalSockManager::SockPtr sock = sockManager_.get_idle_client_sock(clientid);
                        //see if some error happend in get_idle_client_sock
                        if( sock.isNull() ){
                            poco_warning_f1(logger_, "Unexpected null SockPtr return by sockManager_.get_idle_client_sock, client id :%s"                                            ,clientid);
                            continue;
                        }else{
                            int chunk_pos = chunk_info->chunk_num;
                            string fileid( chunk_info->fileid );
                            TorrentFileInfoPtr fileInfo = jobInfo_.torrentInfo.get_file(fileid);

                            if( false == jobInfo_.localFileInfo.has_file(fileid) ){
                                jobInfo_.localFileInfo.add_file( fileid, fileInfo->relative_path() );
                            }
                            FilePtr file = jobInfo_.localFileInfo.get_file(fileid);
                            tm_.start( new DownloadTask(
                                        *fileInfo, 
                                        jobInfo_.downloadInfo, 
                                        jobInfo_.clientid(), 
                                        sock, 
                                        chunk_pos, 
                                        *file
                                        ) 
                                    );
                        }
                    }
                }
            }

        }

        retcode_t Job::request_clients(const string& fileid){
            string tracker_address(jobInfo_.torrentInfo.tracker_address());
            int percentage = jobInfo_.downloadInfo.percentage;
            int needCount = 20;
            CoolClient::ClientIdCollection clientidList;
            JobInfo::owner_info_map_t& infoMap = jobInfo_.ownerInfoMap;
            JobInfo::owner_info_map_t::iterator iter = infoMap.find(fileid);
            if( iter != infoMap.end() ){
                FileOwnerInfoPtrList& infoPtrList = iter->second;
                for(int i = 0; i != infoPtrList.size(); ++i){
                    clientidList.push_back(infoPtrList[i]->clientid);
                }
            }

            FileOwnerInfoPtrList res;
            retcode_t ret = app_.request_clients(tracker_address, fileid, percentage, 
                                          needCount, clientidList, &res);
            if( ret != ERROR_OK ){
                poco_warning_f1(logger_, "app_.request_clients return %d", (int)ret);
            }else{
                iter->second = res;
            }
            return ret;
        }

        retcode_t Job::shake_hand(const string& fileid, const string& clientid){
            ShakeHand self;
            self.set_clientid( app_.clientid() );
            FileInfo* pInfo = self.mutable_info();
            pInfo->set_fileid(fileid);
            pInfo->set_hasfile(1);
            pInfo->set_percentage();
            pInfo->

            return ERROR_OK;
        }
    }
}
