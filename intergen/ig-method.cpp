
#include "ig.h"

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after ifc_fn
bool MethodIG::parse(iglexer& lex, const charstr& host, const charstr& ns, const charstr& extifc, dynarray<forward>& fwds, bool isevent, bool iscreator)
{
    file = lex.get_current_file();
    line = lex.current_line();

    bstatic = lex.matches("static");
    bvirtual = lex.matches("virtual");

    //rettype fncname '(' ...

    if (!ret.parse(lex, false))
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

        if (!fwds.contains(fwd))
            fwds.push(std::move(fwd));
    }


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

    lex.match(lex.IDENT, name, "expected method name");

    boperator = name == "operator";
    if (boperator) {
        lex.match('(');
        lex.match(')');
        name << "()";
    }

    binternal = binternal || name.first_char() == '_';

    lex.match('(');

    ninargs = noutargs = 0;

    if (!lex.matches(')')) {
        do {
            if (!args.add()->parse(lex, true))
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

    //declaration parsed successfully
    return lex.no_err() && ncontinuable_errors == 0;
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
