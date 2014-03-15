
#include "ig.h"


////////////////////////////////////////////////////////////////////////////////
iglexer::iglexer()
{
    def_group( "ignore", " \t\n\r" );

    IDENT   = def_group( "identifier", "_a..zA..Z", "a..zA..Z_0..9" );
    NUM     = def_group( "number", "0..9" );
    def_group( "operator", ":!%^-+?/|" );
    def_group_single( "separator", "=&*~.;,<>()[]{}" );

    def_keywords( "const:volatile" );

    RLCMD = def_keywords( "rl_cmdr:rl_cmdr_p:rl_cmdr_s:rl_cmdp:rl_cmdp_p:rl_cmdp_s:rl_cmdi:rl_cmdi_p:rl_cmdi_s" );
        
    IGKWD = def_keywords( "ifc_class:ifc_class_var:ifc_class_virtual:ifc_fn:ifc_fnx:ifc_event:ifc_eventx:ifc_in:ifc_out:ifc_inout" );

    int ie = def_escape( "escape", '\\', 0 );
    def_escape_pair( ie, "\\", "\\" );
    def_escape_pair( ie, "\"", "\"" );
    def_escape_pair( ie, "\'", "\'" );
    def_escape_pair( ie, "n", "\n" );
    def_escape_pair( ie, "\r\n", 0 );
    def_escape_pair( ie, "\n", 0 );

    SQSTRING = def_string( ".sqstring", "'", "'", "escape" );
    DQSTRING = def_string( ".dqstring", "\"", "\"", "escape" );


    //ignore strings, comments and macros
    SLCOM = def_string( ".comment", "//", "\n", "escape" );
    def_string( ".comment", "//", "\r\n", "escape" );
    def_string( ".comment", "//", "\r", "escape" );
    def_string( ".comment", "//", "", "escape" );

    def_string( ".macro", "#", "\n", "escape" );
    def_string( ".macro", "#", "\r\n", "escape" );
    def_string( ".macro", "#", "\r", "escape" );
    def_string( ".macro", "#", "", "escape" );

    IFC1 = def_block( "ifc1", "//ifc{", "//}ifc", "" );
    IFC2 = def_block( "ifc2", "/*ifc{", "}ifc*/", "" );

    MLCOM = def_block( ".blkcomment", "/*", "*/", "" );

    ANGLE = def_block( "!angle", "<", ">", "angle .comment .blkcomment" );    //by default disabled
    SQUARE = def_block( "!square", "[", "]", "square .comment .blkcomment" );
    ROUND = def_block( "!round", "(", ")", "round .comment .blkcomment" );
    CURLY = def_block( "curly", "{", "}", "curly .comment .blkcomment .macro" );
}

////////////////////////////////////////////////////////////////////////////////
int iglexer::find_method( const token& classname, dynarray<charstr>& commlist )
{
    DASSERT( ignored(CURLY) ); //not to catch nested {}

    const lextoken& tok = last();
    uint nv=0;

    do {
        ignore(MLCOM, false);
        int c = matches_either(SLCOM,MLCOM);
        if(c == 2)
            complete_block();
        ignore(MLCOM, true);

        if(c) {
            charstr& txt = commlist.get_or_add(nv++);
            txt.reset();

            token t = last().tok;
            char k = t.first_char();

            if(c == 2 && (k == '*' || k == '!'))
                txt << "/*" << t << "*/";
            else if(c == 1 && (k == '/' || k == '@' || k == '!'))
                txt << "//" << t;

            continue;
        }
        else if(matches(RLCMD)) {
            return tok.termid+1;
        }
        else if(matches(IGKWD)) {
            commlist.resize(nv);
            return -1-tok.termid;
        }
        else
            nv=0;

        next();
    }
    while( tok.id  &&  !tok.trailing(CURLY) );

    return 0;
}
