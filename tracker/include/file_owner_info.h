#ifndef FILE_OWNER_INFO_H
#define FILE_OWNER_INFO_H

#include <string>
using std::string;

class FileOwnerInfo{
    public:
        FileOwnerInfo();
        FileOwnerInfo(const string& clientid, const string& torrentid, const string& fileid, int percentage);

        void set_clientid(const string& clientid){
            this->clientid_ = clientid;
        }
        void set_fileid(const string& fileid){
            this->fileid_ = fileid;
        }

        void set_torrentid(const string& torrentid){
            this->torrentid_ = torrentid;
        }
        void set_percentage(int percentage){
            this->percentage_ = percentage;
        }

        string clientid() const{
            return clientid_;
        }

        string torrentid() const{
            return torrentid_;
        }

        string fileid() const{
            return fileid_;
        }

        int percentage() const{
            return percentage_;
        }

    private:
        string clientid_;
        string torrentid_;
        string fileid_;
        int percentage_;
};

#endif
