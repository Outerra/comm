
#include "ig.h"

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after ifc_fn
bool MethodIG::parse( iglexer& lex, const charstr& host, const charstr& ns, dynarray<Arg>& irefargs )
{
    bstatic = lex.match_optional("static");

    //rettype fncname '(' ...

    if(!ret.parse(lex, false))
        throw lex.exc();

    bhasifctargets = !ret.ifctarget.is_empty();

    if(!bstatic && ret.biref)
        ret.add_unique(irefargs);

    biref=bptr=false;

    if(bstatic) {
        charstr tmp;

        if(ret.type == ((tmp="iref<")<<host<<'>')) {
            biref = true;
            (ret.type="iref<")<<ns<<host<<'>';
        }
        else if(ret.type == ((tmp="iref<")<<ns<<host<<'>'))
            biref = true;
        //else if(ret.type == ((tmp="ref<")<<ns<<host<<'>'))
        //    ;
        //else if(ret.type == ((tmp=ns)<<host<<'*'))
        //    bptr = true;
        else {
            lex.set_err() << "invalid return type for static interface method\n"
                "  should be iref<" << host << ">";
            throw lex.exc();
        }

        storage = ret.type;
    }

    lex.match(lex.IDENT, name, "expected method name");

    boperator = name == "operator";
    if(boperator) {
        lex.match('(');
        lex.match(')');
        name << "()";
    }

    binternal = binternal || name.first_char() == '_';

    lex.match('(');

    ninargs = noutargs = 0;

    if(!lex.matches(')')) {
        do {
            if( !args.add()->parse(lex, true) )
                throw lex.exc();

            Arg* arg = args.last();

            if(arg->binarg) {
                ++ninargs;
                if(!arg->defval)
                    ++ninargs_nondef;
            }
            if(arg->boutarg)
                ++noutargs;

            arg->tokenpar = arg->binarg && 
                (arg->basetype=="token" || arg->basetype=="coid::token" || arg->basetype=="charstr" || arg->basetype=="coid::charstr");

            if(!bstatic && arg->biref && arg->boutarg)
                arg->add_unique(irefargs);

            if(arg->ifctarget)
                bhasifctargets = true;
        }
        while(lex.matches(','));
        
        lex.match(')');
    }

    bconst = lex.matches("const");

    //optional default event impl
    bool evbody = lex.matches(lex.IFC_EVBODY);
    if(evbody)
    {
        default_event_body = lex.match_block(lex.ROUND, true);

        if(default_event_body.first_char() == '"') {
            default_event_body.del(0, 1);
            if(default_event_body.last_char() == '"')
                default_event_body.resize(-1);
        }

        if(default_event_body.first_char() != '{') {
            default_event_body.ins(0, '{');
            default_event_body << '}';
        }
    }
    //else
    //    default_event_body = "{ throw coid::exception(\"handler not implemented\"); }";

    bmandatory = !evbody && (ret.type != "void" || noutargs > 0);

    //declaration parsed successfully
    return lex.no_err();
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
                *paramdoc << "\n\n";
            else if(doc)
                doc << "\n\n";
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
                token param = paramline.cut_left(token::TK_whitespace());
                Arg* a = find_arg(param);

                if(a) {
                    paramdoc = &a->doc;
                    if(!paramdoc->is_empty())
                        *paramdoc << '\n';
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
                    ret.doc << '\n';
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

////////////////////////////////////////////////////////////////////////////////
charstr& MethodIG::Arg::match_type( iglexer& lex, charstr& type )
{
    bool nested;
    do {
        type << lex.match(lex.IDENT, "expected type name");

        if(lex.matches('<'))
            type << char('<') << lex.next_as_block(lex.ANGLE) << char('>');

        nested = lex.matches("::");
        if(nested)
            type << "::";
    } while(nested);

    return type;
}

////////////////////////////////////////////////////////////////////////////////
bool MethodIG::Arg::parse( iglexer& lex, bool argname )
{
    //arg: [ifc_in|ifc_out|ifc_inout|] [const] type  
    //type: [class[<templarg>]::]* type[<templarg>] [[*|&] [const]]*

    int io = lex.matches_either("ifc_in","ifc_out","ifc_inout","ifc_ret");
    if(!io) io=1;

    //ifc_return(ifcname)
    if(io == 4) {
        lex.match('(');
        match_type(lex, ifctarget);
        lex.match(')');

        io = 2;
    }

    binarg = (io&1) != 0;
    boutarg = (io&2) != 0;

    bvolatile = lex.matches("ifc_volatile");

    bconst = lex.matches("const");
    benum = lex.matches("enum");

    if(lex.matches("unsigned"))
        type << "unsigned" << ' ';

    match_type(lex, type);

    int isPR=0,wasPR;
    int isCV=0;

    while((wasPR = lex.matches_either('*', '&')))
    {
        isPR = wasPR;
        if(bconst) {
            //const is a part of the inner type
            type.ins(0, "const ");
            bconst = 0;
        }

        type << lex.last().value();

        if((isCV = lex.matches_either("const", "volatile")))
            type << ' ' << lex.last().value();
    }

    if(isCV == 1)   //ends with const
        bconst = true;
    if(isCV)
        type.resize(isCV==1 ? 6 : 9);   //strip the last const and volatile

    bptr = isPR == 1;
    bref = isPR == 2;

    if(boutarg && ((!bptr && !bref && !ifctarget) || type.begins_with("const "))) {
        lex.set_err() << "out argument must be a ref or ptr and cannot be const\n";
    }

    basetype = type;
    if(isPR)
        basetype.shift_end(-1);

    if(basetype.begins_with("const ")) basetype.shift_start(6);
    else if(basetype.ends_with(" const")) basetype.shift_end(-6);

    //special handling for strings and tokens
    if(type == "const char*") {
        basetype = "coid::charstr";
        base2arg = ".c_str()";
    }
    else if(basetype == "token" || basetype == "coid::token") {
        basetype = "coid::charstr";
    }

    biref = basetype.begins_with("iref<");
    if(biref) {
        token bs = basetype;
        bs.shift_start(5);
        bs.cut_right_back('>');

        do {
            token p = bs.cut_left("::");
            fulltype << p;
            if(bs)
                fulltype << '_';
        }
        while(bs);
    }
    else if(ifctarget)
        lex.set_err() << "ifc_ret argument must be an iref<>\n";

    if(ifctarget) {
        type.swap(ifctarget);
        type.ins(0, "iref<");
        type.append('>');
        basetype.set(type.ptr()+5, type.ptre()-1);

        //remove iref from ifctarget
        ifctarget.del(0, 5);
        ifctarget.del(-1, 1);
    }


    if(!argname)
        return lex.no_err();


    lex.match(lex.IDENT, name, "expecting argument name");
    
    //match array
    if(lex.matches('[') )
        size << lex.next_as_block(lex.SQUARE);

    bnojs = false;

    //match default value
    if(lex.matches('=')) {
        //match everything up to a comma or a closing parenthesis
        lex.enable(lex.SQUARE, true);
        lex.enable(lex.ROUND, true);
        do {
            const lexer::lextoken& tok = lex.next();
            
            if( tok == lex.SQUARE || tok == lex.ROUND ) {
                lex.complete_block();
                defval << tok.leading_token(lex) << tok << tok.trailing_token(lex);
                continue;
            }
            if( tok == ','  ||  tok == ')'  ||  tok.end() ) {
                lex.push_back();
                break;
            }

            defval << tok;
        }
        while(1);
        lex.enable(lex.SQUARE, false);
        lex.enable(lex.ROUND, false);

        bnojs = name.first_char() == '_';
    }

    return lex.no_err();
}

////////////////////////////////////////////////////////////////////////////////
void MethodIG::Arg::add_unique( dynarray<MethodIG::Arg>& irefargs )
{
    Arg* ps = irefargs.ptr();
    Arg* pe = irefargs.ptre();

    for(; ps<pe; ++ps) {
        if(ps->fulltype == fulltype) return;
    }

    Arg& arg = *irefargs.add();
    arg = *this;

    char x = arg.type.last_char();
    if(x=='&' || x=='*')
        arg.type.resize(-1);
}
