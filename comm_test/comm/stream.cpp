
#include "../binstream/stlstream.h"
#include "../binstream/filestream.h"
#include "../metastream/metastream.h"
#include "../metastream/fmtstreamcxx.h"
#include "../metastream/fmtstreamjson.h"
#include "../metastream/fmtstreamxml2.h"

#include <sstream>

namespace coid {

struct STLMIX {
    std::vector<std::string> vector;
    std::list<std::string> list;
    std::deque<std::string> deque;
    std::set<std::string> set;
    std::multiset<std::string> multiset;
    std::map<std::string,uint> map;
    std::multimap<std::string,uint> multimap;

    inline friend metastream& operator || (metastream& m, STLMIX& x)
    {
        return m.compound_type(x, [&]()
        {
            m.member("vector", x.vector);
            m.member("list", x.list);
            m.member("deque", x.deque);
            m.member("set", x.set);
            m.member("multiset", x.multiset);
            //m.member("map", x.map);
            //m.member("multimap", x.multimap);
        });
    }
};

struct string_test
{
    charstr normal_text = "abcd123\\";
    charstr quoted_text = "\"hello\"";
    charstr special_text = "\tend\n";

    inline friend metastream& operator || (metastream& m, string_test& x)
    {
        return m.compound_type(x, [&]()
        {
            m.member("normal", x.normal_text);
            m.member("quoted", x.quoted_text);
            m.member("special", x.special_text);
        });
    }

    void test()
    {
        binstreambuf buf;
        fmtstreamcxx fmt(buf);
        metastream meta(fmt);

        buf.write_token_raw("CXX\n\n");

        meta.stream_out(*this);
        meta.stream_flush();

        buf.write_token_raw("\n\nJSON normal\n\n");

        fmtstreamjson fmtjn(buf, false);
        meta.bind_formatting_stream(fmtjn);
        meta.stream_out(*this);
        meta.stream_flush();

        buf.write_token_raw("\n\nJSON escaped\n\n");

        fmtstreamjson fmtje(buf, true);
        meta.bind_formatting_stream(fmtje);
        meta.stream_out(*this);
        meta.stream_flush();

        bofstream bof("string_test.txt");
        buf.transfer_to(bof);
        bof.close();
    }
};


void std_test()
{
    string_test st;
    st.test();

    {
        token t = "fashion";
        const char* v = "test";

        std::ostringstream ost;
        ost << t << v;
    }


    //metastream out and in test

    bofstream bof("stl_meta.test");
    fmtstreamcxx txpo(bof);
    metastream meta(txpo);

    STLMIX x;
    x.vector.push_back("abc");
    x.vector.push_back("def");
    x.list.push_back("ghi");
    x.deque.push_back("ijk");
    x.set.insert("lmn");
    x.multiset.insert("opq");
    x.map.insert(std::pair<std::string,uint>("rst",1));

    meta.stream_out(x);
    meta.stream_flush();

    bof.close();


    bifstream bif("stl_meta.test");

    fmtstreamcxx txpi(bif);
    meta.bind_formatting_stream(txpi);

    STLMIX y;
    meta.stream_in(y);
    meta.stream_acknowledge();

    bif.close();
}

} //namespace coid
