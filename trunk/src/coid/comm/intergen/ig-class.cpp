
#include "ig.h"

#include "../hash/hashmap.h"



////////////////////////////////////////////////////////////////////////////////
int Interface::check_interface( iglexer& lex )
{
    int nerr=0;

    if((oper_get >= 0) != (oper_set >= 0)) {
        out << (lex.prepare_exception() << "warning: both setter and getter operator() must be defined for use with scripts\n");
        lex.clear_err();

        oper_get = oper_set = -1;
        ++nerr;
    }

    if(oper_get < 0) return nerr;

    MethodIG& get = method[oper_get];
    MethodIG& set = method[oper_set];

    //TODO: check types
    if(get.args.size() < 1  ||  set.args.size() < 2) {
        out << (lex.prepare_exception() << "warning: insufficient number of arguments in getter/setter operator()\n");
        lex.clear_err();

        oper_get = oper_set = -1;
        return ++nerr;
    }

    getter = get.ret;
    getter.tokenpar = get.args[0].tokenpar;

    setter = set.args[1];
    setter.tokenpar = set.args[0].tokenpar;

    return nerr;
}

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after rl_cmd or rl_cmd_p
bool Class::parse( iglexer& lex, charstr& templarg_, const dynarray<charstr>& namespcs, dynarray<paste_block>* pasters, dynarray<MethodIG::Arg>& irefargs )
{
    templarg.swap(templarg_);
    namespaces = namespcs;

    namespaces.for_each([this](const charstr& v){ namespc << v << "::"; });
    if(namespc)
        ns.set_from_range(namespc.ptr(), namespc.ptre()-2);

    token t = templarg;
    if(!t.is_empty()) {
        templsub << char('<');
        for( int i=0; !t.is_empty(); ++i ) {
            token x = t.cut_left(' ');

            if(i)  templsub << ", ";
            templsub << t.cut_left(',');
        }
        templsub << char('>');
    }

    hash_map<charstr,uint,hash<token>> map_overloads;
    typedef hash_map<charstr,uint,hash<token>>::value_type t_val;

    int ncontinuable_errors = 0;

    if( !lex.matches(lex.IDENT, classname) )
        lex.set_err() << "expecting class name\n";
    else {
        const lexer::lextoken& tok = lex.last();

        noref = true;

        while( lex.next() != '{' ) {
            if( tok.end() ) {
                lex.set_err() << "unexpected end of file\n";
                return false;
            }
            if( tok == ';' )
                return false;

            noref = false;
        }

        //ignore nested blocks
        lex.ignore(lex.CURLY, true);

        dynarray<charstr> commlist;

        int mt;
        while( 0 != (mt=lex.find_method(classname, commlist)) )
        {
            if(mt<0) {
                //interface definitions
                const lexer::lextoken& tok = lex.last();

                bool classifc = tok == lex.IFC_CLASS;
                bool classvar = tok == lex.IFC_CLASS_VAR;
                bool classvirtual = tok == lex.IFC_CLASS_VIRTUAL;
                bool extfn = tok == lex.IFC_FNX;
                bool extev = tok == lex.IFC_EVENTX;
                bool bimplicit = false;
                bool bdestroy = false;
                int8 binternal = 0;
                int8 bnocapture = 0;
                int8 bcapture = 0;

                charstr extname, implname;
                if(extev || extfn) {
                    //parse external name
                    lex.match('(');

                    if(extfn && lex.matches('~'))
                        bdestroy = true;
                    else {
                        while(int k = lex.matches_either('!', '-', '+'))
                            (&binternal)[k-1]++;

                        lex.matches(lex.IDENT, extname);

                        /*binternal = lex.matches('!');
                        bimplicit = lex.matches('@');
                        if(bimplicit) {
                            lex.match(lex.IDENT, implname);
                            lex.matches(lex.IDENT, extname);
                        }
                        else {
                            lex.matches(lex.IDENT, extname);
                            bimplicit = lex.matches('@');
                            if(bimplicit)
                                lex.match(lex.IDENT, implname);
                        }*/
                    }
                    lex.match(')');
                }

                if(classifc || classvar || classvirtual)
                {
                    if(iface.size() > 0)
                        iface.last()->check_interface(lex);

                    //parse interface declaration
                    Interface* ifc = iface.add();
                    ifc->nifc_methods = 0;
                    ifc->comments.takeover(commlist);
                    ifc->bvirtual = classvirtual;

                    lex.match('(');
                    ifc->bdefaultcapture = lex.matches_either('+', '-') == 1;
                    ifc->name = lex.match(lex.IDENT);

                    while(lex.matches("::")) {
                        ifc->nss.add()->swap(ifc->name);
                        ifc->name = lex.match(lex.IDENT);
                    }

                    if(lex.matches(':')) {
                        //a base class for the interface
                        ifc->baseclass = ifc->base = lex.match(lex.IDENT);
                        while(lex.matches("::")) {
                            ifc->base << "::";
                            token bc = lex.match(lex.IDENT);
                            ifc->base << bc;
                            ifc->baseclass.set(ifc->base.ptre()-bc.len(), ifc->base.ptre());
                        }
                    }

                    lex.match(',');
                    ifc->relpath = lex.match(lex.DQSTRING);

                    if(classvar) {
                        lex.match(',');
                        ifc->varname = lex.match(lex.IDENT);
                    }

                    lex.match(')');

                    ifc->parse_docs();

                    pasters->for_each( [ifc](paste_block& b){
                        if(b.cond.is_empty() || ifc->full_name_equals(b.cond))
                            *ifc->pasters.add() = b.block;
                    });
                    //ifc->pasters = pasters;
                }
                else if(extev || tok == lex.IFC_EVENT) {
                    //parse event declaration
                    if(iface.size() == 0) {
                        out << (lex.prepare_exception() << "error: no preceding interface declared\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }
                    else if(iface.last()->varname.is_empty()) {
                        out << (lex.prepare_exception() << "error: events can be used only with bidirectional interfaces\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    Interface* ifc = iface.last();
                    MethodIG* m = ifc->event.add();

                    m->comments.takeover(commlist);
                    m->binternal = binternal>0;
                    m->bimplicit = bimplicit;

                    if(bimplicit) {
                        lex.match(';', "error: implicit events must not be declared");

                        if(implname == "on_create") {
                            m->name = ifc->on_create_ev = extname ? extname : "on_create";

                            m->ret.type = m->ret.basetype = m->ret.fulltype = "void";
                        }
                        else {
                            out << (lex.prepare_exception() << "error: unrecognized implicit event\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }
                    }
                    else {
                        if(!m->parse(lex, classname, namespc, irefargs))
                            ++ncontinuable_errors;

                        if(extname) {
                            m->intname.takeover(m->name);
                            m->name.takeover(extname);
                        }
                        else
                            m->intname = m->name;

                        if(m->bstatic) {
                            out << (lex.prepare_exception() << "error: interface event cannot be static\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }
                    }
 
                    m->parse_docs();
                }
                else if(extfn || tok == lex.IFC_FN) {
                    //parse function declaration
                    if(iface.size() == 0)
                        throw lex.set_err() << "error: no preceding interface declared\n";

                    Interface* ifc = iface.last();
                    MethodIG* m = ifc->method.add();

                    m->comments.takeover(commlist);
                    m->binternal = binternal>0;

                    if(!m->parse(lex, classname, namespc, irefargs))
                        ++ncontinuable_errors;

                    m->parse_docs();

                    int capture = ifc->bdefaultcapture ? 1 : 0;
                    capture -= bnocapture;
                    capture += bcapture;

                    m->bcapture = capture>0 && !m->bconst && !m->bstatic;

                    if(bcapture>bnocapture && !m->bcapture) {
                        out << (lex.prepare_exception() << "warning: const and static methods aren't captured\n");
                        lex.clear_err();
                    }

                    if(extname) {
                        m->intname.takeover(m->name);
                        m->name.takeover(extname);
                    }
                    else
                        m->intname = m->name;

                    if(m->boperator) {
                        if(m->bconst && ifc->oper_get>=0) {
                            out << (lex.prepare_exception() << "error: property getter already defined\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }
                        if(!m->bconst && ifc->oper_set>=0) {
                            out << (lex.prepare_exception() << "error: property getter already defined\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }

                        if(m->bconst)
                            ifc->oper_get = int(ifc->method.size()-1);
                        else
                            ifc->oper_set = int(ifc->method.size()-1);
                    }

                    if(!m->bstatic)
                        ++iface.last()->nifc_methods;

                    if(m->bstatic && bdestroy) {
                        out << "error: method to call on interface release cannot be static\n";
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    if(bimplicit) {
                        if(m->name == "on_create") {
                            if(m->ret.type != "void" && m->args.size() != 0) {
                                out << (lex.prepare_exception() << "error: invalid format for an on_create method\n");
                                lex.clear_err();
                                ++ncontinuable_errors;
                            }
                            ifc->on_create = m->intname;
                        }

                        ifc->method.pop();
                    }

                    m->bdestroy = bdestroy;

                    if(bdestroy) {
                        //mark and move to the first pos
                        if(ifc->destroy.name) {
                            out << (lex.prepare_exception() << "error: interface release method already specified\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }

                        ifc->destroy = *m;
                        ifc->method.move(m-ifc->method.ptr(), 0, 1);
                    }

                    if(m->bstatic && m->args.size() == 0 && ifc->default_creator.name.is_empty())
                        ifc->default_creator = *m;
                }
                else {
                    //produce a warning for other misplaced keywords
                    out << (lex.prepare_exception() << "warning: misplaced keyword\n");
                    lex.clear_err();
                }
            }
            else {
                //rlcmd
                Method* m = method.add();
                m->parse(lex, mt);

                static token renderer = "renderer";
                if( classname == renderer )
                    m->bstatic = true;          //special handling for the renderer

                uint* v = const_cast<uint*>( map_overloads.find_value(m->name) );
                if(v)
                    m->overload << ++*v;
                else
                    map_overloads.insert_key_value(m->name, 0);
            }
        }

        if(iface.size() > 0)
            iface.last()->check_interface(lex);
    }

    return ncontinuable_errors ? false : lex.no_err();
}

////////////////////////////////////////////////////////////////////////////////
void Interface::compute_hash()
{
    charstr mash;

    MethodIG* ps = method.ptr();
    MethodIG* pe = method.ptre();

    int indexs=0, indexm=0;

    for(; ps<pe; ++ps)
    {
        ps->index = ps->bstatic ? indexs++ : indexm++;

        mash << ps->name << ps->ret.type;

        const MethodIG::Arg* pas = ps->args.ptr();
        const MethodIG::Arg* pae = ps->args.ptre();

        for(; pas<pae; ++pas)
        {
            mash << pas->type << pas->size << (pas->binarg?'i':' ') << (pas->boutarg?'o':' ');
        }
    }

    hash = __coid_hash_string(mash.ptr(), mash.len());

    ps = event.ptr();
    pe = event.ptre();
    indexm = 0;

    for(; ps<pe; ++ps)
    {
        ps->index = indexm++;
    }
}

////////////////////////////////////////////////////////////////////////////////
void Interface::parse_docs()
{
    auto b = comments.ptr();
    auto e = comments.ptre();
    charstr doc;

    for(; b!=e; ++b)
    {
        token line = *b;

        line.trim();
        line.skip_char('/');
        if(!line) {
            if(doc) //paragraph
                docs.add()->swap(doc);
            continue;
        }

        if(line.first_char() != '@') {
            if(!doc.is_empty())
                doc << ' ';
            doc.append_escaped(line);
        }
        else {
            //close previous string
            docs.add()->swap(doc);
            doc.append_escaped(line);
        }
    }

    if(doc)
        docs.add()->swap(doc);
}
