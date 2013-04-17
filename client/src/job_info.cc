#include "job_info.h"
#include "client.h"
#include <Poco/Util/Application.h>
#include <Poco/Bugcheck.h>

using Poco::Util::Application;

namespace CoolDown{
    namespace Client{

        //TorrentFileInfo 
        TorrentFileInfo::TorrentFileInfo(const Torrent::File& file)
        :file_(file), chunk_count_(file_.chunk().size()), fileid_(file_.checksum()){
        }

        TorrentFileInfo::~TorrentFileInfo(){
        }

        int TorrentFileInfo::get_chunk_count() const{
            return this->chunk_count_;
        }

        uint64_t TorrentFileInfo::get_size() const{
            return file_.size();
        }

        string TorrentFileInfo::get_checksum() const{
            return file_.checksum();
        }

        string TorrentFileInfo::get_fileid() const{
            return this->fileid_;
        }

        string TorrentFileInfo::get_chunk_checksum(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return "";
            }
            return file_.chunk().Get(chunk_pos).checksum();

        }
        uint64_t TorrentFileInfo::get_chunk_offset(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return 0;
            }
            uint64_t chunk_size = file_.chunk().Get(0).size();
            return chunk_size * chunk_pos;
        }


        int TorrentFileInfo::get_chunk_size(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return -1;
            }
            return file_.chunk().Get(chunk_pos).size();
        }


        //TorrentInfo
        TorrentInfo::TorrentInfo(const Torrent::Torrent& torrent)
        :torrent_(torrent), file_count_(torrent_.file().size()){
            for(int pos = 0; pos != torrent_.file().size(); ++i){
                const Torrent::File& file = torrent_.Get(pos);
                fileMap_[file.checksum()] = TorrentFileInfoPtr(file);
            }
        }

        TorrentInfo::~TorrentInfo(){
        }

        const file_map_t& TorrentFileInfo::get_file_map() const{
            return this->fileMap_;
        }
        
        int TorrentFileInfo::get_file_count() const{
            return this->file_count_;
        }
        
        const TorrentFileInfoPtr& TorrentInfo::get_file(const string& fileid){
            map<string, TorrentFileInfoPtr>::iterator iter = fileMap_.find(fileid);
            poco_assert( iter != fileMap_.end() );
            return iter->second;
        }

        //JobInfo
        JobInfo::JobInfo()
        :app_(dynamic_cast<CoolClient&>(Application::instance()) ),
         logger_(app_.logger())
        {
        }

        JobInfo::~JobInfo(){
        }

        string JobInfo::clientid() const{
            return app_.clientid();
        }

    }
}
