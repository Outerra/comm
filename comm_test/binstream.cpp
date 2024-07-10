
#include <comm/binstream/binstreambuf.h>
#include <comm/binstream/cachestream.h>
#include <comm/binstream/enc_base64stream.h>
#include <comm/binstream/enc_hexstream.h>
#include <comm/binstream/filestream.h>
#include <comm/binstream/filestreamgz.h>
#include <comm/binstream/filestreamzstd.h>
#include <comm/binstream/forkstream.h>
#include <comm/binstream/hash_sha1stream.h>
#include <comm/binstream/hash_xxhashstream.h>
#include <comm/binstream/nullstream.h>
#include <comm/binstream/packstream.h>
#include <comm/binstream/packstreambzip2.h>
#include <comm/binstream/packstreamlz4.h>
#include <comm/binstream/packstreamzip.h>
#include <comm/binstream/packstreamzstd.h>

#include <comm/binstream/stdstream.h>
#include <comm/binstream/stlstream.h>

#include <comm/binstream/txtstream.h>

#include <comm/metastream/fmtstreamcxx.h>
#include <comm/metastream/fmtstreamjson.h>
#include <comm/metastream/fmtstreamnull.h>
#include <comm/metastream/fmtstreamxml2.h>
//#include <comm/metastream/fmtstream_jsc.h>
//#include <comm/metastream/fmtstream_lua_capi.h>
//#include <comm/metastream/fmtstream_v8.h>

using namespace coid;

void test()
{
    binstreambuf buf;
    cachestream cache;
    enc_base64stream e64;
    enc_hexstream ehex;
    filestream file;
    filestreamgz fgz;
    filestreamzstd fzst;
    forkstream fork;
    hash_sha1stream hsha1;
    hash_xxhashstream hxx;
    nullstream_t nuls;

    packstreambzip2 bzip2;
    packstreamlz4 lz4;
    packstreamzip zip;
    packstreamzstd zstd;
    stdoutstream out;

    fmtstreamcxx cxx;
    fmtstreamjson json(true);
    fmtstreamxml2 xml;
    fmtstreamnull fnull;

    std::vector<int> vec;
    std::string str;

    xml << vec;
    xml << str;
}