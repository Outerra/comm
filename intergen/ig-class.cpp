
#include "ig.h"

#include "../dir.h"
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
        out << (lex.prepare_exception() << "warning: both setter and getter operator() must be defined for use with scripts\n");
        lex.clear_err();

        oper_get = oper_set = -1;
        ++nerr;
    }

    if (oper_get < 0) return nerr;

    MethodIG& get = method[oper_get];
    MethodIG& set = method[oper_set];

    //TODO: check types
    if (get.args.size() < 1 || set.args.size() < 2) {
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

            bool dataifc = tok == "ifc_struct"_T || tok == "IFC_STRUCT"_T;
            bool isclass = false;
            bool isclassvirtual = false;
            bool isvar = false;
            bool isextpath = false;

            if (tok == "ifc_class"_T || tok == "IFC_CLASS"_T)
            {
                isclass = true;
            }
            else if (tok == "ifc_class_var"_T || tok == "IFC_CLASS_VAR"_T)
            {
                isclass = true;
                isvar = true;
            }
            else if (tok == "ifc_class_virtual"_T || tok == "IFC_CLASS_VIRTUAL"_T)
            {
                isclass = true;
                isclassvirtual = true;
            }
            else if (tok == "ifc_class_virtual_var"_T || tok == "IFC_CLASS_VIRTUAL_VAR"_T)
            {
                isclass = true;
                isclassvirtual = true;
                isvar = true;
            }

            bool extfn_struct = tok == "ifc_fnx_struct"_T;
            bool extfn_class = tok == "ifc_fnx_class"_T;
            bool extfn = tok == "ifc_fnx"_T || extfn_struct || extfn_class;
            bool extev = tok == "ifc_eventx"_T;
            bool bimplicit = false;
            bool bdestroy = false;
            bool bpure = false;
            int8 binternal = 0;
            int8 bnocapture = 0;
            int8 bcapture = 0;

            charstr extifcname;
            charstr extname;

            if (extev || extfn) {
                //parse external name
                lex.match('(');

                bool has_rename = true;

                if (extfn_struct || extfn_class) {
                    //match fully qualified interface name
                    extifcname = lex.match(lex.IDENT);

                    while (lex.matches("::"_T)) {
                        extifcname << "::"_T << lex.match(lex.IDENT);
                    }

                    has_rename = lex.matches(',');
                }

                if (!has_rename)
                    ;
                else if (extfn && lex.matches('~')) {
                    bdestroy = true;
                }
                else {
                    while (int k = lex.matches_either('!', '-', '+')) {
                        (&binternal)[k - 1]++;
                    }

                    bimplicit = lex.matches('@');

                    lex.matches(lex.IDENT, extname);

                    bpure = extev && lex.matches('=') && lex.matches('0');
                }
                lex.match(')');
            }

            if (dataifc || isclass)
            {
                if (lastifc)
                    lastifc->check_interface(lex, classpasters);

                lastifaces = dataifc ? &iface_data : &iface_refc;

                //parse interface declaration
                lastifc = lastifaces->add();
                Interface* ifc = lastifc;

                ifc->file = lex.get_current_file();
                ifc->line = lex.current_line();

                //ifc->nifc_methods = 0;
                ifc->comments.takeover(commlist);

                lex.match('(');
                ifc->bdefaultcapture = lex.matches_either('+', '-') == 1;
                ifc->bnoscript = lex.matches('!');
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
                    ifc->bvirtualbase = lex.matches("virtual"_T);

                    ifc->baseclass = ifc->base = lex.match(lex.IDENT);
                    while (lex.matches("::"_T)) {
                        ifc->base << "::"_T;
                        token bc = lex.match(lex.IDENT);
                        ifc->base << bc;
                        ifc->baseclass.set(ifc->base.ptre() - bc.len(), ifc->base.ptre());
                    }
                }

                lex.match(',');

                ifc->relpath = lex.match(lex.DQSTRING);

                ifc->bvartype = isvar;
                if (isvar) {
                    lex.match(',');
                    ifc->varname = lex.match(lex.IDENT);
                }

                if (lex.matches(','))
                {
                    ifc->basesrc = lex.match(lex.DQSTRING);
                    isextpath = true;

                    if (!ifc->baseclass) {
                        lex.prepare_exception() << "error: no base interface specified\n";
                        throw lex.exc();
                    }
                }

                if (ifc->baseclass && !ifc->bvirtualbase && !isextpath)
                {
                    //same class interface extension
                    //find in previous interfaces
                    Interface* base_ifc = 0;
                    for (Interface& pifc : *lastifaces)
                    {
                        if (&pifc == ifc)
                            continue;
                        if (pifc.nsname == ifc->base) {
                            base_ifc = &pifc;
                            break;
                        }
                    }

                    if (base_ifc)
                    {
                        //ifc->copy_methods(*base_ifc);
                    }
                    // else not declared in this class, assume virtual base interface
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

                ifc->bvirtual = isclassvirtual;
                ifc->bdataifc = dataifc;
                ifc->bextend = !ifc->baseclass.is_empty();
                ifc->bextend_ext = isextpath;
            }
            else if (extev || tok == "ifc_event"_T)
            {
                //event declaration may be commented out if the method is a duplicate (with multiple interfaces)
                bool slcom = lex.enable(lex.SLCOM, false);
                bool mlcom = lex.ignore(lex.MLCOM, false);
                int duplicate = lex.matches_either("//", "/*");
                lex.enable(lex.SLCOM, slcom);

                //parse event declaration
                if (!lastifc) {
                    lex.prepare_exception() << "error: no preceding interface declared\n";
                    throw lex.exc();
                }

                MethodIG* m = lastifc->event.add();

                m->comments.takeover(commlist);
                m->binternal = binternal > 0 || lastifc->bnoscript;
                m->bimplicit = bimplicit;
                m->bduplicate = duplicate != 0;
                m->bpure = bpure;
                m->classname << namespc << classname;

                {
                    if (!m->parse(lex, classname, namespc, lastifc->nsname, irefargs, true))
                        ++ncontinuable_errors;

                    const MethodIG* old = lastifc->method.find_if([m](const MethodIG& o) {
                        return &o != m && o.name == m->name && m->matches_args(o);
                    });

                    if (old) {
                        charstr relpath;
                        directory::get_relative_path(m->file, old->file, relpath, true);
                        out << (lex.prepare_exception() << "method already declared in " << relpath << "(" << old->line << "): " << m->name << "\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    if (duplicate == 2) {
                        lex.match(';');
                        lex.match("*/");
                    }
                    else if (lastifc->varname.is_empty() && !lastifc->bextend_ext) {
                        lex.ignore(lex.MLCOM, mlcom);
                    }

                    if (extname) {
                        m->intname.takeover(m->name);
                        m->name.takeover(extname);
                    }
                    else
                        m->intname = m->name;

                    m->basename = m->name;

                    if (m->bstatic) {
                        out << (lex.prepare_exception() << "error: interface event cannot be static\n");
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
                        out << (lex.prepare_exception() << "error: unrecognized implicit event\n");
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
                        out << (lex.prepare_exception() << "warning: a matching duplicate event " << m->name << " not found in previous interfaces\n");
                        lex.clear_err();
                    }
                }

                m->parse_docs();
            }
            else if (extfn || tok == "ifc_fn"_T)
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
                m->binternal = binternal > 0 || lastifc->bnoscript;
                m->bduplicate = duplicate != 0;
                m->bimplicit = bimplicit;
                m->classname << namespc << classname;

                if (!m->parse(lex, classname, namespc, lastifc->nsname, irefargs, false))
                    ++ncontinuable_errors;

                const MethodIG* old = lastifc->method.find_if([m](const MethodIG& o) {
                    return &o != m && o.bcreator == m->bcreator && o.name == m->name && m->matches_args(o);
                });

                if (old) {
                    charstr relpath;
                    directory::get_relative_path(m->file, old->file, relpath, true);
                    out << (lex.prepare_exception() << "method already declared in " << relpath << "(" << old->line << "): " << m->name << "\n");
                    lex.clear_err();
                    ++ncontinuable_errors;
                }

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
                    out << (lex.prepare_exception() << "warning: const and static methods aren't captured\n");
                    lex.clear_err();
                }

                if (extname) {
                    m->intname.takeover(m->name);
                    m->name.takeover(extname);
                }
                else
                    m->intname = m->name;

                m->basename = m->name;

                if (m->boperator) {
                    DASSERT(m->basename.ends_with("()"));
                    m->basename.resize(-2);
                    m->basename.trim() << '_';
                    if (m->bconst)
                        m->basename << "const";
                    else
                        m->basename << "nonconst";

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

                //if (!m->bstatic)
                //    ++lastifc->nifc_methods;

                if (m->bstatic && bdestroy) {
                    out << "error: method to call on interface release cannot be static\n";
                    lex.clear_err();
                    ++ncontinuable_errors;
                }

                if (m->bimplicit) {
                    if (m->name == "connect") {
                        /// @connect called when interface connects successfully
                        if (m->ret.type != "void" && m->args.size() != 0) {
                            out << (lex.prepare_exception() << "error: invalid format for connect method\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }
                        lastifc->on_connect = m->name = m->intname;
                    }
                    else if (m->name == "unload") {
                        /// @unload invoked when client dll/script is unloaded
                        lastifc->on_unload = m->name = m->intname;

                        if (!m->bstatic) {
                            out << (lex.prepare_exception() << "error: unload method must be static\n");
                            lex.clear_err();
                            ++ncontinuable_errors;
                        }

                        m->bcreator = false;
                    }
                    else {
                        out << (lex.prepare_exception() << "error: unrecognized implicit method\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    //ifc->method.pop();
                }

                m->bdestroy = bdestroy;

                if (bdestroy) {
                    //mark and move to the first pos
                    if (lastifc->destroy.name) {
                        out << (lex.prepare_exception() << "error: interface release method already specified\n");
                        lex.clear_err();
                        ++ncontinuable_errors;
                    }

                    lastifc->destroy = *m;
                    lastifc->method.move(m - lastifc->method.ptr(), 0, 1);
                    m = lastifc->method.ptr();
                }

                if (m->bcreator && m->args.size() == 0 && lastifc->default_creator.is_empty())
                    lastifc->default_creator = m->name;

                if (!lastifc->bnoscript && !m->bstatic && !binternal && !m->boperator) {
                    //check if another public method with the same name exists
                    MethodIG* mdup = lastifc->method.find_if([&](const MethodIG& mi) {
                        return !mi.bstatic && mi.name == m->name;
                    });
                    if (mdup != m) {
                        out << (lex.prepare_exception() << "error: overloaded methods not supported for scripting interfaces\n");
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
                        out << (lex.prepare_exception() << "warning: a matching duplicate method " << m->name << " not found in previous interfaces\n");
                        lex.clear_err();
                    }
                }
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
        out << (lex.prepare_exception() << "error: mixed interface types in host class `" << classname << "'\n");
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

    for (; b != e; ++b)
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
