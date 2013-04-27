#include "chunk_selector.h"
#include "local_sock_manager.h"
#include "job_info.h"
#include <Poco/Bugcheck.h>
#include <Poco/Util/Application.h>
#include <boost/foreach.hpp>


namespace CoolDown{
    namespace Client{

        ChunkSelector::ChunkSelector(JobInfo& info, LocalSockManager& sockManager)
        :jobInfo_(info),
        logger_(::Poco::Util::Application::instance().logger()){
        //sockManager_(sockManager){
        }
        ChunkSelector::~ChunkSelector(){
        }

        vector<string> ChunkSelector::fileidlist(){
            return jobInfo_.fileidlist();
        }

        void ChunkSelector::init_queue(){
            //remove all chunk in queue
            this->chunk_queue_ = chunk_priority_queue_t();
            const TorrentInfo::file_map_t& fileMap = jobInfo_.torrentInfo.get_file_map();
            BOOST_FOREACH(const string& fileid, jobInfo_.UniqueFileidList() ){
                TorrentInfo::file_map_t::const_iterator iter = fileMap.find(fileid);
                poco_assert( iter != fileMap.end() );
                poco_trace_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);

                poco_assert( iter->second.size() != 0 );
                poco_trace_f2(logger_, "assert passed at file : %s, line : %d", string(__FILE__), __LINE__ - 1);

                int chunk_count = iter->second.at(0)->chunk_count();
                for(int chunk_pos = 0; chunk_pos != chunk_count; ++chunk_pos){
                    if( jobInfo_.downloadInfo.bitmap_map[iter->first]->test(chunk_pos) ){
                        //this part has been downloaded
                        continue;
                    }else{
                        ChunkInfoPtr info(new ChunkInfo);
                        info->chunk_num = chunk_pos;
                        info->fileid = iter->first;
                        get_priority(info, 0);
                        chunk_queue_.push(info);
                        poco_debug_f1(logger_, "new chunk of file '%s' has been added to chunk_queue_", info->fileid);
                    }
                }
                
            }
        }

        void ChunkSelector::get_priority(ChunkInfoPtr info, int baseline){
            JobInfo::owner_info_map_t& infoMap = jobInfo_.ownerInfoMap;
            poco_assert( infoMap.find(info->fileid) != infoMap.end() );
            const FileOwnerInfoPtrList& infoList = infoMap[ info->fileid ];
            poco_debug_f2(logger_, "%d clients owns file '%s' ", (int)infoList.size(), info->fileid);

            info->clientLists.clear();
            info->priority = 0;

            BOOST_FOREACH(FileOwnerInfoPtr p, infoList){
                //poco_debug_f1(logger_, "to_ulong = %lu", p->bitmap_ptr->to_ulong());
                if( p->bitmap_ptr->test(info->chunk_num) ){
                    info->clientLists.push_back(p);
                }
            }

            if( info->clientLists.size() == 0 ){
                poco_notice_f2(logger_, "no client has file '%s', chunk_pos '%d'", info->fileid, info->chunk_num);
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
        
        void ChunkSelector::report_success_chunk(int chunk_num, const string& fileid){
            //mark this chunk succeed.
            (*(jobInfo_.downloadInfo.bitmap_map[fileid]))[chunk_num] = 1;
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

        void ChunkSelector::report_no_owner_chunk(int chunk_num, const string& fileid){
            ChunkInfoPtr info(new ChunkInfo);
            info->status = NOOWNER;
            info->chunk_num = chunk_num;
            info->fileid = fileid;

            //since no owner of this chunk is online, we give it the lowest priority, which is less-equal than 0.
            get_priority(info, 0 - HIGHEST);
            FastMutex::ScopedLock lock( chunk_queue_mutex_ );
            chunk_queue_.push(info);
        }
    }
}
