
#include "ig.h"

////////////////////////////////////////////////////////////////////////////////
charstr& MethodIG::Arg::match_type(iglexer& lex, charstr& type)
{
    if (lex.matches("::"))
        type = "::";
    else
        type.reset();

    bool nested;
    do {
        type << lex.match(lex.IDENT, "expected type name");

        if (lex.matches('<'))
            type << char('<') << lex.next_as_block(lex.ANGLE) << char('>');

        nested = lex.matches("::");
        if (nested)
            type << "::";
    }
    while (nested);

    return type;
}

////////////////////////////////////////////////////////////////////////////////
bool MethodIG::Arg::parse(iglexer& lex, int8 argname, bool no_fn_arg)
{
    //arg: [ifc_in|ifc_out|ifc_inout|] [const] type
    //type: [class[<templarg>]::]* type[<templarg>] [[*|&] [const]]*

    int io = lex.matches_either("ifc_in", "ifc_out", "ifc_inout");
    if (!io) io = 1;

    if (io == 2)
        ifckwds = "ifc_out";
    else if (io == 3)
        ifckwds = "ifc_inout";

    binarg = (io & 1) != 0;
    boutarg = (io & 2) != 0;

    bvolatile = lex.matches("ifc_volatile");

    if (bvolatile) {
        if (ifckwds)
            ifckwds << ' ';
        ifckwds << "ifc_volatile";
    }

    bconst = lex.matches("const");
    struct_type = (meta::arg::ex_type)lex.matches_either("enum", "struct", "class");

    bool bunsigned = lex.matches("unsigned");

    match_type(lex, type);
    if (bunsigned)
        type.ins(0, "unsigned ");

    int isPR = 0, wasPR;    // * 1, & 2, && 3
    int isCV = 0;

    //parse a sequence of pointer and reference specifications
    while ((wasPR = lex.matches_either('*', '&')))
    {
        isPR = isPR == 2 && wasPR == 2 ? 3 : wasPR;

        if (bconst) {
            //const was a part of the inner type, move to the beginning
            type.ins(0, "const ");
            bconst = 0;
        }

        type << lex.last().value();

        if ((isCV = lex.matches_either("const", "volatile"))) {
            type << ' ' << lex.last().value();
            isPR = 0;
        }
    }

    if (isCV == 1)   //ends with const
        bconst = true;
    if (isCV)
        type.resize(isCV == 1 ? -6 : -9);   //strip the last const and volatile

    bxref = isPR == 3;
    bptr = isPR == 1;
    bref = bxref || isPR == 2;

    if (boutarg && ((!bptr && !bref) || type.begins_with("const "))) {
        out << (lex.prepare_exception()
            << "error: out argument must be a ref or ptr and cannot be const\n");
        lex.clear_err();
    }

    token tbasetype = type;
    if (isPR)
        tbasetype.shift_end(isPR == 3 ? -2 : -1);

    if (tbasetype.begins_with("const ")) tbasetype.shift_start(6);
    else if (tbasetype.ends_with(" const")) tbasetype.shift_end(-6);

    bspecptr = false;

    //special handling for strings and tokens
    if (type == "const char*") {
        tbasetype = type;
        bspecptr = true;
    }


    if (!no_fn_arg && lex.matches('(')) {
        //a function argument [type] (*name)(arg1[,arg2]*)
        if (!lex.matches('*')) {
            //possibly a member fn
            lex.match(lex.IDENT, memfnclass, "expecting class name");

            lex.match("::");
            lex.match('*');
        }

        lex.match(lex.IDENT, name, "expecting argument name");
        lex.match(')');

        //parse argument list as a block
        fnargs = lex.match_block(lex.ROUND, true);
        bfnarg = true;
    }
    else if (argname > 0) {
        lex.match(lex.IDENT, name, "expecting argument name");
    }
    else if (argname < 0) {
        argname = lex.matches(lex.IDENT, name) ? 1 : 0;
    }

    //match array
    if (argname && lex.matches('[')) {
        if (bconst) {
            type.ins(0, "const ");
            tbasetype = token(type.ptr() + 6, type.ptre());
            bconst = 0;
        }
        arsize = '[';
        arsize << lex.next_as_block(lex.SQUARE);
        arsize << ']';
    }

    basetype = tbasetype;

    biref = tbasetype.begins_with("iref<");
    bcref = tbasetype.begins_with("coref<");

    if (biref || bcref) {
        token bs = tbasetype;
        bs.shift_start(5);
        bs.cut_right_back('>');

        ptrtype = bs.trim();
    }
    else if (bptr) {
        ptrtype = tbasetype;
    }

    bnoscript = false;

    //match default value
    if (argname && lex.matches('=')) {
        //match everything up to a comma or a closing parenthesis
        bool was_square = lex.enable(lex.SQUARE, true);
        bool was_round = lex.enable(lex.ROUND, true);
        bool was_curly = lex.enable(lex.CURLY, true);
        do {
            const lexer::lextoken& tok = lex.next();

            if (tok == lex.SQUARE || tok == lex.ROUND || tok == lex.CURLY) {
                lex.complete_block();
                defval << tok.leading_token(lex) << tok << tok.trailing_token(lex);
                continue;
            }
            if (tok == ',' || tok == ')' || tok.end()) {
                lex.push_back();
                break;
            }

            defval << tok;
        }
        while (1);

        lex.enable(lex.CURLY, was_curly);
        lex.enable(lex.SQUARE, was_square);
        lex.enable(lex.ROUND, was_round);

        //arguments starting with underscore that have default value are not used in script
        bnoscript = name.first_char() == '_';
    }

    return lex.no_err();
}
