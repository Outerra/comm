
#include "ig.h"

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after ifc_fn
bool MethodIG::parse(iglexer& lex, const charstr& host, const charstr& ns, const charstr& nsifc, dynarray<Arg>& irefargs, bool isevent)
{
    bstatic = lex.match_optional("static");

    //rettype fncname '(' ...

    if (!ret.parse(lex, false))
        throw lex.exc();

    if (!bstatic && ret.biref)
        ret.add_unique(irefargs);

    biref = bptr = bifccr = false;
    int ncontinuable_errors = 0;

    if (bstatic && !bimplicit) {
        charstr tmp;
        bcreator = true;

        if (ret.bptr && ret.type == ((tmp = host) << '*')) {
            (ret.type = "iref<") << ns << host << '>';
        }
        else if (ret.type == ((tmp = "iref<") << host << '>')) {
            biref = true;
            (ret.type = "iref<") << ns << host << '>';
        }
        else if (ret.type == ((tmp = "iref<") << ns << host << '>')) {
            biref = true;
        }
        else if (ret.type == ((tmp = "iref<") << nsifc << '>')) {
            biref = true;
            bifccr = true;
        }
        else {
            out << (lex.prepare_exception()
                << "error: invalid return type for static interface creator method\n  should be iref<"
                << host << ">");
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
                (arg->basetype=="token" || arg->basetype=="coid::token" || arg->basetype=="charstr" || arg->basetype=="coid::charstr");

            if (!bstatic && arg->biref && (isevent ? arg->binarg : arg->boutarg))
                arg->add_unique(irefargs);
        }
        while (lex.matches(','));

        lex.match(')');
    }

    bconst = lex.matches("const");
    bool boverride = lex.matches("override");

    //optional default event impl
    int evbody = lex.matches_either(lex.IFC_DEFAULT_EMPTY, lex.IFC_DEFAULT_BODY, lex.IFC_EVBODY);
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

    bnoevbody = !evbody && (ret.type != "void" || noutargs > 0);

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

    for(; b!=e; ++b)
    {
        token line = *b;

        line.trim();
        line.skip_char('/');
        line.trim();

        if(!line) {
            if(paramdoc)
                *paramdoc << "\r\n\r\n";
            else if(doc)
                doc << "\r\n\r\n";
            continue;
        }

        if(line.first_char() != '@') {
            charstr& target = paramdoc ? *paramdoc : doc;
            if(!target.is_empty())
                target << ' ';
            //target.append_escaped(line);
            target << line;
        }
        else {
            //close previous string
            if(paramdoc)
                paramdoc = 0;
            else if(doc)
                docs.add()->swap(doc);

            token paramline = line;
            if(paramline.consume("@param ")) {
                token param = paramline.cut_left_group(" \t\n\r"_T);
                Arg* a = find_arg(param);

                if(a) {
                    paramdoc = &a->doc;
                    if(!paramdoc->is_empty())
                        *paramdoc << "\r\n";
                    //paramdoc->append_escaped(paramline);
                    *paramdoc << paramline;
                }
                else {
                    //doc.append_escaped(line);
                    doc << line;
                }
            }
            else if(paramline.consume("@return ")) {
                if(ret.doc)
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

    if(doc)
        docs.add()->swap(doc);
}
