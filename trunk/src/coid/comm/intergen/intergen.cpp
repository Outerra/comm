
#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../binstream/enc_base64stream.h"
#include "../metastream/metagen.h"
#include "../metastream/fmtstreamcxx.h"
#include "../dir.h"

#include "../hash/hashkeyset.h"

#include "ig.h"

stdoutstream out;

static const int VERSION = 4;

////////////////////////////////////////////////////////////////////////////////
const token iglexer::MARK = "rl_cmd";
const token iglexer::MARKP = "rl_cmd_p";
const token iglexer::MARKS = "rl_cmd_s";
const token iglexer::CLASS = "class";
const token iglexer::STRUCT = "struct";
const token iglexer::TEMPL = "template";
const token iglexer::NAMESPC = "namespace";

const token iglexer::IFC_CLASS = "ifc_class";
const token iglexer::IFC_CLASS_VAR = "ifc_class_var";
const token iglexer::IFC_CLASS_VIRTUAL = "ifc_class_virtual";
const token iglexer::IFC_FN = "ifc_fn";
const token iglexer::IFC_FNX = "ifc_fnx";
const token iglexer::IFC_EVENT = "ifc_event";
const token iglexer::IFC_EVENTX = "ifc_eventx";
const token iglexer::IFC_EVBODY = "ifc_evbody";
const token iglexer::IFC_INOUT = "ifc_inout";
const token iglexer::IFC_IN = "ifc_in";
const token iglexer::IFC_OUT = "ifc_out";


////////////////////////////////////////////////////////////////////////////////
///
struct File
{
    charstr fpath, fnameext;
    charstr hdrname;
    charstr fname;

    dynarray<Class> classes;

    dynarray<paste_block> pasters;
    dynarray<MethodIG::Arg> irefargs;

    friend metastream& operator || (metastream& m, File& p)
    {
        return m.compound("File", [&]()
        {
            int version = VERSION;
            m.member("hdr",p.fnameext);          //< file name
            m.member("HDR",p.hdrname);           //< file name without extension, uppercase
            m.member("class",p.classes);
            m.member("irefargs",p.irefargs);
            m.member("version",version);
        });
    }


    int parse(token path);

    bool find_class( iglexer& lex, dynarray<charstr>& namespc, charstr& templarg );
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
int generate( const T& t, const token& patfile, const token& outfile )
{
    directory::mkdir_tree(outfile, true);

    directory::set_writable(outfile, true);

    bifstream bit(patfile);
    bofstream bof(outfile);

    if( !bit.is_open() ) {
        out << "error: can't open the template file '" << patfile << "'\n";
        return -5;
    }

    if( !bof.is_open() ) {
        out << "error: can't create the output file '" << outfile << "'\n";
        return -5;
    }

    metagen mtg;
    mtg.set_source_path(patfile);

    if( !mtg.parse(bit) ) {
        out << "error: error parsing the document template:\n";
        out << mtg.err() << '\n';
        out.flush();

        return -6;
    }

    out << "writing " << outfile << " ...\n";
    mtg.generate(t, bof);

    bof.close();

    directory::set_writable(outfile, false);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
int generate_rl( const File& cgf, charstr& patfile, const token& outfile )
{
    uint l = patfile.len();
    patfile << "template.inl.mtg";

    bifstream bit(patfile);
    bofstream bof(outfile);

    metagen mtg;
    mtg.set_source_path(patfile);

    patfile.resize(l);

    if( !bit.is_open() ) {
        out << "error: can't open the template file '" << patfile << "'\n";
        return -5;
    }

    if( !bof.is_open() ) {
        out << "error: can't create the output file '" << outfile << "'\n";
        return -5;
    }

    if( !mtg.parse(bit) ) {
        out << "error: error parsing the document template:\n";
        out << mtg.err();

        return -6;
    }

    if( cgf.classes.size() > 0 ) {
        out << "writing " << outfile << " ...\n";
        cgf.classes.for_each([&](const Class& c) {
            if(c.method.size())
                mtg.generate(c, bof);
        });
    }
    else
        out << "no rl_cmd's found\n";

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
void generate_ig( File& file, charstr& tdir, charstr& fdir  )
{
    directory::treat_trailing_separator(tdir, true);
    uint tlen = tdir.len();

    directory::treat_trailing_separator(fdir, true);
    uint flen = fdir.len();

    int nifc=0;

    uints nc = file.classes.size();
    for(uints c=0; c<nc; ++c)
    {
        Class& cls = file.classes[c];

        //ig
        uints ni = cls.iface.size();
        for(uint i=0; i<ni; ++i)
        {
            Interface& ifc = cls.iface[i];

            ifc.compute_hash(VERSION);

            fdir.resize(flen);
            tdir.resize(tlen);

            //interface.h
            token end;

            if(ifc.relpath.ends_with_icase(end=".hpp") || ifc.relpath.ends_with_icase(end=".hxx") || ifc.relpath.ends_with_icase(end=".h"))
                fdir << ifc.relpath;    //contains the file name already
            else if(ifc.relpath.last_char() == '/' || ifc.relpath.last_char() == '\\')
                fdir << ifc.relpath << ifc.name << ".h";
            else
                fdir << ifc.relpath << '/' << ifc.name << ".h";

            ifc.relpath.set_from_range(fdir.ptr()+flen, fdir.ptre());
            directory::compact_path(ifc.relpath);
            ifc.relpath.replace('\\', '/');

            ifc.relpathjs = ifc.relpath;
            ifc.relpathjs.ins(-(int)end.len(), ".js");

            ifc.basepath = ifc.relpath;
            ifc.hdrfile = ifc.basepath.cut_right_back('/', token::cut_trait_keep_sep_with_source());

            ifc.srcfile = &file.fnameext;
            ifc.srcclass = &cls.classname;

            uints nm = ifc.method.size();
            for(uints m=0; m<nm; ++m) {
                if(ifc.method[m].bstatic)
                    ifc.storage = ifc.method[m].storage;
            }

            tdir << "interface.h.mtg";

            if( generate(ifc, tdir, fdir) < 0 )
                return;

            //interface.js.h
            fdir.ins(int(fdir.contains_back('.') - fdir.ptr()), ".js");

            tdir.ins(-6, ".js");

            if( generate(ifc, tdir, fdir) < 0 )
                return;


            tdir.resize(tlen);
            tdir << "interface.doc.mtg";

            fdir.resize(-int(token(fdir).cut_right_back("\\/").len()));
            fdir << "/docs";
            directory::mkdir(fdir);

            fdir << '/' << ifc.name << ".html";

            generate(ifc, tdir, fdir);

            ++nifc;
        }
    }

    //file.intergen.cpp
    fdir.resize(flen);
    fdir << file.fname << ".intergen.cpp";

    tdir.resize(tlen);
    tdir << "file.intergen.cpp.mtg";

    if(nifc>0)
        generate(file, tdir, fdir);
    else
        bofstream bof(fdir);    //create zero file

    //file.intergen.js.cpp
    fdir.ins(-4, ".js");
    tdir.ins(-8, ".js");

    if(nifc>0)
        generate(file, tdir, fdir);
    else
        bofstream bof(fdir);

    tdir.resize(tlen);
    fdir.resize(flen);
}

void test();

////////////////////////////////////////////////////////////////////////////////
int main( int argc, char* argv[] )
{
    //test();

    if( argc<3 ) {
        out << "usage: intergen <file>.hpp template-path\n";
        return -1;
    }

    charstr fdst = token(argv[1]).cut_left_back('.');
    fdst << ".inl";

    charstr tdir = argv[2];
    if(!directory::is_valid_directory(tdir)) {
        out << "invalid template path\n";
        return -2;
    }
    directory::treat_trailing_separator(tdir, true);

    charstr fdir = token(argv[1]).cut_left_back("\\/", token::cut_trait_return_with_sep_default_empty());

    File cgf;

    //parse
    int rv = cgf.parse(argv[1]);
    if(rv)  return rv;


    //generate
    generate_ig(cgf, tdir, fdir);
    generate_rl(cgf, tdir, fdst);

    out << "ok\n";

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
bool File::find_class( iglexer& lex, dynarray<charstr>& namespc, charstr& templarg )
{
    lexer::lextoken& tok = lex.last();
    int nested_curly = 0;

    do {
        lex.next();

        if( tok == lex.IFC1  ||  tok == lex.IFC2 ) {
            lex.complete_block();
            paste_block* pb = pasters.add();

            token t = tok;
            t.skip_space();
            pb->cond = t.get_line();

            pb->block = t;
            continue;
        }

        if( tok == '{' )
            ++nested_curly;
        else if( tok == '}' ) {
            if(nested_curly-- == 0) {
                //this should be namespace closing
                //namespc.resize(-(int)token(namespc).cut_right_back("::", token::cut_trait_keep_sep_with_returned()).len());
                namespc.pop();
            }
        }

        if( tok != lex.IDENT )  continue;

        if( tok == lex.NAMESPC ) {
            //namespc.reset();
            while( lex.next() ) {
                if( tok == '{' ) {
                    ++nested_curly;
                    break;
                }
                if( tok == ';' ) {
                    namespc.reset();    //it was 'using namespace ...'
                    break;
                }

                *namespc.add() = tok;
            }
        }

        if( tok == lex.TEMPL )
        {
            //read template arguments
            lex.match_block(lex.ANGLE, true);
            tok.swap_to_string(templarg);
        }

        if( tok == lex.CLASS || tok == lex.STRUCT )
            return true;
    }
    while(tok.id);

    return false;
}

////////////////////////////////////////////////////////////////////////////////
///Parse file
int File::parse( token path )
{
    bifstream bif(path);
    if(!bif.is_open()) {
        out << "error: can't open the file " << path << "\n";
        return -2;
    }

    iglexer lex;
    lex.bind(bif);
    lex.set_current_file(path);


    out << "processing " << path << " file ...\n";

    token name = path;
    name.cut_left_back("\\/", token::cut_trait_remove_sep_default_empty());
    
    fpath = path;
    fnameext = name;
    fname = name.cut_left_back('.');

    hdrname = fname;
    hdrname.replace('-', '_');
    hdrname.toupper();

    uint nm=0;
    uint ne=0;
    int mt;
    charstr templarg;
    dynarray<charstr> namespc;

    try {
        for( ; 0 != (mt=find_class(lex,namespc,templarg)); ++nm )
        {
            Class* pc = classes.add();
            if( !pc->parse(lex,templarg,namespc,&pasters,irefargs)  ||  pc->method.size() == 0 && pc->iface.size() == 0 )
                classes.resize(-1);
        }
    }
    catch( lexer::lexception& ) {}

    if( !lex.no_err() ) {
        out << lex.err() << '\n';
        out.flush();
        return -1;
    }

    if(pasters.size() > 0 && classes.size() == 0) {
        out << lex.prepare_exception() << "warning: ifc preprocessor tokens found, but no interface declared\n";
        lex.clear_err();
    }

    return 0;
}
