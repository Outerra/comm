
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

////////////////////////////////////////////////////////////////////////////////
class iglexer : public lexer
{
public:
    int IDENT,NUM,CURLY,ROUND,SQUARE,ANGLE,SQSTRING,DQSTRING,RLCMD,IGKWD;
    int IFC1,IFC2,SLCOM,MLCOM;

    static const token MARK;
    static const token MARKP;
    static const token MARKS;
    static const token CLASS;
    static const token STRUCT;
    static const token TEMPL;
    static const token NAMESPC;

    static const token IFC_CLASS;
    static const token IFC_CLASS_VAR;
    static const token IFC_CLASS_VIRTUAL;
    static const token IFC_FN;
    static const token IFC_FNX;
    static const token IFC_EVENT;
    static const token IFC_EVENTX;
    static const token IFC_INOUT;
    static const token IFC_IN;
    static const token IFC_OUT;


    virtual void on_error_prefix( bool rules, charstr& dst, int line ) override
    {
        if(!rules)
            dst << infile << char('(') << line << ") : ";
    }

    void set_current_file( const token& file ) {
        infile = file;
    }

    iglexer();


    ///Find method mark within current class declaration
    int find_method( const token& classname, dynarray<charstr>& commlist );

    charstr& set_err() {
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
struct paste_block {
    charstr block;
    charstr cond;
};

////////////////////////////////////////////////////////////////////////////////
struct Method
{
    ///Argument descriptor
    struct Arg
    {
        charstr type;                   ///< parameter type (stripped of const qualifier and last reference)
        charstr name;                   ///< parameter name
        charstr size;                   ///< size expression if the parameter is an array, including [ ]
        charstr defval;
        char bptr;                      ///< '*' if the type was pointer-type
        char bref;                      ///< '&' if the type is a reference
        bool bretarg;                   ///< true if this is the return argument
        bool bsizearg;                  ///< true if this is the size argument
        bool bconst;                    ///< true if the type had const qualifier


        bool parse( iglexer& lex );

        friend binstream& operator << (binstream& bin, const Arg& m)
        {   return bin << m.type << m.name << m.size << m.defval << m.bconst << m.bptr << m.bref << m.bretarg << m.bsizearg; }
        friend binstream& operator >> (binstream& bin, Arg& m)
        {   return bin >> m.type >> m.name >> m.size >> m.defval >> m.bconst >> m.bptr >> m.bref >> m.bretarg >> m.bsizearg; }
        friend metastream& operator << (metastream& m, const Arg& p)
        {
            MSTRUCT_OPEN(m,"Arg")
            MM(m,"type",p.type)
            MM(m,"name",p.name)
            MM(m,"size",p.size)
            MM(m,"defval",p.defval)
            MM(m,"const",p.bconst)
            MM(m,"ptr",p.bptr)
            MM(m,"ref",p.bref)
            MM(m,"retarg",p.bretarg)
            MM(m,"sizearg",p.bsizearg)
            MSTRUCT_CLOSE(m)
        }
    };

    charstr retexpr;                    ///< expression for rl_cmd_p() for methods returning void* or T*
    charstr retparm;                    ///< parameter name associated with the return expression (pointer-type)
    charstr sizeparm;                   ///< parameter name that will receive command's payload length (bytes)
    charstr templarg;                   ///< template arguments (optional)
    charstr templsub;                   ///< template arguments for substitution
    charstr rettype;                    ///< return type
    charstr name;                       ///< method name
    charstr overload;

    bool bstatic;                       ///< static method
    bool bsizearg;                      ///< has size-argument
    bool bptr;                          ///< ptr instead of ref
    bool biref;                         ///< iref instead of ref

    dynarray<Arg> args;


    bool parse( iglexer& lex, int prefix );

    bool generate_h( binstream& bin );


    friend binstream& operator << (binstream& bin, const Method& m)
    {   return bin << m.retexpr << m.retparm << m.templarg << m.templsub << m.rettype
    << m.name << m.bstatic << m.bsizearg << m.bptr << m.biref << m.overload << m.args; }
    friend binstream& operator >> (binstream& bin, Method& m)
    {   return bin >> m.retexpr >> m.retparm >> m.templarg >> m.templsub >> m.rettype
    >> m.name >> m.bstatic >> m.bsizearg >> m.bptr >> m.biref >> m.overload >> m.args; }
    friend metastream& operator << (metastream& m, const Method& p)
    {
        MSTRUCT_OPEN(m,"Method")
        MM(m,"retexpr",p.retexpr)
        MM(m,"retparm",p.retparm)
        MM(m,"templarg",p.templarg)
        MM(m,"templsub",p.templsub)
        MM(m,"rettype",p.rettype)
        MM(m,"name",p.name)
        MM(m,"static",p.bstatic)
        MM(m,"sizearg",p.bsizearg)
        MM(m,"ptr",p.bptr)
        MM(m,"iref",p.biref)
        MM(m,"overload",p.overload)
        MM(m,"args",p.args)
        MSTRUCT_CLOSE(m)
    }
};

////////////////////////////////////////////////////////////////////////////////
struct MethodIG
{
    ///Argument descriptor
    struct Arg
    {
        charstr type;                   ///< parameter type (stripped of const qualifier)
        token basetype;                 ///< base type (stripped of the last ptr/ref)
        charstr base2arg;               ///< suffix to convert from base (storage) type to type parameter
        charstr name;                   ///< parameter name
        charstr size;                   ///< size expression if the parameter is an array, including [ ]
        charstr defval;
        charstr fulltype;              
        bool bptr;                      ///< true if the type is a pointer
        bool bref;                      ///< true if the type is a reference
        bool biref;
        bool bconst;                    ///< true if the type had const qualifier
        bool benum;
        bool binarg;                    ///< input type argument
        bool boutarg;                   ///< output type argument
        bool tokenpar;                  ///< input argument that accepts token (token or charstr)
        bool bnojs;                     ///< not used in JS, use default val


        Arg()
            : bptr(false), bref(false), biref(false), bconst(false), binarg(true), boutarg(false), tokenpar(false)
        {}

        bool parse( iglexer& lex, bool argname );

        void add_unique( dynarray<Arg>& irefargs );


        friend binstream& operator << (binstream& bin, const Arg& m)
        {   return bin << m.type << m.basetype << m.base2arg << m.name << m.size << m.defval << m.fulltype
            << m.bconst << m.benum << m.bptr << m.bref << m.biref << m.binarg << m.boutarg << m.tokenpar << m.bnojs; }
        friend binstream& operator >> (binstream& bin, Arg& m)
        {   return bin; }
        friend metastream& operator << (metastream& m, const Arg& p)
        {
            MSTRUCT_OPEN(m,"MethodIG::Arg")
            MM(m,"type",p.type)
            MM(m,"basetype",p.basetype)
            MM(m,"base2arg",p.base2arg)
            MM(m,"name",p.name)
            MM(m,"size",p.size)
            MM(m,"defval",p.defval)
            MM(m,"fulltype",p.fulltype)
            MM(m,"const",p.bconst)
            MM(m,"enum",p.benum)
            MM(m,"ptr",p.bptr)
            MM(m,"ref",p.bref)
            MM(m,"iref",p.biref)
            MM(m,"inarg",p.binarg)
            MM(m,"outarg",p.boutarg)
            MM(m,"token",p.tokenpar)
            MM(m,"nojs",p.bnojs)
            MSTRUCT_CLOSE(m)
        }
    };

    charstr templarg;                   ///< template arguments (optional)
    charstr templsub;                   ///< template arguments for substitution
    charstr name;                       ///< method name
    charstr intname;                    ///< internal name
    charstr storage;                    ///< storage for host class, iref<type>, ref<type> or type*

    int index;

    bool bstatic;                       ///< a static (creator) method
    bool bptr;                          ///< ptr instead of ref
    bool biref;                         ///< iref instead of ref
    bool bconst;                        ///< const method
    bool boperator;
    bool binternal;                     ///< internal method, invisible to scripts (starts with an underscore)
    bool bcapture;                      ///< method captured when interface is in capturing mode
    bool bimplicit;                     ///< an implicit event
    bool bdestroy;                      ///< a method to call on interface destroy

    Arg ret;
    dynarray<Arg> args;

    int ninargs;                        ///< number of input arguments
    int ninargs_nondef;
    int noutargs;                       ///< number of output arguments

    dynarray<charstr> comments;         ///< comments preceding the method declaration


    MethodIG()
        : bstatic(false), bdestroy(false), bptr(false), biref(true), bconst(false)
        , boperator(false) , binternal(false), bcapture(false)
        , bimplicit(false), ninargs(0), ninargs_nondef(0), noutargs(0), index(-1)
    {}

    bool parse( iglexer& lex, const charstr& host, const charstr& ns, dynarray<Arg>& irefargs );

    bool generate_h( binstream& bin );


    friend binstream& operator << (binstream& bin, const MethodIG& m)
    {   return bin << m.templarg << m.templsub << m.ret << m.name << m.intname << m.storage
        << m.boperator << m.binternal << m.bcapture << m.bstatic << m.bptr << m.biref
        << m.bconst << m.bimplicit << m.bdestroy << m.args << m.ninargs << m.ninargs_nondef
        << m.noutargs << m.comments << m.index; }
    friend binstream& operator >> (binstream& bin, MethodIG& m)
    {   return bin >> m.templarg >> m.templsub >> m.ret >> m.name >> m.intname >> m.storage
        >> m.boperator >> m.binternal >> m.bcapture >> m.bstatic >> m.bptr >> m.biref
        >> m.bconst >> m.bimplicit >> m.bdestroy >> m.args >> m.ninargs >> m.ninargs_nondef
        >> m.noutargs >> m.comments >> m.index; }
    friend metastream& operator << (metastream& m, const MethodIG& p)
    {
        MSTRUCT_OPEN(m,"MethodIG")
        MM(m,"templarg",p.templarg)
        MM(m,"templsub",p.templsub)
        MM(m,"return",p.ret)
        MM(m,"name",p.name)
        MM(m,"intname",p.intname)
        MM(m,"storage",p.storage)
        MM(m,"operator",p.boperator)
        MM(m,"internal",p.binternal)
        MM(m,"capture",p.bcapture)
        MM(m,"static",p.bstatic)
        MM(m,"ptr",p.bptr)
        MM(m,"iref",p.biref)
        MM(m,"const",p.bconst)
        MM(m,"implicit",p.bimplicit)
        MM(m,"destroy",p.bdestroy)
        MM(m,"args",p.args)
        MM(m,"ninargs",p.ninargs)
        MM(m,"ninargs_nondef",p.ninargs_nondef)
        MM(m,"noutargs",p.noutargs)
        MM(m,"comments",p.comments)
        MM(m,"index",p.index)
        MSTRUCT_CLOSE(m)
    }
};


///
struct Interface
{
    dynarray<charstr> nss;
    charstr name;
    charstr relpath, relpathjs;
    charstr hdrfile;
    charstr storage;
    charstr varname;

    charstr base;
    token baseclass;
    token basepath;

    dynarray<MethodIG> method;
    dynarray<MethodIG> event;

    MethodIG destroy;
    MethodIG default_creator;

    int oper_get;
    int oper_set;

    MethodIG::Arg getter, setter;

    charstr on_create, on_create_ev;

    uint nifc_methods;

    dynarray<charstr> pasters;
    charstr* srcfile;
    charstr* srcclass;

    uint hash;

    dynarray<charstr> comments;

    bool bvirtual;
    bool bdefaultcapture;

    Interface() : oper_get(-1), oper_set(-1), bvirtual(false), bdefaultcapture(false)
    {}

    void compute_hash();

    bool full_name_equals(token name) const {
        bool hasns = nss.find_if([&name](const charstr& v) { return !(name.consume(v) && name.consume("::")); }) == 0;
        if(hasns && name.consume(this->name))
            return name.is_empty();
        return false;
    }

    int check_interface( iglexer& lex );

    friend binstream& operator << (binstream& bin, const Interface& m)
    {   return bin << m.nss << m.name << m.relpath << m.relpathjs << m.hdrfile << m.storage << m.method
        << m.getter << m.setter << m.on_create << m.on_create_ev << bool(m.oper_get>=0) << m.nifc_methods
        << m.varname << m.event << m.destroy << m.hash << m.comments << m.pasters << *m.srcfile << *m.srcclass
        << m.base << m.baseclass << m.basepath << m.bvirtual << m.default_creator; }
    friend binstream& operator >> (binstream& bin, Interface& m)
    {   return bin; }
    friend metastream& operator << (metastream& m, const Interface& p)
    {
        MSTRUCT_OPEN(m,"Interface")
        MM(m,"ns",p.nss)
        MM(m,"name",p.name)
        MM(m,"relpath",p.relpath)
        MM(m,"relpathjs",p.relpathjs)
        MM(m,"hdrfile",p.hdrfile)
        MM(m,"storage",p.storage)
        MM(m,"method",p.method)
        MM(m,"getter",p.getter)
        MM(m,"setter",p.setter)
        MM(m,"oncreate",p.on_create)
        MM(m,"oncreateev",p.on_create_ev)
        MMT(m,"hasprops",bool)
        MM(m,"nifcmethods",p.nifc_methods)
        MM(m,"varname",p.varname)
        MM(m,"event",p.event)
        MM(m,"destroy",p.destroy)
        MM(m,"hash",p.hash)
        MM(m,"comments",p.comments)
        MMT(m,"pasters",dynarray<charstr>)
        MMT(m,"srcfile",charstr)
        MMT(m,"class",charstr)
        MM(m,"base",p.base)
        MM(m,"baseclass",p.baseclass)
        MM(m,"basepath",p.basepath)
        MM(m,"virtual",p.bvirtual)
        MM(m,"default_creator",p.default_creator)
        MSTRUCT_CLOSE(m)
    }
};


///
struct Class
{
    charstr classname;
    charstr templarg;
    charstr templsub;
    charstr namespc;                    ///< namespace with the trailing :: (if not empty)
    charstr ns;                         ///< namespace without the trailing ::
    bool noref;

    dynarray<charstr> namespaces;
    dynarray<Method> method;
    dynarray<Interface> iface;


    bool parse( iglexer& lex, charstr& templarg_, const dynarray<charstr>& namespcs, dynarray<paste_block>* pasters, dynarray<MethodIG::Arg>& irefargs );

    friend binstream& operator << (binstream& bin, const Class& m)
    {   return bin << m.classname << m.templarg << m.templsub << m.ns << m.namespc
        << m.noref << m.method << m.iface << m.namespaces; }
    friend binstream& operator >> (binstream& bin, Class& m)
    {   return bin >> m.classname >> m.templarg >> m.templsub >> m.ns >> m.namespc
        >> m.noref >> m.method >> m.iface >> m.namespaces; }
    friend metastream& operator << (metastream& m, const Class& p)
    {
        MSTRUCT_OPEN(m,"Class")
        MM(m,"class",p.classname)
        MM(m,"templarg",p.templarg)
        MM(m,"templsub",p.templsub)
        MM(m,"ns",p.ns)
        MM(m,"nsx",p.namespc)
        MM(m,"noref",p.noref)
        MM(m,"method",p.method)
        MM(m,"iface",p.iface)
        MM(m,"nss",p.namespaces)
        MSTRUCT_CLOSE(m)
    }
};

#endif //__INTERGEN__IG_H__
