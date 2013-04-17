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
#include <Poco/File.h>
#include "torrent.pb.h"

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
using Poco::File;

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
            File local_file;
        };

        class TorrentFileInfo{
            public:
                TorrentFileInfo(const Torrent::File& file);
                ~TorrentFileInfo();
                uint64_t get_size() const;
                string get_checksum() const;
                string get_fileid() const;

                int get_chunk_count() const;
                int get_chunk_size(int chunk_pos) const;
                string get_chunk_checksum(int chunk_pos) const;
                uint64_t get_chunk_offset(int chunk_pos) const;
            private:
                Torrent::File file_;
                int chunk_count_;
                string fileid_;
        };

        typedef SharedPtr<TorrentFileInfo> TorrentFileInfoPtr;

        class TorrentInfo{
            public:
                typedef map<string, TorrentFileInfoPtr> file_map_t;
                TorrentInfo(const Torrent::Torrent& torrent);
                ~TorrentInfo();
                int get_file_count() const;
                const file_map_t& get_file_map() const;
                const TorrentFileInfoPtr& get_file(const string& fileid);

            private:
                Torrent::Torrent torrent_;
                int file_count_;
                file_map_t fileMap_;
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

            Condition download_speed_limit_cond;
            FastMutex download_speed_limit_mutex;

            Condition download_pause_cond;
            FastMutex download_pause_mutex;

            int percentage;
            int max_parallel_task;
            file_bitmap_ptr bitmap;
        };


        class CoolClient;
        class JobInfo{
            private:
                CoolClient& app_;
                Logger& logger_;

            public:
                JobInfo(const Torrent::Torrent& torrent);
                ~JobInfo();

                string clientid() const;

                typedef map<string, FileOwnerInfoPtr> owner_info_map_t;
                LocalFileInfo localFileInfo;
                owner_info_map_t ownerInfoMap;
                DownloadInfo downloadInfo;
                TorrentInfo torrentInfo;

        };
    }
}

#endif
