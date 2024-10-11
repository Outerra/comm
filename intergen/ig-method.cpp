
#include "ig.h"

#include "../dir.h"

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after ifc_fn
bool MethodIG::parse(iglexer& lex, const charstr& host, const charstr& ns, const charstr& extifc, Interface* ifc, method_type mtype)
{
    file = lex.get_current_file();
    line = lex.current_line();

    bstatic = lex.matches("static");
    bvirtual = lex.matches("virtual");

    bool is_callback = mtype == method_type::callback;

    //rettype fncname '(' ...

    if (!ret.parse(lex, 0, is_callback))
        throw lex.exc();

    ret.name = "return";

    if (extifc) {
        ret.hosttype = ret.ptrtype;

        ret.ptrtype = extifc;
        ret.basetype = extifc;
        ret.type = ret.ifc_type == meta::arg::ifc_type::ifc_class
            ? "iref<"_T
            : "coref<"_T;
        ret.type << extifc << '>';

        forward fwd;
        fwd.type = ret.ifc_type;
        fwd.parse(extifc);

        if (!ifc->fwds.contains(fwd))
            ifc->fwds.push(std::move(fwd));
    }

    bool iscreator = mtype == method_type::creator;

    biref = bptr = false;
    int ncontinuable_errors = 0;

    if (iscreator || (bstatic && !bimplicit)) {
        charstr tmp;

        if (ret.bptr && ret.type == ((tmp = host) << '*')) {
            (ret.type = "iref<") << ns << host << '>';
            bcreator = true;
        }
        else if (ret.basetype == ((tmp = "iref<") << host << '>')) {
            (ret.type = "iref<") << ns << host << '>';
            biref = true;
            bcreator = true;
        }
        else if (ret.basetype == ((tmp = "iref<") << ns << host << '>')) {
            biref = true;
            bcreator = true;
        }
        else if (iscreator) {
            out << (lex.prepare_exception()
                << "error: invalid return type for static interface creator method, should be iref<" << host << ">\n");
            lex.clear_err();
            ++ncontinuable_errors;
        }

        storage = ret.type;
    }

    if (is_callback) {
        if (!lex.matches('(') || !lex.matches('*') || !lex.matches(')')) {
            out << (lex.prepare_exception()
                << "error: expected `return_type (*)(...)' style callback declaration\n");
            lex.clear_err();
            ++ncontinuable_errors;
        }
    }
    else {
        lex.match(lex.IDENT, name, "expected method name");

        boperator = name == "operator";
        if (boperator) {
            lex.match('(');
            lex.match(')');
            name << "()";
        }

        binternal = binternal || name.first_char() == '_';
    }

    lex.match('(');

    ninargs = noutargs = 0;

    if (!lex.matches(')')) {
        do {
            if (!args.add()->parse(lex, is_callback ? -1 : 1, false))
                throw lex.exc();

            Arg* arg = args.last();

            if (arg->binarg) {
                ++ninargs;
                if (!arg->defval)
                    ++ninargs_nondef;
            }
            if (arg->boutarg)
                ++noutargs;

            arg->tokenpar = arg->binarg &&
                (arg->basetype == "token" || arg->basetype == "coid::token" || arg->basetype == "charstr" || arg->basetype == "coid::charstr");
        }
        while (lex.matches(','));

        lex.match(')');
    }

    bool retvoid = ret.type == "void";

    nalloutargs = noutargs;
    if (!retvoid)
        ++nalloutargs;
    bmultioutargs = nalloutargs > 1;

    bconst = lex.matches("const");
    bool boverride = lex.matches("override");

    //optional default event impl
    int evbody = lex.matches_either("ifc_default_empty"_T, "ifc_default_body"_T, "ifc_evbody"_T);
    if (evbody)
    {
        if (evbody > 1) {
            token text = lex.match_block(lex.ROUND, true);
            text.consume_char('"');
            text.consume_end_char('"');

            default_event_body.reset().append_unescaped(text);
        }
        else
            default_event_body = ' ';
    }

    bnoevbody = !evbody && nalloutargs > 0;

    for (Arg& arg : args) {
        const MethodIG* callback = ifc->class_ptr->callback.contains(arg.basetype, [](const MethodIG& m, const charstr& type) {
            return m.name == type;
        });

        if (callback)
            arg.bcallback = true;
    }

    //declaration parsed successfully
    return lex.no_err() && ncontinuable_errors == 0;
}

////////////////////////////////////////////////////////////////////////////////
bool MethodIG::parse_callback(iglexer& lex, const charstr& namespc, dynarray<MethodIG>& callbacks)
{
    int ncontinuable_errors = 0;
    charstr extname;

    lex.match('(');
    lex.match(lex.IDENT, extname);
    lex.match(',');

    MethodIG* m = callbacks.add();

    //m->comments.takeover(commlist);
    //m->classname << namespc << classname;

    //m->bretifc = extfn_class || extfn_struct;
    //if (m->bretifc)
    //    m->ret.ifc_type = extfn_class ? meta::arg::ifc_type::ifc_class : meta::arg::ifc_type::ifc_struct;

    if (!m->parse(lex, "", namespc, "", nullptr, MethodIG::method_type::callback))
        ++ncontinuable_errors;

    const MethodIG* old = callbacks.find_if([m](const MethodIG& o) {
        return &o != m && o.name == m->name && m->matches_args(o);
    });

    if (old) {
        charstr relpath;
        directory::get_relative_path(m->file, old->file, relpath, true);
        out << (lex.prepare_exception() << "callback already declared in " << relpath << "(" << old->line << "): " << m->name << "\n");
        lex.clear_err();
        ++ncontinuable_errors;
    }

    if (extname) {
        m->intname.takeover(m->name);
        m->name.takeover(extname);
    }
    else
        m->intname = m->name;

    m->basename = m->name;

    m->parse_docs();
    return true;
}

////////////////////////////////////////////////////////////////////////////////
void MethodIG::parse_docs()
{
    auto b = comments.ptr();
    auto e = comments.ptre();
    charstr doc;
    charstr* paramdoc = 0;

    for (; b != e; ++b)
    {
        token line = *b;

        line.trim();
        line.skip_char('/');
        line.trim();

        if (!line) {
            if (paramdoc)
                *paramdoc << "\r\n\r\n";
            else if (doc)
                doc << "\r\n\r\n";
            continue;
        }

        if (line.first_char() != '@') {
            charstr& target = paramdoc ? *paramdoc : doc;
            if (!target.is_empty())
                target << ' ';
            //target.append_escaped(line);
            target << line;
        }
        else {
            //close previous string
            if (paramdoc)
                paramdoc = 0;
            else if (doc)
                docs.add()->swap(doc);

            token paramline = line;
            if (paramline.consume("@param ")) {
                token param = paramline.cut_left_group(" \t\n\r"_T);
                Arg* a = find_arg(param);

                if (a) {
                    paramdoc = &a->doc;
                    if (!paramdoc->is_empty())
                        *paramdoc << "\r\n";
                    //paramdoc->append_escaped(paramline);
                    *paramdoc << paramline;
                }
                else {
                    //doc.append_escaped(line);
                    doc << line;
                }
            }
            else if (paramline.consume("@return ")) {
                if (ret.doc)
                    ret.doc << "\r\n";
                //ret.doc.append_escaped(paramline);
                ret.doc << paramline;
            }
            else {
                //doc.append_escaped(line);
                doc << line;
            }
        }
    }

    if (doc)
        docs.add()->swap(doc);
}
