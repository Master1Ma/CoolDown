#ifndef JOB_INFO_h
#define JOB_INFO_h

#include <map>
#include <string>
#include <vector>
#include <boost/dynamic_bitset.hpp>
#include <boost/cstdint.hpp>
#include <Poco/SharedPtr.h>
#include <boost/atomic.hpp>
#include <Poco/Logger.h>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>

using std::map;
using std::string;
using std::vector;
using boost::dynamic_bitset;
using boost::uint64_t;
using boost::atomic_bool;
using boost::atomic_uint64_t;
using boost::atomic_uint32_t;
using Poco::SharedPtr;
using Poco::Logger;
using Poco::Condition;
using Poco::FastMutex;

namespace CoolDown{
    namespace Client{

        typedef dynamic_bitset<uint64_t> file_bitmap_t;
        typedef SharedPtr<file_bitmap_t> file_bitmap_ptr;
        typedef vector<string> StringList;
        const static string INVALID_FILEID = "INVALID FILEID";
        const static string INVALID_CLIENTID = "INVALID CLIENTID";

        struct LocalFileInfo{
            string path;
            string filename;
        };

        struct TorrentInfo{
            string fileid;
            string file_checksum;

            uint64_t file_size;
            string file_name;
            string create_time;
            int type;

            int size_per_chunk;
            int chunk_count;
            StringList chunk_checksum_list;
        };

        struct FileOwnerInfo{
            FileOwnerInfo()
                :clientid(INVALID_CLIENTID),
                ip("") , message_port(0),
                percentage(0), bitmap_ptr(new file_bitmap_t)
            {
            }

            FileOwnerInfo(const string& clientid, const string& ip, int message_port, int percentage)
                :clientid(clientid), ip(ip), message_port(message_port), 
                percentage(percentage), bitmap_ptr(new file_bitmap_t){
                }

            string clientid;
            string ip;
            int message_port;
            int percentage;
            file_bitmap_ptr bitmap_ptr;
        };

        typedef SharedPtr<FileOwnerInfo> FileOwnerInfoPtr;

        struct DownloadInfo{
            atomic_bool is_finished;
            atomic_bool is_download_paused;
            atomic_bool is_upload_paused;

            atomic_uint64_t bytes_upload_this_second;
            atomic_uint64_t bytes_download_this_second;
            atomic_uint64_t upload_speed_limit;     //bytes per second
            atomic_uint64_t download_speed_limit;   //bytes per second
            
            atomic_uint64_t upload_total;
            atomic_uint64_t download_total;

            int percentage;
            int max_parallel_task;
            file_bitmap_ptr bitmap;
        };


        class CoolClient;
        class JobInfo{
            public:
                JobInfo();
                ~JobInfo();

                string clientid() const;

                typedef map<string, FileOwnerInfoPtr> owner_info_map_t;
                LocalFileInfo localFileInfo;
                owner_info_map_t ownerInfoMap;
                DownloadInfo downloadInfo;
                TorrentInfo torrentInfo;
                Condition download_speed_limit_cond;
                FastMutex download_speed_limit_mutex;

                Condition download_pause_cond;
                FastMutex download_pause_mutex;

            private:
                CoolClient& app_;
                Logger& logger_;
        };
    }
}

#endif
