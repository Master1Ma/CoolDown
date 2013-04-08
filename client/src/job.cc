#include "job.h"
#include "job_info.h"
#include "local_sock_manager.h"
#include "download_task.h"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <Poco/Observer.h>
#include <Poco/Path.h>
#include <Poco/File.h>

using std::vector;
using std::min_element;
using Poco::Observer;
using Poco::Path;
using Poco::File;

namespace CoolDown{
    namespace Client{

        Job::Job(JobInfo& info, LocalSockManager& m, Logger& logger)
        :jobInfo_(info), sockManager_(m), cs_(jobInfo_, sockManager_), logger_(logger){
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
            cs_.report_failed_chunk( pTask->chunk_pos() );
            pTask->set_reported();
        }

        void Job::run(){
            while(1){
                //see if the Job(upload&download) has been shutdown
                if( jobInfo_.downloadInfo.is_finished ){
                    break;
                }
                Path path(jobInfo_.localFileInfo.path, jobInfo_.localFileInfo.filename);
                File file(path);
                //see if the download has been paused
                if( jobInfo_.downloadInfo.is_download_paused ){
                    poco_debug(logger_, "download paused, goint to wait the download_pause_cond.");
                    jobInfo_.download_pause_cond.wait(jobInfo_.download_pause_mutex);
                }else{
                    ChunkInfoPtr chunk_info = cs_.get_chunk();
                    if( chunk_info.isNull() ){
                        poco_debug(logger_, "Download succeed.");
                        break;
                    }
                    vector<double> payloads;
                    payloads.reserve(chunk_info->clientLists.size());

                    for(int i = 0; i != chunk_info->clientLists.size(); ++i){
                        FileOwnerInfoPtr ownerInfo = chunk_info->clientLists[i];
                        payloads.push_back( sockManager_.get_payload_percentage(ownerInfo->clientid) );
                    }

                    vector<double>::iterator iter = min_element(payloads.begin(), payloads.end());
                    int index = iter - payloads.begin();
                    poco_debug_f1(logger_, "Choose the %d file owner.", index);

                    //see if the minimal payload is 100%
                    if( abs(*iter - 1) < 1e-6 ){
                        poco_debug(logger_, "Even the lowest payload client is 100% payload.");
                        try{
                            max_payload_cond_.wait(max_payload_mutex_, 1000);
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
                            tm_.start( new DownloadTask(jobInfo_, clientid, sock, chunk_pos, 
                                        jobInfo_.torrentInfo.chunk_checksum_list[chunk_pos], file) );
                        }
                    }
                }
            }

        }
    }
}
