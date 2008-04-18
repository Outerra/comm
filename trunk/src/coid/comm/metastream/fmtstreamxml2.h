/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Brano Kemen
 *
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __COID_COMM_FMTSTREAMXML2__HEADER_FILE__
#define __COID_COMM_FMTSTREAMXML2__HEADER_FILE__

#include "../namespace.h"
#include "../str.h"
#include "../lexer.h"
#include "fmtstream.h"



COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class fmtstreamxml2 : public fmtstream
{
protected:
    binstream* _binr;
    binstream* _binw;

    charstr _bufw;
    lexer _tokenizer;

    int lexid, lexstr, lexchr, lextag;

public:
    fmtstreamxml2() : _tokenizer(true)  {init(0,0);}
    fmtstreamxml2( binstream& b ) : _tokenizer(true)  {init(&b, &b);}
    fmtstreamxml2( binstream* br, binstream* bw ) : _tokenizer(true)  {init(br, bw);}
    ~fmtstreamxml2() {}

    void init( binstream* br, binstream* bw )
    {
        _binr = _binw = 0;
        if(bw)  bind( *bw, BIND_OUTPUT );
        if(br)  bind( *br, BIND_INPUT );

        _tokenizer.def_group( "", " \t\r\n" );
        _tokenizer.def_group_single( "sep", "/=" );

        //add anything that can be a part of identifier or value (strings are treated separately)
        lexid = _tokenizer.def_group( "id", "a..zA..Z_:", "0..9a..zA..Z_:-." );

        int er = _tokenizer.def_escape( "esc", '&' );
        _tokenizer.def_escape_pair( er, "lt;", "<" );
        _tokenizer.def_escape_pair( er, "gt;", ">" );
        _tokenizer.def_escape_pair( er, "amp;", "&" );
        _tokenizer.def_escape_pair( er, "apos;", "'" );
        _tokenizer.def_escape_pair( er, "quot;", "\"" );

        lexstr = _tokenizer.def_string( "str", "\"", "\"", "esc" );
        lexchr = _tokenizer.def_string( "str", "\'", "\'", "esc" );

        //tag block, cannot have nested tags but can contain strings
        lextag = _tokenizer.def_block( "tag", "<", ">", "str" );
    }

    lexer& get_lexer() {
        return _tokenizer;
    }

};

COID_NAMESPACE_END


#endif  // ! __COID_COMM_FMTSTREAMXML2__HEADER_FILE__
