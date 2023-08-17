
#ifndef __INTERGEN__IG_H__
#define __INTERGEN__IG_H__

#include "../str.h"
#include "../dynarray.h"
#include "../lexer.h"
#include "../binstream/binstream.h"
#include "../binstream/stdstream.h"
#include "../metastream/metastream.h"

using namespace coid;

extern stdoutstream out;

struct Class;

////////////////////////////////////////////////////////////////////////////////
struct paste_block {
    charstr block;
    charstr condx;
    dynarray<charstr> namespc;

    enum class position {
        before_class,
        inside_class,
        after_class,
    };
    position pos = position::before_class;

    bool in_dispatch = false;

    void fill(charstr& dst) const {
        for (const charstr& ns : namespc)
            dst << "namespace "_T << ns << " {\r\n"_T;
        dst << block;
        for (uints n = namespc.size(); n > 0; --n)
            dst << "\r\n}"_T;
    }
};


////////////////////////////////////////////////////////////////////////////////
class iglexer : public lexer
{
public:
    int IDENT, NUM, CURLY, ROUND, SQUARE, ANGLE, SQSTRING, DQSTRING, RLCMD, IGKWD;
    int IFC_LINE_COMMENT, IFC_BLOCK_COMMENT, IFC_DISPATCH_LINE_COMMENT, IFC_DISPATCH_BLOCK_COMMENT;
    int SLCOM, MLCOM;

    inline static const token MARK = "rl_cmd";
    inline static const token MARKP = "rl_cmd_p";
    inline static const token MARKS = "rl_cmd_s";
    inline static const token CLASS = "class";
    inline static const token STRUCT = "struct";
    inline static const token TEMPL = "template";
    inline static const token NAMESPC = "namespace";

    inline static const token IFC_CLASS = "ifc_class";
    inline static const token IFC_CLASS_VAR = "ifc_class_var";
    inline static const token IFC_CLASS_EXTEND = "ifc_class_extend";
    inline static const token IFC_CLASS_VIRTUAL = "ifc_class_virtual";
    inline static const token IFC_STRUCT = "ifc_struct";
    inline static const token IFC_FN = "ifc_fn";
    inline static const token IFC_FNX = "ifc_fnx";
    inline static const token IFC_EVENT = "ifc_event";
    inline static const token IFC_EVENTX = "ifc_eventx";
    inline static const token IFC_EVBODY = "ifc_evbody";
    inline static const token IFC_DEFAULT_BODY = "ifc_default_body";
    inline static const token IFC_DEFAULT_EMPTY = "ifc_default_empty";
    inline static const token IFC_INOUT = "ifc_inout";
    inline static const token IFC_IN = "ifc_in";
    inline static const token IFC_OUT = "ifc_out";


    virtual void on_error_prefix(bool rules, charstr& dst, int line) override
    {
        if (!rules)
            dst << infile << char('(') << line << ") : ";
    }

    void set_current_file(const token& file) { infile = file; }
    const charstr& get_current_file() const { return infile; }

    iglexer();


    ///Find method mark within current class declaration
    int find_method(const token& classname, dynarray<paste_block>& classpasters, dynarray<charstr>& commlist);

    charstr& syntax_err() {
        prepare_exception() << "syntax error: ";
        return _errtext;
    }

    bool no_err() const {
        return _errtext.is_empty();
    }

private:
    charstr infile;
};

////////////////////////////////////////////////////////////////////////////////
struct Method
{
    ///Argument descriptor
    struct Arg
    {
        charstr type;                   //< parameter type (stripped of const qualifier and last reference)
        charstr name;                   //< parameter name
        charstr size;                   //< size expression if the parameter is an array, including [ ]
        charstr defval;
        char bptr;                      //< '*' if the type was pointer-type
        char bref;                      //< '&' if the type is a reference
        bool bretarg;                   //< true if this is the return argument
        bool bsizearg;                  //< true if this is the size argument
        bool bconst;                    //< true if the type had const qualifier


        bool parse(iglexer& lex);

        friend metastream& operator || (metastream& m, Arg& p)
        {
            return m.compound("Arg", [&]()
            {
                m.member("type", p.type);
                m.member("name", p.name);
                m.member("size", p.size);
                m.member("defval", p.defval);
                m.member("const", p.bconst);
                m.member("ptr", p.bptr);
                m.member("ref", p.bref);
                m.member("retarg", p.bretarg);
                m.member("sizearg", p.bsizearg);
            });
        }
    };

    charstr retexpr;                    //< expression for rl_cmd_p() for methods returning void* or T*
    charstr retparm;                    //< parameter name associated with the return expression (pointer-type)
    charstr sizeparm;                   //< parameter name that will receive command's payload length (bytes)
    charstr templarg;                   //< template arguments (optional)
    charstr templsub;                   //< template arguments for substitution
    charstr rettype;                    //< return type
    charstr name;                       //< method name
    charstr overload;

    bool bstatic;                       //< static method
    bool bsizearg;                      //< has size-argument
    bool bptr;                          //< ptr instead of ref
    bool biref;                         //< iref instead of ref

    dynarray<Arg> args;


    bool parse(iglexer& lex, int prefix);

    bool generate_h(binstream& bin);


    friend metastream& operator || (metastream& m, Method& p)
    {
        return m.compound("Method", [&]()
        {
            m.member("retexpr", p.retexpr);
            m.member("retparm", p.retparm);
            m.member("templarg", p.templarg);
            m.member("templsub", p.templsub);
            m.member("rettype", p.rettype);
            m.member("name", p.name);
            m.member("static", p.bstatic);
            m.member("sizearg", p.bsizearg);
            m.member("ptr", p.bptr);
            m.member("iref", p.biref);
            m.member("overload", p.overload);
            m.member("args", p.args);
        });
    }
};

////////////////////////////////////////////////////////////////////////////////
struct MethodIG
{
    ///Argument descriptor
    struct Arg
    {
        charstr type;                   //< parameter type (stripped of const qualifier)
        token basetype;                 //< base type (stripped of the last ptr/ref)
        token barenstype;               //< full bare type (without iref)
        token barens;                   //< namespace part of full bare type
        token baretype;                 //< type part of full bare type
        charstr name;                   //< parameter name
        charstr arsize;                 //< size expression if the parameter is an array, including [ ]
        charstr fnargs;                 //< argument list of a function-type argument
        charstr memfnclass;             //< member fn class
        charstr defval;
        charstr fulltype;
        charstr ifckwds;                //< ifc_out, ifc_inout and ifc_volatile string
        charstr doc;
        bool bspecptr = false;          //< special type where pointer is not separated (e.g const char*)
        bool bptr = false;              //< true if the type is a pointer
        bool bref = false;              //< true if the type is a reference
        bool bxref = false;             //< true if the type is xvalue reference
        bool biref = false;
        bool bconst = false;            //< true if the type had const qualifier
        bool benum = false;
        bool binarg = true;             //< input type argument
        bool boutarg = false;           //< output type argument
        bool bvolatile = false;
        bool tokenpar = false;          //< input argument that accepts token (token or charstr)
        bool bnoscript = false;         //< not used in scripts, use default val
        bool bfnarg = false;            //< function type arg


        bool operator == (const Arg& a) const {
            return fulltype == a.fulltype
                && arsize == a.arsize;
        }

        bool parse(iglexer& lex, bool argname);

        static charstr& match_type(iglexer& lex, charstr& dst);

        void add_unique(dynarray<Arg>& irefargs);

        friend metastream& operator || (metastream& m, Arg& p)
        {
            return m.compound("MethodIG::Arg", [&]()
            {
                m.member("type", p.type);
                m.member("basetype", p.basetype);
                m.member("barenstype", p.barenstype);
                m.member("barens", p.barens);
                m.member("baretype", p.baretype);
                m.member("name", p.name);
                m.member("size", p.arsize);
                m.member("fnargs", p.fnargs);
                m.member("memfnclass", p.memfnclass);
                m.member("defval", p.defval);
                m.member("fulltype", p.fulltype);
                m.member("ifckwds", p.ifckwds);
                m.member("doc", p.doc);
                m.member("const", p.bconst);
                m.member("enum", p.benum);
                m.member("specptr", p.bspecptr);
                m.member("ptr", p.bptr);
                m.member("ref", p.bref);
                m.member("xref", p.bxref);
                m.member("iref", p.biref);
                m.member("inarg", p.binarg);
                m.member("outarg", p.boutarg);
                m.member("volatile", p.bvolatile);
                m.member("token", p.tokenpar);
                m.member("nojs", p.bnoscript);
                m.member("fnarg", p.bfnarg);
            });
        }
    };

    charstr file;
    uint line = 0;

    charstr name;                       //< method name
    charstr intname;                    //< internal name
    charstr basename;                   //< method name, changed operator name
    charstr storage;                    //< storage for host class, iref<type>, ref<type> or type*
    charstr default_event_body;

    int index = -1;

    bool bstatic = false;               //< a static (creator) method
    bool bcreator = false;
    bool bptr = false;                  //< ptr instead of ref
    bool biref = true;                  //< iref instead of ref
    bool bifccr = false;                //< ifc returning creator (not host)
    bool bconst = false;                //< const method
    bool boperator = false;
    bool binternal = false;             //< internal method, invisible to scripts (starts with an underscore or ifc_fn(!))
    bool bcapture = false;              //< method captured when interface is in capturing mode
    bool bimplicit = false;             //< an implicit event/method
    bool bdestroy = false;              //< a method to call on interface destroy
    bool bnoevbody = false;             //< mandatory event
    bool bpure = false;                 //< pure virtual on client
    bool bduplicate = false;            //< a duplicate method/event from another interface of the host
    bool binherit = false;              //< method inherited from base interface

    Arg ret;
    dynarray<Arg> args;

    int ninargs = 0;                    //< number of input arguments
    int ninargs_nondef = 0;
    int noutargs = 0;                   //< number of output arguments

    dynarray<charstr> comments;         //< comments preceding the method declaration
    dynarray<charstr> docs;             //< doc paragraphs


    bool parse(iglexer& lex, const charstr& host, const charstr& ns, const charstr& nsifc, dynarray<Arg>& irefargs, bool isevent);

    void parse_docs();

    Arg* find_arg(const coid::token& name) {
        return args.find_if([&](const Arg& a) {
            return a.name == name;
        });
    }

    bool matches_args(const MethodIG& m) const {
        if (ninargs != m.ninargs || noutargs != m.noutargs)
            return false;

        const Arg* a1 = args.ptr();
        const Arg* a2 = m.args.ptr();
        const Arg* e1 = args.ptre();

        for (; a1 < e1; ++a1, ++a2) {
            if (!(*a1 == *a2)) break;
        }

        return a1 == e1;
    }

    bool generate_h(binstream& bin);

    friend metastream& operator || (metastream& m, MethodIG& p)
    {
        return m.compound("MethodIG", [&]()
        {
            m.member("return", p.ret);
            m.member("name", p.name);
            m.member("intname", p.intname);
            m.member("basename", p.basename);
            m.member("storage", p.storage);
            m.member("operator", p.boperator);
            m.member("internal", p.binternal);
            m.member("capture", p.bcapture);
            m.member("static", p.bstatic);
            m.member("creator", p.bcreator);
            m.member("ptr", p.bptr);
            m.member("iref", p.biref);
            m.member("ifccr", p.bifccr);
            m.member("const", p.bconst);
            m.member("implicit", p.bimplicit);
            m.member("destroy", p.bdestroy);
            m.member("noevbody", p.bnoevbody);
            m.member("pure", p.bpure);
            m.member("duplicate", p.bduplicate);
            m.member("inherit", p.binherit);
            m.member("args", p.args);
            m.member("ninargs", p.ninargs);
            m.member("ninargs_nondef", p.ninargs_nondef);
            m.member("noutargs", p.noutargs);
            m.member("comments", p.comments);
            m.member("docs", p.docs);
            m.member("index", p.index);
            m.member("default_event_body", p.default_event_body);
        });
    }
};


///
struct Interface
{
    charstr file;
    uint line = 0;
    Interface* base_ifc = 0;

    dynarray<charstr> nss;
    charstr name;
    charstr nsname;
    charstr relpath, relpathjs, relpathlua;
    charstr hdrfile;
    charstr storage;
    charstr varname;

    charstr base;    // base class name with namespaces
    charstr basesrc; // path of base class source file (relative to the file this ifc is generated from)
    token baseclass; // only the base class name
    token basepath;

    dynarray<charstr> baseclassnss; // base class namespaces

    dynarray<MethodIG> method;
    dynarray<MethodIG> event;

    MethodIG destroy;
    charstr default_creator;

    int oper_get = -1;
    int oper_set = -1;

    MethodIG::Arg getter, setter;

    charstr on_connect, on_connect_ev;
    charstr on_unload;

    //uint nifc_methods = 0;

    dynarray<charstr> pasters;              //< pasted before the generated class declaration
    dynarray<charstr> pasteafters;          //< pasted after the generated class declaration
    dynarray<charstr> pasteinners;          //< pasted inside the generated class declaration

    charstr* srcfile = 0;
    charstr* srcclass = 0;
    charstr* srcclassorstruct = 0;
    dynarray<charstr>* srcnamespc = 0;

    uint hash;


    dynarray<charstr> comments;
    dynarray<charstr> docs;

    bool bvirtual = false;
    bool bdefaultcapture = false;
    bool bnoscript = false;
    bool bdataifc = false;
    bool bextend = false;
    bool bextend_ext = false;
    bool bfinal = false;
    bool bvartype = false;

    void copy_methods(Interface& o)
    {
        varname = o.varname;

        method = o.method;
        event = o.event;

        for (auto& m : method)
            m.binherit = true;
        for (auto& e : event)
            e.binherit = true;

        destroy = o.destroy;
        destroy.binherit = true;

        default_creator = o.default_creator;

        oper_get = o.oper_get;
        oper_set = o.oper_set;
        getter = o.getter;
        setter = o.setter;

        on_connect = o.on_connect;
        on_connect_ev = o.on_connect_ev;
    }

    void compute_hash(int version);

    void parse_docs();

    bool full_name_equals(token name) const {
        bool hasns = nss.find_if([&name](const charstr& v) {
            return !(name.consume(v) && name.consume("::"));
        }) == 0;
        if (hasns && name.consume(this->name))
            return name.is_empty();
        return false;
    }

    static bool has_mismatched_method(const MethodIG& m, const dynarray<MethodIG>& methods) {
        int nmatch = 0, nmiss = 0;
        methods.for_each([&](const MethodIG& ms) {
            if (m.name == ms.name) {
                ++nmatch;
                if (!m.matches_args(ms))
                    ++nmiss;
            }
        });

        return nmatch > 0 && nmiss == nmatch;
    }

    int check_interface(iglexer& lex, const dynarray<paste_block>& classpasters);

    friend metastream& operator || (metastream& m, Interface& p)
    {
        return m.compound("Interface", [&]()
        {
            m.member("ns", p.nss);
            m.member("name", p.name);
            m.member("relpath", p.relpath);
            m.member("relpathjs", p.relpathjs);
            m.member("relpathlua", p.relpathlua);
            m.member("hdrfile", p.hdrfile);
            m.member("storage", p.storage);
            m.member("method", p.method);
            m.member("getter", p.getter);
            m.member("setter", p.setter);
            m.member("onconnect", p.on_connect);
            m.member("onconnectev", p.on_connect_ev);
            m.member("onunload", p.on_unload);
            m.member_type<bool>("hasprops", [](bool) {}, [&]() { return p.oper_get >= 0; });
            //m.member("nifcmethods", p.nifc_methods);
            m.member("varname", p.varname);
            m.member("event", p.event);
            m.member("destroy", p.destroy);
            m.member("hash", p.hash);
            //m.member("inhmask", p.inhmask);
            //m.member("ifc_bit", p.ifc_bit);
            m.member("comments", p.comments);
            m.member("docs", p.docs);
            m.member("pasters", p.pasters);
            m.member("pasteafters", p.pasteafters);
            m.member("pasteinners", p.pasteinners);
            m.member_indirect("srcfile", p.srcfile);
            m.member_indirect("class", p.srcclass);
            m.member_indirect("classnsx", p.srcnamespc);
            m.member_indirect("classorstruct", p.srcclassorstruct);
            m.member("base", p.base);
            m.member("baseclass", p.baseclass);
            m.member("basepath", p.basepath);
            m.member("baseclassns", p.baseclassnss);
            m.member("virtual", p.bvirtual);
            m.member("dataifc", p.bdataifc);
            m.member("default_creator", p.default_creator);
            m.member("final", p.bfinal);

        });
    }
};


///
struct Class
{
    charstr classorstruct;
    charstr classname;
    charstr templarg;
    charstr templsub;
    charstr namespc;                    //< namespace with the trailing :: (if not empty)
    charstr ns;                         //< namespace without the trailing ::
    bool noref = true;
    bool datahost = false;              //< data-type host

    dynarray<charstr> namespaces;
    dynarray<Method> method;
    dynarray<Interface> iface_refc;
    dynarray<Interface> iface_data;

    dynarray<paste_block> classpasters;


    bool parse(iglexer& lex, charstr& templarg_, const dynarray<charstr>& namespcs, dynarray<paste_block>* pasters, dynarray<MethodIG::Arg>& irefargs);

    friend metastream& operator || (metastream& m, Class& p)
    {
        return m.compound("Class", [&]()
        {
            m.member("classorstruct", p.classorstruct);
            m.member("class", p.classname);
            m.member("templarg", p.templarg);
            m.member("templsub", p.templsub);
            m.member("ns", p.ns);
            m.member("nsx", p.namespc);
            m.member("noref", p.noref);
            m.member("datahost", p.datahost);
            m.member("method", p.method);
            m.member("iface", p.iface_refc);
            m.member("iface_data", p.iface_data);
            m.member("nss", p.namespaces);
        });
    }
};

#endif //__INTERGEN__IG_H__
