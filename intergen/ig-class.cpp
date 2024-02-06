
#include "ig.h"

#include "../hash/hashmap.h"



////////////////////////////////////////////////////////////////////////////////
int Interface::check_interface(iglexer& lex, const dynarray<paste_block>& classpasters)
{
    for (const paste_block& b : classpasters) {
        if (b.condx.is_empty() || full_name_equals(b.condx)) {
            b.fill(*pasteinners.add());
        }
    }

    int nerr = 0;

    if ((oper_get >= 0) != (oper_set >= 0)) {
        out << (lex.prepare_exception()
            << "warning: both setter and getter operator() must be defined for use with scripts\n");
        lex.clear_err();

        oper_get = oper_set = -1;
        ++nerr;
    }

    if (oper_get < 0) return nerr;

    MethodIG& get = method[oper_get];
    MethodIG& set = method[oper_set];

    //TODO: check types
    if (get.args.size() < 1  ||  set.args.size() < 2) {
        out << (lex.prepare_exception()
            << "warning: insufficient number of arguments in getter/setter operator()\n");
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
bool Class::parse(iglexer& lex, charstr& templarg_, const dynarray<charstr>& namespcs, dynarray<paste_block>* filepasters, dynarray<MethodIG::Arg>& irefargs)
{
    templarg.swap(templarg_);
    //namespaces = namespcs;
    for (auto& x : namespcs) {
        if (x)
            namespaces.push(x);
    }

    namespaces.for_each([this](const charstr& v) { namespc << v << "::"; });
    if (namespc)
        ns.set_from_range(namespc.ptr(), namespc.ptre() - 2);

    token t = templarg;
    if (!t.is_empty()) {
        templsub << char('<');
        for (int i = 0; !t.is_empty(); ++i) {
            t.cut_left(' ');

            if (i)  templsub << ", ";
            templsub << t.cut_left(',');
        }
        templsub << char('>');
    }

    hash_map<charstr, uint, hasher<token>> map_overloads;

    int ncontinuable_errors = 0;

    if (lex.matches_block(lex.CURLY, true)) {
        //some dummy unnamed class
        return false;
    }

    if (!lex.matches(lex.IDENT, classname)) {
        lex.syntax_err() << "expecting class name\n";
        throw lex.exc();
    }

    const lexer::lextoken& tok = lex.last();

    noref = true;

    while (lex.next() != '{') {
        if (tok.end()) {
            lex.syntax_err() << "unexpected end of file\n";
            return false;
        }
        if (tok == ';')
            return false;

        noref = false;
    }

    //ignore nested blocks
    //lex.ignore(lex.CURLY, true);

    dynarray<charstr> commlist;
    dynarray<Interface>* lastifaces = 0;
    Interface* lastifc = 0;

    int mt;
    while (0 != (mt = lex.find_method(classname, classpasters, commlist)))
    {
        if (mt < 0) {
            //interface definitions
            const lexer::lextoken& tok = lex.last();

            bool dataifc = tok == lex.IFC_STRUCT;
            int classifc = 0;
            int classvar = 0;

            if (tok == lex.IFC_CLASS)
            {
                classifc = 1;
            }
            else if (tok == lex.IFC_CLASSX)
            {
                classifc = 2;
            }
            else if (tok == lex.IFC_CLASS_INHERIT)
            {
                classifc = 3;
            }
            else if (tok == lex.IFC_CLASS_INHERITABLE)
            {
                classifc = 4;
            }
            else if (tok == lex.IFC_CLASS_VAR)
            {
                classvar = 1;
            }
            else if (tok == lex.IFC_CLASSX_VAR)
            {
                classvar = 2;
            }
            else if (tok == lex.IFC_CLASS_VAR_INHERIT)
            {
                classvar = 3;
            }
            else if (tok == lex.IFC_CLASS_VAR_INHERITABLE)
            {
                classvar = 4;
            }

            bool classvirtual = tok == lex.IFC_CLASS_VIRTUAL;
            bool classext = (classifc == 2) || (classvar == 2);
            bool classinherit = (classifc == 3) || (classvar == 3);
            bool classinheritable = (classifc == 4) || (classvar == 4);

            bool extfn = tok == lex.IFC_FNX;
            bool extev = tok == lex.IFC_EVENTX;
            bool bimplicit = false;
            bool bdestroy = false;
            bool bpure = false;
            int8 binternal = 0;
            int8 bnocapture = 0;
            int8 bcapture = 0;

            charstr extname;//, implname;
            if (extev || extfn) {
                //parse external name
                lex.match('(');

                if (extfn && lex.matches('~'))
                    bdestroy = true;
                else {
                    while (int k = lex.matches_either('!', '-', '+'))
                        (&binternal)[k - 1]++;

                    bimplicit = lex.matches('@');

                    lex.matches(lex.IDENT, extname);

                    bpure = extev && lex.matches('=') && lex.matches('0');

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

            if (dataifc || classifc || classvar || classvirtual)
            {
                if (lastifc)
                    lastifc->check_interface(lex, classpasters);

                lastifaces = dataifc ? &iface_data : &iface_refc;

                //parse interface declaration
                lastifc = lastifaces->add();
                Interface* ifc = lastifc;

                ifc->nifc_methods = 0;
                ifc->comments.takeover(commlist);
                ifc->bvirtual = classvirtual;
                ifc->bdataifc = dataifc;
                ifc->binheritable = classinheritable;
                ifc->bvirtualorinheritable = ifc->bvirtual || ifc->binheritable;

                lex.match('(');
                ifc->bdefaultcapture = lex.matches_either('+', '-') == 1;
                ifc->name = lex.match(lex.IDENT);

                while (lex.matches("::"_T)) {
                    if (ifc->nsname)
                        ifc->nsname << "::"_T;
                    ifc->nsname << ifc->name;

                    ifc->nss.add()->swap(ifc->name);
                    ifc->name = lex.match(lex.IDENT);
                }

                if (ifc->nsname)
                    ifc->nsname << "::"_T;
                ifc->nsname << ifc->name;

                if (lex.matches(':')) {
                    //a base class for the interface
                    ifc->baseclass = ifc->base = lex.match(lex.IDENT);
                    while (lex.matches("::"_T)) {
                        ifc->base << "::"_T;
                        token bc = lex.match(lex.IDENT);
                        ifc->base << bc;
                        ifc->baseclass.set(ifc->base.ptre() - bc.len(), ifc->base.ptre());
                    }
                }

                lex.match(',');

                if (classext) {
                    //get ifc to extend
                    charstr base = lex.match(lex.IDENT).value();
                    token baseclass = base;

                    while (lex.matches("::"_T)) {
                        base << "::"_T;
                        token bc = lex.match(lex.IDENT);
                        base << bc;
                        baseclass.set(base.ptre() - bc.len(), base.ptre());
                    }

                    if (ifc->nsname)
                        ifc->nsname << "::"_T;
                    ifc->nsname << ifc->name;

                    //find in previous interfaces
                    Interface* bifc = 0;
                    for (Interface& pifc : *lastifaces)
                    {
                        if (&pifc == ifc)
                            continue;
                        if (pifc.nsname == base) {
                            bifc = &pifc;
                            break;
                        }
                    }

                    if (!bifc) {
                        lex.prepare_exception()
                            << "error: base interface " << base << " not declared in this class\n";
                        throw lex.exc();
                    }

                    ifc->copy_methods(*bifc);
                }
                else if (classinherit)
                {
                    //a base class for the interface
                    token bc;
                    ifc->baseclass = ifc->base = bc = lex.match(lex.IDENT);
                    while (lex.matches("::"_T)) {
                        *ifc->baseclassnss.add() = bc;
                        ifc->base << "::"_T;
                        bc = lex.match(lex.IDENT);
                        ifc->base << bc;
                        ifc->baseclass.set(ifc->base.ptre() - bc.len(), ifc->base.ptre());
                    }
                }
                else 
                {
                    ifc->relpath = lex.match(lex.DQSTRING);
                }

                if (classinherit) // parse file with  
                {
                    lex.match(',');
                    ifc->bdirect_inheritance = true;
                    ifc->basesrc = lex.match(lex.DQSTRING);
                    
                    lex.match(',');
                    ifc->relpath = lex.match(lex.DQSTRING);
                }

                if (!classext && (classvar && classvar != 3)) {
                    lex.match(',');
                    ifc->varname = lex.match(lex.IDENT);
                }

                lex.match(')');

                ifc->parse_docs();

                if (filepasters) {
                    for (paste_block& b : *filepasters) {
                        if (b.condx.is_empty() || ifc->full_name_equals(b.condx)) {
                            if (!b.in_dispatch) {
                                if (b.pos == paste_block::position::after_class)
                                    b.fill(*ifc->pasteafters.add());
                                else
                                    b.fill(*ifc->pasters.add());
                            }
                        }
                    }
                }
                else {
                    ifc->relpath = lex.match(lex.DQSTRING);
                }
            }
            else if (extev || tok == lex.IFC_EVENT)
            {
                //event declaration may be commented out if the method is a duplicate (with multiple interfaces)
                bool slcom = lex.enable(lex.SLCOM, false);
                bool mlcom = lex.ignore(lex.MLCOM, false);
                int duplicate = lex.matches_either("//", "/*");
                lex.enable(lex.SLCOM, slcom);

                //parse event declaration
                if (!lastifc) {
                    lex.prepare_exception()
                        << "error: no preceding interface declared\n";
                    throw lex.exc();
                }
                else if (lastifc->varname.is_empty() && !lastifc->bdirect_inheritance) {
                    out << (lex.prepare_exception()
                        << "error: events can be used only with bidirectional interfaces\n");
                    lex.clear_err();
                    ++ncontinuable_errors;
                }

                MethodIG* m = lastifc->event.add();

                m->comments.takeover(commlist);
                m->binternal = binternal > 0;
                m->bimplicit = bimplicit;
                m->bduplicate = duplicate != 0;
                m->bpure = bpure;

                {
                    if (!m->parse(lex, classname, namespc, lastifc->nsname, irefargs, true))
                        ++ncontinuable_errors;

                    if (duplicate == 2) {
                        lex.match(';');
                        lex.match("*/");
                    }
                    else if (lastifc->varname.is_empty() && !lastifc->bdirect_inheritance) {
                        lex.ignore(lex.MLCOM, mlcom);
                    }

                    if (extname) {
                        m->intname.takeover(m->name);
                        m->name.takeover(extname);
                    }
                    else
                        m->intname = m->name;

                    if (m->bstatic) {
                        out << (lex.prepare_exception()
                            << "error: interface event cannot be static\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }
                }

                if (m->bimplicit) {
                    //lex.match(';', "error: implicit events must not be declared");

                    if (m->name == "connect") {
                        /// @connect invoked on successfull interface connection
                        lastifc->on_connect_ev = m->name = m->intname;

                        //m->ret.type = m->ret.basetype = m->ret.fulltype = "void";
                    }
                    else {
                        out << (lex.prepare_exception()
                            << "error: unrecognized implicit event\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }
                }

                if (m->bduplicate) {
                    //find original in previous interface
                    int nmiss = 0;

                    Interface* fi = lastifaces->ptr();
                    for (; fi < lastifc; ++fi) {
                        if (fi->has_mismatched_method(*m, fi->event))
                            ++nmiss;
                    }

                    if (nmiss) {
                        out << (lex.prepare_exception()
                            << "warning: a matching duplicate event " << m->name << " not found in previous interfaces\n");
                        lex.clear_err();
                    }
                }

                m->parse_docs();
            }
            else if (extfn || tok == lex.IFC_FN)
            {
                //method declaration may be commented out if the method is a duplicate (with multiple interfaces)
                bool slcom = lex.enable(lex.SLCOM, false);
                bool mlcom = lex.ignore(lex.MLCOM, false);
                int duplicate = lex.matches_either("//", "/*");
                lex.enable(lex.SLCOM, slcom);


                //parse function declaration
                if (lastifc == 0) {
                    lex.syntax_err() << "no preceding interface declared\n";
                    throw lex.exc();
                }

                MethodIG* m = lastifc->method.add();

                m->comments.takeover(commlist);
                m->binternal = binternal > 0;
                m->bduplicate = duplicate != 0;
                m->bimplicit = bimplicit;

                if (!m->parse(lex, classname, namespc, lastifc->nsname, irefargs, false))
                    ++ncontinuable_errors;

                if (duplicate == 2) {
                    lex.match(';');
                    lex.match("*/");
                }
                lex.ignore(lex.MLCOM, mlcom);


                m->parse_docs();

                int capture = lastifc->bdefaultcapture ? 1 : 0;
                capture -= bnocapture;
                capture += bcapture;

                m->bcapture = capture > 0 && !m->bconst && !m->bstatic;

                if (bcapture > bnocapture && !m->bcapture) {
                    out << (lex.prepare_exception()
                        << "warning: const and static methods aren't captured\n");
                    lex.clear_err();
                }

                if (extname) {
                    m->intname.takeover(m->name);
                    m->name.takeover(extname);
                }
                else
                    m->intname = m->name;

                if (m->boperator) {
                    if (m->bconst && lastifc->oper_get >= 0) {
                        out << (lex.prepare_exception() << "error: property getter already defined\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }
                    if (!m->bconst && lastifc->oper_set >= 0) {
                        out << (lex.prepare_exception() << "error: property getter already defined\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    if (m->bconst)
                        lastifc->oper_get = int(lastifc->method.size() - 1);
                    else
                        lastifc->oper_set = int(lastifc->method.size() - 1);
                }

                if (!m->bstatic)
                    ++lastifc->nifc_methods;

                if (m->bstatic && bdestroy) {
                    out << "error: method to call on interface release cannot be static\n";
                    lex.clear_err();
                    ++ncontinuable_errors;
                }

                if (m->bimplicit) {
                    if (m->name == "connect") {
                        /// @connect called when interface connects successfully
                        if (m->ret.type != "void" && m->args.size() != 0) {
                            out << (lex.prepare_exception()
                                << "error: invalid format for connect method\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }
                        lastifc->on_connect = m->name = m->intname;
                    }
                    else if (m->name == "unload") {
                        /// @unload invoked when client dll/script is unloaded
                        lastifc->on_unload = m->name = m->intname;

                        if (!m->bstatic) {
                            out << (lex.prepare_exception()
                                << "error: unload method must be static\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }

                        m->bcreator = false;
                    }
                    else {
                        out << (lex.prepare_exception()
                            << "error: unrecognized implicit method\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    //ifc->method.pop();
                }

                m->bdestroy = bdestroy;

                if (bdestroy) {
                    //mark and move to the first pos
                    if (lastifc->destroy.name) {
                        out << (lex.prepare_exception()
                            << "error: interface release method already specified\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    lastifc->destroy = *m;
                    lastifc->method.move(m - lastifc->method.ptr(), 0, 1);
                    m = lastifc->method.ptr();
                }

                if (m->bcreator && m->args.size() == 0 && lastifc->default_creator.name.is_empty())
                    lastifc->default_creator = *m;

                if (!m->bstatic && !m->binternal && !m->boperator) {
                    //check if another public method with the same name exists
                    MethodIG* mdup = lastifc->method.find_if([&](const MethodIG& mi) {
                        return !mi.bstatic && !mi.binternal && mi.name == m->name;
                    });
                    if (mdup != m) {
                        out << (lex.prepare_exception()
                            << "error: overloaded methods not supported for scripting interface\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }
                }

                if (m->bduplicate) {
                    //find original in previous interface
                    int nmiss = 0;

                    Interface* fi = lastifaces->ptr();
                    for (; fi < lastifc; ++fi) {
                        if (fi->has_mismatched_method(*m, fi->method))
                            ++nmiss;
                    }

                    if (nmiss) {
                        out << (lex.prepare_exception()
                            << "warning: a matching duplicate method " << m->name << " not found in previous interfaces\n");
                        lex.clear_err();
                    }
                }
            }
            else {
                //produce a warning for other misplaced keywords
                out << (lex.prepare_exception()
                    << "warning: misplaced keyword\n");
                lex.clear_err();
            }
        }
        else {
            //rlcmd
            Method* m = method.add();
            m->parse(lex, mt);

            static token renderer = "renderer";
            if (classname == renderer)
                m->bstatic = true;          //special handling for the renderer

            uint* v = const_cast<uint*>(map_overloads.find_value(m->name));
            if (v)
                m->overload << ++ * v;
            else
                map_overloads.insert_key_value(m->name, 0);
        }
    }

    if (iface_refc.size() > 0)
        iface_refc.last()->check_interface(lex, classpasters);
    if (iface_data.size() > 0)
        iface_data.last()->check_interface(lex, classpasters);

    //check consistency
    if (iface_refc.size() > 0 && iface_data.size() > 0) {
        out << (lex.prepare_exception()
            << "error: mixed interface types in host class `" << classname << "'\n");
        lex.clear_err();
        ++ncontinuable_errors;
    }

    datahost = iface_data.size() > 0;

    return ncontinuable_errors ? false : lex.no_err();
}

////////////////////////////////////////////////////////////////////////////////
void Interface::compute_hash(int version)
{
    charstr mash;

    MethodIG* ps = method.ptr();
    MethodIG* pe = method.ptre();

    int indexs = 0, indexm = 0;

    for (; ps < pe; ++ps)
    {
        ps->index = ps->bstatic ? indexs++ : indexm++;

        mash << ps->name << ps->ret.type;

        const MethodIG::Arg* pas = ps->args.ptr();
        const MethodIG::Arg* pae = ps->args.ptre();

        for (; pas < pae; ++pas) {
            mash << pas->type << pas->arsize << (pas->binarg ? 'i' : ' ') << (pas->boutarg ? 'o' : ' ');
        }
    }

    mash << ':';

    ps = event.ptr();
    pe = event.ptre();
    indexm = 0;

    for (; ps < pe; ++ps)
    {
        ps->index = indexm++;

        mash << ps->name << ps->ret.type;

        const MethodIG::Arg* pas = ps->args.ptr();
        const MethodIG::Arg* pae = ps->args.ptre();

        for (; pas < pae; ++pas) {
            mash << pas->type << pas->arsize << (pas->binarg ? 'i' : ' ') << (pas->boutarg ? 'o' : ' ');
        }
    }

    mash << 'v' << version;

    hash = __coid_hash_string(mash.ptr(), mash.len());
}

////////////////////////////////////////////////////////////////////////////////
void Interface::parse_docs()
{
    auto b = comments.ptr();
    auto e = comments.ptre();
    charstr doc;

    for (; b!=e; ++b)
    {
        token line = *b;

        line.trim();
        if (line.consume("/**")) {
            line.skip_whitespace();
            line.shift_end(-3);
            line.trim_whitespace();
        }
        else {
            line.skip_char('/');
            line.trim_whitespace();
        }

        if (!line) {
            if (doc) //paragraph
                docs.add()->swap(doc);
            continue;
        }

        if (line.first_char() != '@') {
            if (!doc.is_empty())
                doc << ' ';
            //doc.append_escaped(line);
            doc << line;
        }
        else {
            //close previous string
            docs.add()->swap(doc);
            //doc.append_escaped(line);
            doc << line;
        }
    }

    if (doc)
        docs.add()->swap(doc);
}
