
#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../binstream/enc_base64stream.h"
#include "../metastream/metagen.h"
#include "../metastream/fmtstreamcxx.h"
#include "../dir.h"
#include "../intergen/ifc.h"

#include "../hash/hashkeyset.h"
#include "ig.h"

//debug string:
// [inputs] $(ProjectDir)..\..\..\intergen\metagen
// $(ProjectDir)..\..\..\intergen\test\test.hpp $(ProjectDir)..\..\..\intergen\metagen

stdoutstream out;

////////////////////////////////////////////////////////////////////////////////
///
struct File
{
    charstr fpath, fnameext;
    charstr hdrname;
    charstr fname;

    timet mtime;

    dynarray<Class> classes;
    dynarray<File> dependencies;

    dynarray<paste_block> pasters;
    dynarray<MethodIG::Arg> irefargs;

    dynarray<charstr> pastedefers;          //< pasted before the generated class definition

    friend metastream& operator || (metastream& m, File& p)
    {
        return m.compound("File", [&]()
        {
            int version = intergen_interface::VERSION;
            m.member("hdr",p.fnameext);          //< file name
            m.member("HDR",p.hdrname);           //< file name without extension, uppercase
            m.member("class",p.classes);
            m.member("pastedefers", p.pastedefers);
            m.member("irefargs",p.irefargs);
            m.nonmember("version",version);
        });
    }


    int parse(token path);

    bool find_class( iglexer& lex, dynarray<charstr>& namespc, charstr& templarg );
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
static int generate( bool empty, const T& t, const charstr& patfile, const charstr& outfile, __time64_t mtime )
{
    directory::xstat st;
    bifstream bit;

    if (!directory::stat(patfile, &st) || bit.open(patfile) != 0) {
        out << "error: can't open template file '" << patfile << "'\n";
        return -5;
    }

    if (st.st_mtime > mtime)
        mtime = st.st_mtime;

    if (empty) {
        //create an empty file to satisfy dependency checker
        directory::set_writable(outfile, true);
        directory::truncate(outfile, 0);
        directory::set_file_times(outfile, mtime + 2, mtime + 2);
        directory::set_writable(outfile, false);
        return 0;
    }

    directory::mkdir_tree(outfile, true);
    directory::set_writable(outfile, true);

    metagen mtg;
    mtg.set_source_path(patfile);

    if( !mtg.parse(bit) ) {
        //out << "error: error parsing the document template:\n";
        out << mtg.err() << '\n';
        out.flush();

        return -6;
    }

    bofstream bof(outfile);
    if( !bof.is_open() ) {
        out << "error: can't create output file '" << outfile << "'\n";
        return -5;
    }

    out << "writing " << outfile << " ...\n";
    mtg.generate(t, bof);

    bof.close();

    directory::set_writable(outfile, false);
    directory::set_file_times(outfile, mtime+2, mtime+2);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
int generate_rl( const File& cgf, charstr& patfile, const token& outfile )
{
    uint l = patfile.len();
    patfile << "template.inl.mtg";

    bifstream bit(patfile);

    metagen mtg;
    mtg.set_source_path(patfile);

    patfile.resize(l);

    if( !bit.is_open() ) {
        out << "error: can't open template file '" << patfile << "'\n";
        return -5;
    }

    if( !mtg.parse(bit) ) {
        out << "error: error parsing the document template:\n";
        out << mtg.err();

        return -6;
    }

    bofstream bof(outfile);

    if( cgf.classes.size() > 0 )
    {
        if( !bof.is_open() ) {
            out << "error: can't create output file '" << outfile << "'\n";
            return -5;
        }

        out << "writing " << outfile << " ...\n";
        cgf.classes.for_each([&](const Class& c) {
            if(c.method.size())
                mtg.generate(c, bof);
        });
    }
    else
        out << "no rl_cmd's found\n";

    __time64_t mtime = cgf.mtime + 2;
    directory::set_file_times(outfile, mtime, mtime);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
bool generate_ifc(File& file, Class& cls, Interface& ifc, timet mtime, charstr& tdir, charstr& fdir)
{
    uint tlen = tdir.len();
    uint flen = fdir.len();

    ifc.compute_hash(intergen_interface::VERSION);

    //interface.h
    token end;

    if (ifc.relpath.ends_with_icase(end = ".hpp") || ifc.relpath.ends_with_icase(end = ".hxx") || ifc.relpath.ends_with_icase(end = ".h"))
        fdir << ifc.relpath;    //contains the file name already
    else if (ifc.relpath.last_char() == '/' || ifc.relpath.last_char() == '\\')
        fdir << ifc.relpath << ifc.name << ".h";
    else if (ifc.relpath)
        fdir << ifc.relpath << '/' << ifc.name << ".h";
    else
        fdir << ifc.name << ".h";

    ifc.relpath.set_from_range(fdir.ptr() + flen, fdir.ptre());
    directory::compact_path(ifc.relpath);
    ifc.relpath.replace('\\', '/');

    ifc.relpathjs = ifc.relpath;
    ifc.relpathjs.ins(-(int)end.len(), ".js");

    ifc.relpathlua = ifc.relpath;
    ifc.relpathlua.ins(-(int)end.len(), ".lua");

    ifc.basepath = ifc.relpath;
    ifc.hdrfile = ifc.basepath.cut_right_back('/', token::cut_trait_keep_sep_with_source_default_full());

    ifc.srcfile = &file.fnameext;
    ifc.srcclass = &cls.classname;
    ifc.srcnamespc = &cls.namespaces;
    ifc.srcclassorstruct = &cls.classorstruct;

    uints nm = ifc.method.size();
    for (uints m = 0; m < nm; ++m) {
        if (ifc.method[m].bstatic)
            ifc.storage = ifc.method[m].storage;
    }

    tdir << (ifc.bdataifc ? "interface-data.h.mtg" : "interface.h.mtg");

    if (generate(false, ifc, tdir, fdir, mtime) < 0)
        return false;

    if (!ifc.bdataifc) {
        //interface.js.h
        fdir.resize(flen);
        fdir << ifc.relpathjs;

        tdir.resize(tlen);
        tdir << "interface.js.h.mtg";

        if (generate(false, ifc, tdir, fdir, mtime) < 0)
            return false;

        //iterface.lua.h
        tdir.resize(tlen);
        tdir << "interface.lua.h.mtg";

        fdir.resize(flen);
        fdir << ifc.relpathlua;

        if (generate(false, ifc, tdir, fdir, mtime) < 0)
            return false;
    }

    // class interface docs
    tdir.resize(tlen);
    tdir << "interface.doc.mtg";

    fdir.resize(-int(token(fdir).cut_right_group_back("\\/").len()));
    fdir << "/docs";
    directory::mkdir(fdir);

    fdir << '/' << ifc.name << ".html";

    return generate(false, ifc, tdir, fdir, mtime) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
void generate_ig(File& file, charstr& tdir, charstr& fdir)
{
    directory::treat_trailing_separator(tdir, true);
    uint tlen = tdir.len();

    directory::treat_trailing_separator(fdir, true);
    uint flen = fdir.len();

    //find the date of the oldest mtg file
    timet mtime = file.mtime;

    directory::list_file_paths(tdir, "mtg", directory::recursion_mode::files,
        [&](const charstr& name, directory::list_entry) {
        directory::xstat st;
        if (directory::stat(name, &st))
            if (st.st_mtime > mtime)
                mtime = st.st_mtime;
    });

    int nifc = 0;

    uints nc = file.classes.size();
    for (uints c = 0; c < nc; ++c)
    {
        Class& cls = file.classes[c];

        //refc interfaces
        for (Interface& ifc : cls.iface_refc) {
            fdir.resize(flen);
            tdir.resize(tlen);

            if (!generate_ifc(file, cls, ifc, mtime, tdir, fdir))
                return;

            ++nifc;
        }

        //data interfaces
        for (Interface& ifc : cls.iface_data) {
            fdir.resize(flen);
            tdir.resize(tlen);

            if (!generate_ifc(file, cls, ifc, mtime, tdir, fdir))
                return;

            ++nifc;
        }
    }

    //file.intergen.cpp
    fdir.resize(flen);
    fdir << file.fname << ".intergen.cpp";

    tdir.resize(tlen);
    tdir << "file.intergen.cpp.mtg";

    generate(nifc == 0, file, tdir, fdir, mtime);

    //file.intergen.js.cpp
    fdir.ins(-4, ".js");
    tdir.ins(-8, ".js");

    generate(nifc == 0, file, tdir, fdir, mtime);

    //file.intergen.lua.cpp
    fdir.resize(flen);
    fdir << file.fname << ".intergen.lua.cpp";

    tdir.resize(tlen);
    tdir << "file.intergen.lua.cpp.mtg";

    generate(nifc == 0, file, tdir, fdir, mtime);

    tdir.resize(tlen);
    fdir.resize(flen);
}

void test();

////////////////////////////////////////////////////////////////////////////////
int generate_index(const charstr& path)
{
    if(!directory::is_valid_directory(path)) {
        out << "failed to open " << path;
        return 0;
    }

    charstr before, after, single, multi;
    binstreambuf buf;
    int n = 0;

    const substring ssbeg = "/**interface metadata begin**/"_T;
    const substring ssend = "/**interface metadata end**/"_T;

    directory::list_file_paths(path, "html", directory::recursion_mode::files,
        [&](const charstr& name, directory::list_entry) {
            if (name.ends_with("index.html"_T))
                return;

            bifstream bif(name);
            if (!bif.is_open()) {
                out << "can't open " << name << '\n';
                return;
            }
            buf.reset_all();
            buf.transfer_from(bif);
            bif.close();

            token text = buf;
            token lead = text.cut_left(ssbeg);

            if (!before)
                before = lead;

            single = text.cut_left(ssend);

            if (single) {
                if (multi)
                    multi << ", \r\n";
                multi << single;
            }

            if (!after)
                after = text;

            ++n;
        });

    if (before) {
        out << "found " << n << " interface files, generating index.html ... ";
        out.flush();

        charstr dpath = path;
        directory::append_path(dpath, "index.html");

        bofstream bof(dpath);
        if (!bof.is_open()) {
            out << "failed\nError writing " << dpath << '\n';
            return 0;
        }

        bof.xwrite_token_raw(before);
        bof.xwrite_token_raw(multi);
        bof.xwrite_token_raw(after);

        bof.close();

        out << "ok\n";
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    //test();
    if (argc >= 2 && token(argv[1]) == "-index") {
        //generate index from existing html files
        charstr path;
        if (argc > 2)
            path = argv[2];
        else
            path = directory::get_cwd();

        return generate_index(path);
    }

    if (argc < 3) {
        out << "usage: intergen <file>.hpp template-path\n";
        return -1;
    }

    charstr fdst = token(argv[1]).cut_left_back('.');
    fdst << ".inl";

    charstr tdir = argv[2];
    if (!directory::is_valid_directory(tdir)) {
        out << "invalid template path\n";
        return -2;
    }
    directory::treat_trailing_separator(tdir, true);

    charstr fdir = token(argv[1]).cut_left_group_back("\\/", token::cut_trait_return_with_sep_default_empty());

    File cgf;

    //parse
    int rv = cgf.parse(argv[1]);
    if (rv)
        return rv;


    //generate
    generate_ig(cgf, tdir, fdir);
    generate_rl(cgf, tdir, fdst);

    out << "ok\n";

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
bool File::find_class(iglexer& lex, dynarray<charstr>& namespc, charstr& templarg)
{
    const lexer::lextoken& tok = lex.last();

    do {
        lex.next();

        bool dispatch_comment = tok == lex.IFC_DISPATCH_LINE_COMMENT || tok == lex.IFC_DISPATCH_BLOCK_COMMENT;

        if (dispatch_comment || tok == lex.IFC_LINE_COMMENT || tok == lex.IFC_BLOCK_COMMENT) {
            lex.complete_block();
            paste_block* pb = pasters.add();

            token t = tok;
            t.skip_space().trim_whitespace();
            token cond = t.get_line();
            pb->block = t;
            pb->namespc = namespc;
            pb->pos = cond.consume_end_char('+') ? paste_block::position::after_class : paste_block::position::before_class;
            pb->in_dispatch = dispatch_comment;
            pb->condx = cond;

            continue;
        }

        if (tok == '{') {
            namespc.push(charstr());
            //++nested_curly;
        }
        else if (tok == '}') {
            namespc.pop();
        }

        if (tok != lex.IDENT)  continue;

        if (tok == lex.NAMESPC) {
            charstr* ns = 0;

            while (lex.next()) {
                if (tok == "::"_T && ns) {
                    *ns << tok;
                    continue;
                }
                else if (tok == '{') {
                    //++nested_curly;
                    break;
                }
                else if (tok == ';') {
                    //it was 'using namespace ...'
                    namespc.pop();
                    break;
                }
                else if (!ns)
                    ns = namespc.add();

                *ns << tok;
            }
        }

        if (tok == lex.TEMPL)
        {
            //read template arguments
            lex.match_block(lex.ANGLE, true);
            templarg = tok;
        }

        if (tok == lex.CLASS || tok == lex.STRUCT)
            return true;
    }
    while (tok.id);

    return false;
}

////////////////////////////////////////////////////////////////////////////////
///Parse file
int File::parse(token path)
{
    directory::xstat st;

    bifstream bif;
    if (!directory::stat(path, &st) || bif.open(path) != 0) {
        out << "error: can't open the file " << path << "\n";
        return -2;
    }

    mtime = st.st_mtime;

    iglexer lex;
    lex.bind(bif);
    lex.set_current_file(path);


    out << "processing " << path << " file ...\n";

    token name = path;
    name.cut_left_group_back("\\/", token::cut_trait_remove_sep_default_empty());

    fpath = path;
    fnameext = name;
    fname = name.cut_left_back('.');

    hdrname = fname;
    hdrname.replace('-', '_');
    hdrname.toupper();

    int mt;
    charstr templarg;
    dynarray<charstr> namespc;

    int nerr = 0;

    try {
        for (; 0 != (mt = find_class(lex, namespc, templarg));)
        {
            Class* pc = classes.add();
            pc->classorstruct = lex.last();

            if (!pc->parse(lex, templarg, namespc, &pasters, irefargs) || (pc->method.size() == 0 && pc->iface_refc.size() == 0 && pc->iface_data.size() == 0)) {
                classes.resize(-1);
            }
        }
    }
    catch (lexer::lexception&) {}

    for (Class& c : classes)
    {
        for (Interface& i : c.iface_refc)
        {
            if (i.bdirect_inheritance) 
            {
                charstr base_src_path = token(path).cut_left_group_back(DIR_SEPARATORS, coid::token::cut_trait_remove_sep_default_empty());
                if (!base_src_path.is_empty()) 
                {
                    base_src_path << "/";
                }
                base_src_path << i.basesrc;

                out << "Parsing base class source file: " << i.basesrc << "\n";
 
                File* base_file = dependencies.add();
                int res = base_file->parse(base_src_path);
                if (res != 0) 
                {
                    return res;
                }

                for(Class& cls : base_file->classes)
                {
                    Interface* found = cls.iface_refc.find_if([&](Interface& ifc) {
                        return ifc.nsname == i.base;
                        });

                    if (found) 
                    {
                        for (const MethodIG& e : found->event) 
                        {
                            const MethodIG * found = i.event.find_if([&e](const MethodIG& it) {
                                return e.name == it.name && e.intname == it.intname;
                            });

                            if (!found) 
                            {
                                i.event.push(e);
                                i.event.last()->binherit = true;
                            }
                        }

                        for (const MethodIG& m : found->method)
                        {
                            const MethodIG* found = i.event.find_if([&m](const MethodIG& it) {
                                return m.name == it.name && m.intname == it.intname;
                                });

                            if (!found && !m.bcreator)
                            {
                                i.method.push(m);
                                i.method.last()->binherit = true;
                            }
                        }

                        i.varname = found->varname;
                    }
                    else
                    {
                        out << "Base class not found in " << i.basesrc << "\n";
                        return -1;
                    }
                }

            }
        }
    }

    if (!lex.no_err()) {
        out << lex.err() << '\n';
        out.flush();
        return -1;
    }

    if (!nerr && pasters.size() > 0 && classes.size() == 0) {
        out << (lex.prepare_exception()
            << "warning: ifc tokens found, but no interface declared\n");
        lex.clear_err();
    }

    for (const paste_block& pb : pasters) {
        if (pb.in_dispatch)
            pb.fill(*pastedefers.add());
    }

    return 0;
}
