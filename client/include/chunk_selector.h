#ifndef CHUNK_SELECTOR_H
#define CHUNK_SELECTOR_H

#include "job_info.h"
#include <vector>
#include <queue>
#include <Poco/Mutex.h>
using std::vector;
using std::priority_queue;
using Poco::FastMutex;

namespace CoolDown{
    namespace Client{

        class LocalSockManager;

        enum ChunkStatus{
            FREE = 1,
            DOWNLOADING = 2,
            FINISHED = 3,
            FAILED = 4,
        };

        struct ChunkInfo{
            ChunkInfo()
            :status(FREE), priority(0), chunk_num(0){
            }
            ChunkStatus status;
            int priority;
            int chunk_num;
            vector<FileOwnerInfoPtr> clientLists;
        };
        typedef SharedPtr<ChunkInfo> ChunkInfoPtr;

        
        struct chunk_cmp{
            bool operator()(const ChunkInfoPtr& left, const ChunkInfoPtr& right){
                return left->priority < right->priority;
            }
        };

        class ChunkSelector{
            public:
                ChunkSelector(JobInfo& info, LocalSockManager& sockManager);
                ~ChunkSelector();

                ChunkInfoPtr get_chunk();
                void report_chunk(ChunkInfoPtr info);

            private:
                void init_queue();
                void get_priority(ChunkInfoPtr info, int baseline);

                const static int RARE_COUNT = 5;
                enum Priority{
                    HIGHEST = 20,
                    NORMAL = 10,
                    BUSY = 5,
                    UNAVAILABLE = 0
                };
                JobInfo& jobInfo_;
                LocalSockManager& sockManager_;

                priority_queue<ChunkInfoPtr, vector<ChunkInfoPtr>, chunk_cmp> chunk_queue_;
                FastMutex chunk_queue_mutex_;
        };
    }
}

#endif
