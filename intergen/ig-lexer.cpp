
#include "ig.h"


////////////////////////////////////////////////////////////////////////////////
iglexer::iglexer()
{
    def_group("ignore", " \t\n\r");

    IDENT = def_group("identifier", "_a..zA..Z", "a..zA..Z_0..9");
    NUM = def_group("number", "0..9");
    def_group("operator", ":!%^-+?/|");
    def_group_single("separator", "=&*~.;,<>()[]{}");

    def_keywords("const:volatile");

    RLCMD = def_keywords("rl_cmdr:rl_cmdr_p:rl_cmdr_s:rl_cmdp:rl_cmdp_p:rl_cmdp_s:rl_cmdi:rl_cmdi_p:rl_cmdi_s");

    IGKWD = def_keywords("\
ifc_class:ifc_class_var:ifc_class_virtual:ifc_class_virtual_var:ifc_struct:\
IFC_CLASS:IFC_CLASS_VAR:IFC_CLASS_VIRTUAL:IFC_CLASS_VIRTUAL_VAR:IFC_STRUCT:\
ifc_creator:ifc_creatorx:ifc_fn:ifc_fnx:ifc_event:ifc_eventx:\
ifc_in:ifc_out:ifc_inout:ifc_ret");

    int ie = def_escape("escape", '\\', 0);
    def_escape_pair(ie, "\\", "\\");
    def_escape_pair(ie, "\"", "\"");
    def_escape_pair(ie, "\'", "\'");
    def_escape_pair(ie, "n", "\n");
    def_escape_pair(ie, "r", "\r");
    def_escape_pair(ie, "\r\n", token());
    def_escape_pair(ie, "\n", token());

    SQSTRING = def_string(".sqstring", "'", "'", "escape");
    DQSTRING = def_string(".dqstring", "\"", "\"", "escape");


    //ignore strings, comments and macros
    SLCOM = def_string(".comment", "//", "\n", "escape");
    def_string(".comment", "//", "\r\n", "escape");
    def_string(".comment", "//", "\r", "escape");
    def_string(".comment", "//", "", "escape");

    def_string(".macro", "#", "\n", "escape");
    def_string(".macro", "#", "\r\n", "escape");
    def_string(".macro", "#", "\r", "escape");
    def_string(".macro", "#", "", "escape");

    IFC_LINE_COMMENT = def_block("ifc1", "//ifc{", "//}ifc", "comment blkcomment");
    IFC_BLOCK_COMMENT = def_block("ifc2", "/*ifc{", "}ifc*/", "comment blkcomment");

    IFC_DISPATCH_LINE_COMMENT = def_block("ifcd1", "//ifc-dispatch{", "//}ifc-dispatch", "");
    IFC_DISPATCH_BLOCK_COMMENT = def_block("ifcd2", "/*ifc-dispatch{", "}ifc-dispatch*/", "");

    MLCOM = def_block(".blkcomment", "/*", "*/", "");

    ANGLE = def_block("!angle", "<", ">", "angle .comment .blkcomment");    //by default disabled
    SQUARE = def_block("!square", "[", "]", "square .comment .blkcomment");
    ROUND = def_block("!round", "(", ")", "round .comment .blkcomment");
    CURLY = def_block("curly", "{", "}", "curly ifc1 ifc2 .comment .blkcomment .macro");
}

////////////////////////////////////////////////////////////////////////////////
int iglexer::find_method(const token& classname, dynarray<paste_block>& classpasters, dynarray<nested_type>& nested_types, dynarray<charstr>& comment_list)
{
    //DASSERT( ignored(CURLY) ); //not to catch nested {}

    const lextoken& tok = last();
    uint nv = 0;

    do {
        ignore(MLCOM, false);
        ignore(SLCOM, false);

        int ic = matches_either(IFC_LINE_COMMENT, IFC_BLOCK_COMMENT);
        if (ic)
        {
            complete_block();

            token ifc_block = tok;
            ifc_block.skip_space().trim_whitespace();
            token cond = ifc_block.get_line();
            ifc_block.skip_whitespace();

            //if ifc_block contains enum/class/struct nested classes, we need to register those and mark method arguments that use them
            bool has_nested = ifc_block.contains("enum") || ifc_block.contains("class") || ifc_block.contains("struct");
            if (has_nested)
            {
                iglexer nestlex;
                nestlex.bind(ifc_block);

                while (1)
                {
                    const lextoken& ntok = nestlex.next();
                    if (ntok.end())
                        break;
                    int ecs = 0;
                    if (ntok == "enum"_T) ecs = 1;
                    else if (ntok == "class"_T) ecs = 2;
                    else if (ntok == "struct"_T) ecs = 3;
                    if (!ecs)
                        continue;

                    if (ecs == 1)
                        nestlex.matches("class"); //enum class

                    token type_name = nestlex.match(IDENT);

                    nested_type& nt = *nested_types.add();
                    nt.type_name = type_name;
                    nt.ecs = ecs;
                }
            }

            paste_block* pb = classpasters.add();
            pb->block = ifc_block;
            //pb->namespc = namespc;
            pb->pos = paste_block::position::inside_class;
            pb->in_dispatch = false;
            pb->condx = cond;

            ignore(MLCOM, true);
            ignore(SLCOM, true);
            continue;
        }

        int mc = matches_either(SLCOM, MLCOM);
        if (mc == 2)
            complete_block();

        if (mc)
        {
            charstr& txt = comment_list.get_or_add(nv++);
            txt.reset();

            token t = last().val;
            char k = t.first_char();

            if (mc == 2 && (k == '*' || k == '!')) {
                ++t;
                t.trim_char('*');
                txt << "/**" << t << "**/";
            }
            else if (mc == 1 && (k == '/' || k == '@' || k == '!')) {
                ++t;
                if (k == '!')
                    k = '@';
                txt << "//" << k << t;
            }
        }

        ignore(MLCOM, true);
        ignore(SLCOM, true);

        if (mc)
            continue;

        if (matches(RLCMD)) {
            return tok.termid + 1;
        }
        else if (matches(IGKWD)) {
            comment_list.resize(nv);
            return -1 - tok.termid;
        }
        else if (matches('{')) {
            complete_block();
            continue;
        }
        else {
            nv = 0;
            //produce error for old/non-existing ifc keywords
            if (tok.value().begins_with("ifc_")) {
                syntax_err() << "unrecognized ifc keyword encountered: " << tok.value();
                throw exc();
            }
        }

        next();
    }
    while (tok.id && !tok.trailing(CURLY));

    return 0;
}
