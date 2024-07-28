
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
// $(ProjectDir)..\..\..\comm_test\intergen\test.hpp $(ProjectDir)..\..\..\intergen\metagen

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
    dynarray<forward> fwds;
    dynarray<charstr> pastedefers;          //< pasted before the generated class definition

    friend metastream& operator || (metastream& m, File& p)
    {
        return m.compound_type(p, [&]()
        {
            int version = intergen_interface::VERSION;
            m.member("hdr", p.fnameext);          //< file name
            m.member("HDR", p.hdrname);           //< file name without extension, uppercase
            m.member("class", p.classes);
            m.member("pastedefers", p.pastedefers);
            m.member("fwds", p.fwds);
            m.nonmember("version", version);
        });
    }


    int parse(token path, const char* ref_file, int ref_line);

    bool find_class(iglexer& lex, dynarray<charstr>& namespc, charstr& templarg);
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
static int generate(bool empty, const T& t, const charstr& patfile, const charstr& outfile, __time64_t mtime)
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

    if (!mtg.parse(bit)) {
        //out << "error: error parsing the document template:\n";
        out << mtg.err() << '\n';
        out.flush();

        return -6;
    }

    bofstream bof(outfile);
    if (!bof.is_open()) {
        out << "error: can't create output file '" << outfile << "'\n";
        return -5;
    }

    out << "writing " << outfile << " ...\n";
    mtg.generate(t, bof);

    bof.close();

    directory::set_writable(outfile, false);
    directory::set_file_times(outfile, mtime + 2, mtime + 2);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
int generate_rl(const File& cgf, charstr& patfile, const token& outfile)
{
    uint l = patfile.len();
    patfile << "template.inl.mtg";

    bifstream bit(patfile);

    metagen mtg;
    mtg.set_source_path(patfile);

    patfile.resize(l);

    if (!bit.is_open()) {
        out << "error: can't open template file '" << patfile << "'\n";
        return -5;
    }

    if (!mtg.parse(bit)) {
        out << "error: error parsing the document template:\n";
        out << mtg.err();

        return -6;
    }

    bofstream bof(outfile);

    if (cgf.classes.size() > 0)
    {
        if (!bof.is_open()) {
            out << "error: can't create output file '" << outfile << "'\n";
            return -5;
        }

        out << "writing " << outfile << " ...\n";
        cgf.classes.for_each([&](const Class& c) {
            if (c.method.size())
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
    if (ifc.event.size() > 0 && ifc.varname.is_empty()) {
        out << "error: interface " << ifc.name << " has ifc_event methods and can be used only with bi-directional interfaces (ifc_class_var)\n";
        return false;
    }


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

    token reldir = token(ifc.relpath).cut_left_group_back("\\/", token::cut_trait_return_with_sep_default_empty());

    ifc.relpathjs = reldir;
    ifc.relpathjs << "js/" << ifc.name << ".h";

    ifc.relpathlua = reldir;
    ifc.relpathlua << "lua/" << ifc.name << ".h";

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

    fdir.resize(flen);

    if (!ifc.bdataifc) {
        //interface.js.h
        tdir.resize(tlen);
        tdir << "interface.js.h.mtg";

        if (generate(false, ifc, tdir, fdir + ifc.relpathjs, mtime) < 0)
            return false;

        //iterface.lua.h
        tdir.resize(tlen);
        tdir << "interface.lua.h.mtg";

        if (generate(false, ifc, tdir, fdir + ifc.relpathlua, mtime) < 0)
            return false;
    }

    // class interface docs
    tdir.resize(tlen);
    tdir << "interface.doc.mtg";

    fdir << reldir << "docs";
    directory::mkdir(fdir);

    fdir << '/' << ifc.name << ".html";

    return generate(false, ifc, tdir, fdir, mtime) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
void generate_ig(File& file, charstr& tdir, charstr& fdir)
{
    directory::treat_trailing_separator(tdir, directory::separator());
    uint tlen = tdir.len();

    directory::treat_trailing_separator(fdir, directory::separator());
    uint flen = fdir.len();

    //find the date of the oldest mtg file
    timet mtime = file.mtime;

    directory::list_file_paths(tdir, "mtg", directory::recursion_mode::immediate_files,
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

    //file.intergen.deps
    fdir.resize(flen);
    fdir << file.fname << ".intergen.deps";

    if (file.dependencies.size() > 0) {
        out << "writing " << fdir << " ...\n";

        charstr txt;
        for (const File& f : file.dependencies) {
            txt << f.fpath << "\r\n";
        }

        bofstream bof(fdir);
        bof.xwrite_token_raw(txt);
    }
    else {
        directory::delete_file(fdir);
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
    if (!directory::is_valid_directory(path)) {
        out << "failed to open " << path;
        return 0;
    }

    charstr before, after, single, multi;
    binstreambuf buf;
    int n = 0;

    const substring ssbeg = "/**interface metadata begin**/"_T;
    const substring ssend = "/**interface metadata end**/"_T;

    directory::list_file_paths(path, "html", directory::recursion_mode::immediate_files,
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
    directory::treat_trailing_separator(tdir, directory::separator());

    charstr fdir = token(argv[1]).cut_left_group_back("\\/", token::cut_trait_return_with_sep_default_empty());

    File cgf;

    //parse
    int rv = cgf.parse(argv[1], "intergen.cpp", __LINE__);
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
            pb->file = lex.get_current_file();
            pb->line = lex.current_line();

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
int File::parse(token path, const char* ref_file, int ref_line)
{
    directory::xstat st;

    bifstream bif;
    if (!directory::stat(path, &st) || bif.open(path) != 0) {
        charstr compact_path = path;
        directory::compact_path(compact_path, '/');
        if (ref_file)
            out << ref_file << '(' << ref_line << "): error: can't open file '" << compact_path << "'\n";
        else
            out << "error: can't open file '" << compact_path << "'\n";
        return -2;
    }

    mtime = st.st_mtime;

    charstr cpath = path;
    directory::compact_path(cpath, '\\');

    iglexer lex;
    lex.bind(bif);
    lex.set_current_file(cpath);


    out << "processing " << cpath << " file ...\n";

    token name = cpath;
    name.cut_left_group_back("\\/", token::cut_trait_remove_sep_default_empty());

    fpath = directory::get_cwd();
    directory::append_path(fpath, cpath);
    //fpath = path;
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

            if (!pc->parse(lex, templarg, namespc, &pasters) || (pc->method.size() == 0 && pc->iface_refc.size() == 0 && pc->iface_data.size() == 0)) {
                classes.resize(-1);
            }
        }

        for (Class& c : classes)
        {
            for (Interface& i : c.iface_refc)
            {
                if (i.bvirtualbase)
                    continue;

                if (!i.bextend_ext) {
                    //not an external extend
                    if (!i.bextend)
                        continue;

                    //find in the same class
                    i.base_ifc = c.iface_refc.find_if([&](Interface& ifc) {
                        return &ifc != &i && ifc.nsname == i.base;
                    });
                }

                charstr base_src_path;
                if (i.basesrc) {
                    base_src_path = token(cpath).cut_left_group_back(DIR_SEPARATORS, coid::token::cut_trait_remove_sep_default_empty());
                    if (!base_src_path.is_empty())
                        base_src_path << '\\';
                    directory::append_path(base_src_path, i.basesrc);
                    directory::compact_path(base_src_path, '\\');
                }

                Class* base_class = 0;
                Interface* base_ifc = 0;

                if (!i.bextend_ext) {
                    //find in the same class
                    base_src_path = fnameext;

                    base_ifc = c.iface_refc.find_if([&](Interface& ifc) {
                        return &ifc != &i && ifc.nsname == i.base;
                    });

                    if (base_ifc) {
                        base_class = &c;
                    }
                }
                else if (!i.basesrc || base_src_path == cpath) {
                    //find in other classes of the same file
                    base_src_path = fnameext;

                    for (Class& cls : classes)
                    {
                        if (&c == &cls)
                            continue;

                        base_ifc = cls.iface_refc.find_if([&](Interface& ifc) {
                            return ifc.nsname == i.base;
                        });

                        if (base_ifc) {
                            base_class = &cls;
                            break;
                        }
                    }
                }
                else {
                    //find in another file
                    out << "Parsing base class source file: " << i.basesrc << "\n";

                    File* base_file = dependencies.add();
                    int res = base_file->parse(base_src_path, i.file.c_str(), i.line);
                    if (res != 0) {
                        return res;
                    }

                    for (Class& cls : base_file->classes)
                    {
                        base_ifc = cls.iface_refc.find_if([&](Interface& ifc) {
                            return ifc.nsname == i.base;
                        });

                        if (base_ifc) {
                            base_class = &cls;
                            break;
                        }
                    }
                }

                if (!base_ifc) {
                    out << i.file << '(' << i.line << "): error: base class " << i.base << " not found in " << base_src_path << '\n';
                    nerr++;
                    break;
                }
                //else if (base_ifc->bvartype && i.bvartype) {
                //    out << i.file << '(' << i.line << "): error: extended interface `" << i.nsname << "' of ifc_class_var base interface `" << base_ifc->nsname << "' cannot use ifc_class_var\n";
                //    nerr++;
                //}

                uint nbase_events = (uint)base_ifc->event.size();
                MethodIG* base_events = i.event.ins(0, nbase_events);
                uint ntotal_events = (uint)i.event.size();

                for (uint bi = 0; bi < nbase_events; ++bi)
                {
                    const MethodIG& be = base_ifc->event[bi];

                    int found = -1;
                    for (uint x = nbase_events; x < ntotal_events; ++x) {
                        const MethodIG& it = i.event[x];
                        if (be.name == it.name && be.matches_args(it)) {
                            found = x;
                            break;
                        }
                    }

                    if (found >= 0) {
                        //found an override
                        base_events[bi] = std::move(i.event[found]);
                        i.event.del(found);
                        --ntotal_events;
                    }
                    else {
                        base_events[bi] = be;
                        //base_events[bi].fix_copy(be);
                        base_events[bi].binherit = true;
                    }
                }

                //creators are not inherited
                uint nbase_methods = 0;
                for (const MethodIG& m : base_ifc->method) { if (!m.bcreator) ++nbase_methods; }

                MethodIG* base_methods = i.method.ins(0, nbase_methods);
                uint ntotal_methods = (uint)i.method.size();

                int im = 0;
                for (uint bi = 0; bi < nbase_methods; ++bi)
                {
                    while (base_ifc->method[im].bcreator) ++im;
                    const MethodIG& bm = base_ifc->method[im++];

                    int found = -1;
                    for (uint x = nbase_methods; x < ntotal_methods; ++x) {
                        const MethodIG& it = i.method[x];
                        if (bm.bcreator == it.bcreator && bm.name == it.name && bm.matches_args(it)) {
                            found = x;
                            break;
                        }
                    }

                    if (found >= 0) {
                        //found an override
                        base_methods[bi] = std::move(i.method[found]);
                        i.method.del(found);
                        --ntotal_methods;
                    }
                    else {
                        base_methods[bi] = bm;
                        //base_methods[bi].fix_copy(bm);
                        base_methods[bi].binherit = true;
                    }
                }

                i.base_ifc = base_ifc;
            }
        }

        //check dependencies
        for (Class& c : classes)
        {
            for (Interface& i : c.iface_refc)
            {
                if (!i.bextend && !i.bextend_ext)
                    continue;

                if (!i.base_ifc)
                    continue;

                Interface* root = i.base_ifc;
                const charstr* varname = root->bvartype ? &root->varname : 0;
                while (root->base_ifc) {
                    root = root->base_ifc;
                    if (root->bvartype)
                        varname = &root->varname;
                }

                if (varname && i.bvartype) {
                    out << i.file << '(' << i.line << "): error: extended interface `" << i.nsname << "' of ifc_class_var base interface `" << i.base_ifc->nsname << "' cannot use ifc_class_var\n";
                    nerr++;
                }

                if (varname)
                    i.varname = *varname;
            }
        }
    }
    catch (lexer::lexception&) {}


    if (nerr || !lex.no_err()) {
        out << lex.err() << '\n';
        out.flush();
        return -1;
    }

    if (pasters.size() > 0 && classes.size() == 0) {
        out << (lex.prepare_exception() << "warning: ifc tokens found, but no interface declared\n");
        lex.clear_err();
    }

    for (const paste_block& pb : pasters) {
        if (pb.condx) {
            //check if used anywhere
            bool found = false;
            for (const Class& c : classes) {
                for (const Interface& i : c.iface_refc) {
                    if (i.nsname == pb.condx) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                out << pb.file << '(' << pb.line << "): error: interface " << pb.condx << " not found in file\n";
                ++nerr;
            }
        }

        if (pb.in_dispatch)
            pb.fill(*pastedefers.add());
    }

    return nerr ? -1 : 0;
}
