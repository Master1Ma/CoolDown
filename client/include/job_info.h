#ifndef JOB_INFO_h
#define JOB_INFO_h

#include "torrent.pb.h"
#include "error_code.h"
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
#include <Poco/Types.h>

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
using Poco::Int64;

namespace CoolDown{
    namespace Client{

        typedef dynamic_bitset<uint64_t> file_bitmap_t;
        typedef SharedPtr<file_bitmap_t> file_bitmap_ptr;
        typedef vector<string> StringList;
        typedef SharedPtr<File> FilePtr;
        const static string INVALID_FILEID = "INVALID FILEID";
        const static string INVALID_CLIENTID = "INVALID CLIENTID";

        struct FileIdentityInfo{
            FileIdentityInfo();

            FileIdentityInfo(const string& relative_path, const string& filename);
            string relative_path;
            string filename;
        };


        typedef vector<FileIdentityInfo> FileIdentityInfoList;

        class LocalFileInfo{
            public:
                LocalFileInfo(const string& top_path);
                retcode_t add_file(const string& fileid, const string& relative_path, const string& filename, Int64 filesize);
                FilePtr get_file(const string& fileid);
                bool has_local_file(const string& fileid, const string& relative_path, const string& filename);
                bool has_file(const string& fileid);
                map<string, StringList>& same_files_map();

                string top_path() const{
                    return this->top_path_;
                }

            private:
                string top_path_;
                map<string, FilePtr> files;
                
                //this map is used when a Torrent has same file in different path.
                //key : fileid, value : (relative_path + filename) of all same files
                //the real download one is the first one in StringList
                //when the download one complete, we simply copy from it to the rest.
                map<string, StringList> same_files_;
                FastMutex mutex_;
        };

        class TorrentFileInfo{
            public:
                TorrentFileInfo(const Torrent::File& file);
                ~TorrentFileInfo();
                uint64_t size() const;
                string checksum() const;
                string fileid() const;
                string relative_path() const;
                string filename() const;

                int chunk_count() const;
                int chunk_size(int chunk_pos) const;
                string chunk_checksum(int chunk_pos) const;
                uint64_t chunk_offset(int chunk_pos) const;
            private:
                Torrent::File file_;
                int chunk_count_;
                string fileid_;
        };

        typedef SharedPtr<TorrentFileInfo> TorrentFileInfoPtr;

        class TorrentInfo{
            public:
                typedef map<string, vector<TorrentFileInfoPtr> >file_map_t;
                TorrentInfo(const Torrent::Torrent& torrent);
                ~TorrentInfo();
                int get_file_count() const;
                const file_map_t& get_file_map() const;
                const TorrentFileInfoPtr& get_one_file_of_same_fileid(const string& fileid);
                const TorrentFileInfoPtr& get_file(const string& fileid, const string& relative_path, const string& filename);
                string tracker_address() const;
                string torrentid() const;

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
        typedef vector<FileOwnerInfoPtr> FileOwnerInfoPtrList;

        struct FileOwnerInfoPtrSelector{
            FileOwnerInfoPtrSelector(const string& clientid)
            :clientid_(clientid){
            }

            bool operator()(const FileOwnerInfoPtr& pInfo){
                return pInfo->clientid == clientid_;
            }

            private:
                string clientid_;
        };

        struct DownloadInfo{
            DownloadInfo();
            atomic_bool is_job_removed;
            atomic_bool is_stopped;
            atomic_bool is_download_paused;
            atomic_bool is_upload_paused;
            //atomic_bool is_download_finished;

            atomic_uint64_t bytes_upload_this_second;
            atomic_uint64_t bytes_download_this_second;
            atomic_uint64_t upload_speed_limit;     //bytes per second
            atomic_uint64_t download_speed_limit;   //bytes per second
            
            atomic_uint64_t upload_total;
            atomic_uint64_t download_total;

            Condition download_speed_limit_cond;
            Condition upload_speed_limit_cond;
            Condition download_pause_cond;
            Condition job_stopped_cond;
            Condition job_finished_cond;

            int time_to_next_report;
            const static int report_period = 30;

            typedef map<string, int> percentage_map_t;
            typedef map<string, file_bitmap_ptr> file_bitmap_map_t;

            percentage_map_t percentage_map;
            file_bitmap_map_t bitmap_map;

            int max_parallel_task;

        };


        class CoolClient;
        class JobInfo{
            private:
                CoolClient& app_;
                Logger& logger_;
                StringList fileidlist_;

            public:
                JobInfo(const Torrent::Torrent& torrent, const string& top_path, 
                        const FileIdentityInfoList& needs);
                ~JobInfo();

                string clientid() const;
                const StringList& fileidlist() const;
                //this return a stringlist on stack, so do NOT use reference
                StringList UniqueFileidList() const;

                //key : fileid, value : FileOwnerInfo
                typedef map<string, FileOwnerInfoPtrList> owner_info_map_t;
                LocalFileInfo localFileInfo;
                owner_info_map_t ownerInfoMap;
                DownloadInfo downloadInfo;
                TorrentInfo torrentInfo;

        };
    }
}

#endif
