#include "file_owner_info.h"

FileOwnerInfo::FileOwnerInfo()
    :percentage_(0)
{
}

FileOwnerInfo::FileOwnerInfo(const string& clientid, const string& torrentid, const string& fileid, int percentage)
    :clientid_(clientid), torrentid_(torrentid_), fileid_(fileid), percentage_(percentage)
{
}


