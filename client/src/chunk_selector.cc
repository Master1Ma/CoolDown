#include "chunk_selector.h"
#include "local_sock_manager.h"
#include "job_info.h"
#include <Poco/Bugcheck.h>


namespace CoolDown{
    namespace Client{

        ChunkSelector::ChunkSelector(JobInfo& info, LocalSockManager& sockManager)
        :jobInfo_(info){
        //sockManager_(sockManager){
        }
        ChunkSelector::~ChunkSelector(){
        }

        void ChunkSelector::init_queue(){
            //remove all chunk in queue
            this->chunk_queue_ = chunk_priority_queue_t();
            const TorrentInfo::file_map_t& fileMap = jobInfo_.torrentInfo.get_file_map();
            TorrentInfo::file_map_t::const_iterator iter = fileMap.begin();

            while( iter != fileMap.end() ){

                for(int chunk_pos = 0; chunk_pos != iter->second->chunk_count(); ++chunk_pos){
                    if( jobInfo_.downloadInfo.bitmap->test(chunk_pos) ){
                        //this part has been downloaded
                        continue;
                    }else{
                        ChunkInfoPtr info(new ChunkInfo);
                        info->chunk_num = chunk_pos;
                        info->fileid = iter->first;
                        get_priority(info, 0);
                        chunk_queue_.push(info);
                    }
                }
                ++iter;
            }
        }

        void ChunkSelector::get_priority(ChunkInfoPtr info, int baseline){
            JobInfo::owner_info_map_t& infoMap = jobInfo_.ownerInfoMap;
            poco_assert( infoMap.find(info->fileid) != infoMap.end() );
            const FileOwnerInfoPtrList& infoList = infoMap[ info->fileid ];

            info->clientLists.clear();
            info->priority = 0;

            FileOwnerInfoPtrList::const_iterator iter = infoList.begin();
            while( iter != infoList.end() ){
                FileOwnerInfoPtr p = *iter;
                if( p->bitmap_ptr->test(info->chunk_num) ){
                    info->clientLists.push_back(p);
                }
                ++iter;
            }

            if( info->clientLists.size() == 0 ){
                info->priority = UNAVAILABLE;
            }else if( info->clientLists.size() < RARE_COUNT ){
                info->priority = HIGHEST - info->clientLists.size();
            }else{
                info->priority = NORMAL;
            }

            info->priority += baseline;
        }
        
        ChunkInfoPtr ChunkSelector::get_chunk(){
            FastMutex::ScopedLock lock( chunk_queue_mutex_ );
            ChunkInfoPtr p;
            if( chunk_queue_.size() > 0 ){
                p = chunk_queue_.top();
                chunk_queue_.pop();
            }
            return p;
        }
        
        void ChunkSelector::report_success_chunk(int chunk_num){
            //mark this chunk succeed.
            (*(jobInfo_.downloadInfo.bitmap))[chunk_num] = 1;
        }

        void ChunkSelector::report_failed_chunk(int chunk_num, const string& fileid){
            ChunkInfoPtr info(new ChunkInfo);
            info->status = FAILED;
            info->chunk_num = chunk_num;
            info->fileid = fileid;
            get_priority(info, 0 - NORMAL);
            FastMutex::ScopedLock lock( chunk_queue_mutex_ );
            chunk_queue_.push(info);
        }
    }
}
