
#include "ig.h"

////////////////////////////////////////////////////////////////////////////////
///Parse function declaration after ifc_fn
bool MethodIG::parse( iglexer& lex, const charstr& host, const charstr& ns, dynarray<Arg>& irefargs )
{
    bstatic = lex.match_optional("static");

    //rettype fncname '(' ...

    if(!ret.parse(lex, false))
        throw lex.exc();

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
        }
        while(lex.matches(','));
        
        lex.match(')');
    }

    bconst = lex.matches("const");

    //declaration parsed succesfully
    return lex.no_err();
}

////////////////////////////////////////////////////////////////////////////////
bool MethodIG::Arg::parse( iglexer& lex, bool argname )
{
    //arg: [ifc_in|ifc_out|ifc_inout|] [const] type  
    //type: [class[<templarg>]::]* type[<templarg>] [[*|&] [const]]*

    int io = lex.matches_either("ifc_in","ifc_out","ifc_inout");
    if(!io) io=1;

    binarg = (io&1) != 0;
    boutarg = (io&2) != 0;

    bconst = lex.matches("const");
    benum = lex.matches("enum");

    bool nested;
    do {
        type << lex.match(lex.IDENT, "expected type name");

        if(lex.matches('<'))
            type << char('<') << lex.next_as_block(lex.ANGLE) << char('>');

        nested = lex.matches("::");
        if(nested)
            type << "::";
    } while(nested);

    int isPR=0,wasPR;
    int isCV=0;

    while(wasPR = lex.matches_either('*', '&'))
    {
        isPR = wasPR;
        if(bconst) {
            //const is a part of the inner type
            type.ins(0, "const ");
            bconst = 0;
        }

        type << lex.last().value();

        if(isCV = lex.matches_either("const", "volatile"))
            type << ' ' << lex.last().value();
    }

    if(isCV == 1)   //ends with const
        bconst = true;
    if(isCV)
        type.resize(isCV==1 ? 6 : 9);   //strip the last const and volatile

    bptr = isPR == 1;
    bref = isPR == 2;

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

    if(!argname)
        return true;


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
                defval << lex.complete_block().outer();
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

    return true;
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
