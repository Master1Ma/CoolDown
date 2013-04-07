#include "job.h"
#include "chunk_selector.h"
#include "job_info.h"
#include "local_sock_manager.h"
#include <vector>
#include <algorithm>

using std::vector;
using std::min_element;

namespace CoolDown{
    namespace Client{

        Job::Job(JobInfo& info, LocalSockManager& m, Logger& logger)
        :info_(info), sockManager_(m), logger_(logger){
        }

        Job::~Job(){
        }

        void Job::run(){
            ChunkSelector cs(info_, sockManager_);
            while(1){
                if( info_.downloadInfo.is_finished ){
                    break;
                }

                if( info_.downloadInfo.is_download_paused ){
                    info_.download_pause_cond.wait(info_.download_pause_mutex);
                }else{
                    ChunkInfoPtr chunk_info = cs.get_chunk();
                    vector<double> payloads;
                    payloads.reserve(chunk_info->clientLists.size());

                    for(int i = 0; i != chunk_info->clientLists.size(); ++i){
                        FileOwnerInfoPtr ownerInfo = chunk_info->clientLists[i];
                        payloads.push_back( sockManager_.get_payload_percentage(ownerInfo->clientid) );
                    }
                    vector<double>::iterator iter = min_element(payloads.begin(), payloads.end());
                    int index = iter - payloads.begin();
                    poco_debug_f1(logger_, "Choose the %d file owner.", index);

                    if( (*iter - 1) < 1e-6 ){
                        poco_debug(logger_, "Even the lowest payload client is 100% payload.");
                    }

                    LocalSockManager::SockPtr sock = sockManager_.get_idle_client_sock(chunk_info->clientLists[index]->clientid);
                }
            }
        }
    }
}
