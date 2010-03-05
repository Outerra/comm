
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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

/** @file */

#ifndef __COID_COMM_METAGEN__HEADER_FILE__
#define __COID_COMM_METAGEN__HEADER_FILE__

#include "../namespace.h"
#include "../str.h"
#include "../binstream/binstream.h"
#include "../local.h"
#include "../lexer.h"

#include "metavar.h"
#include "fmtstreamcxx.h"   //temporary

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Class metagen - generating complex documents from objects using document template.
/**
    It serves as a formatting stream for the metastream class too, so it should
    inherit from binstream base class

    
**/
class metagen //: public binstream
{
    struct Tag;
    typedef local<Tag>      Ltag;
    typedef MetaDesc::Var   Var;

    ///Lexer for tokenizing tags
    struct MtgLexer : lexer
    {
        int IDENT,NUM,DQSTRING,STEXT;

        virtual void on_error_prefix( bool rules, charstr& dst )
        {
            if(!rules) {
                uint c;
                uint l = current_line(0, &c);
                dst << infile << char(':') << l << char(':') << c << ": ";
            }
        }

        void set_current_file( const token& file ) {
            infile = file;
        }


        MtgLexer()
        {
            def_group( "ignore", " \t\n\r" );
            IDENT   = def_group( "identifier", ".a..zA..Z_", ".@a..zA..Z_0..9" );
            NUM     = def_group( "number", "0..9" );
            def_group_single( "separator", "?!=()[]{}/$-" );

            int ie = def_escape( "escape", '\\', 0 );
            def_escape_pair( ie, "\\", "\\" );
            def_escape_pair( ie, "n", "\n" );
            def_escape_pair( ie, "t", "    " );
            def_escape_pair( ie, "\n", 0 );

            DQSTRING = def_string( "dqstring", "\"", "\"", "escape" );

            //static text between tags
            STEXT = def_string( "stext", "$", "$", "" );
            def_string( "stext", "$", "", "" );
            enable( STEXT, false );
        }

        charstr& set_err() {
            return (prepare_exception() << "syntax error: ");
        }

    private:
        charstr infile;
    };

    typedef lexer::lextoken         lextoken;


    ///Variable from cache
    struct Varx
    {
        const Var* var;             ///< associated variable
        Varx* varparent;
        const uchar* data;          ///< cache position
        int index;                  ///< current element index for arrays


        Varx() { var=0; varparent=0; data=0; }
        Varx( const Var* v, const uchar* d ) : var(v), varparent(0), data(d) {}


        ///Find member variable and its position in the cache
        bool find_child( const token& name, Varx& ch ) const
        {
            int i = var->desc->find_child_pos(name);
            if(i<0)  return false;

            ch.var = &var->desc->children[i];
            ch.data = data + i*sizeof(uints);
            ch.data += *(int*)ch.data;
            ch.varparent = (Varx*)this;

            return true;
        }

        ///Find a descendant variable and its position in the cache
        //@param last_is_attrib if set, do not treat the last token as child name and return it here, but only after at least one child has been read
        bool find_descendant( token name, Varx& ch, bool last_is_attrib, token* last = 0 ) const
        {
            const Varx* v = this;
            token part;

            if( name.is_empty() )  return false;

            if(last)
                last->set_empty();

            //leading dots address ancestors
            do {
                part = name.cut_left('.');
                if( !part.is_empty() )  break;

                if( !v->varparent )  return false;
                v = v->varparent;
            }
            while( !name.is_empty() );

            ch = *v;
            if( part.is_empty() )
                return true;

            //find descendant
            int nch = 0;
            do {
                if( last_is_attrib  &&  nch>0  &&  name.len() == 0 ) {
                    if(last)  *last = part;
                    return true;
                }

                if( part.first_char() == '@' ) {
                    if(last)  *last = part;
                    return true;
                }

                if( !ch.find_child(part, ch) ) {
                    if(last)   *last = name.cut_right_back('.');
                    return false;
                }

                part = name.cut_left('.');
                ++nch;
            }
            while( !part.is_empty() );

            return true;
        }

        uint array_size() const
        {
            DASSERT( var->is_array() );
            return *(uint*)data;
        }

        token write_buf( metagen& mg ) const;

        void write_var( metagen& mg ) const {
            mg.bin->xwrite_token_raw( write_buf(mg) );
        }

        bool is_nonzero() const
        {
            if( var->is_array() )  return array_size()>0;
            if( var->is_compound() )  return true;
            return var->desc->btype.value_int(data) != 0;
        }

        bool is_array() const      { return var->is_array(); }
    };

    ///Array element variable from cache
    struct VarxElement : Varx
    {
        uint size;                      ///< element byte size


        ///First array element, return count
        uint first( Varx& ary )
        {
            DASSERT( ary.var->is_array() );
            var = ary.var->element();
            data = ary.data + sizeof(uint);
            varparent = &ary;

            prepare();
            return *(uint*)ary.data;
        }

        VarxElement& next()
        {
            data += size - sizeof(uints);
            prepare();
            return *this;
        }

    private:
        void prepare()
        {
            if( var->is_primitive() && !var->is_array_element() )
                size = var->get_size();
            else
                size = *(int*)data;
            
            data += sizeof(uints);
        }
    };

    struct Attribute
    {
        struct Value {
            token value;                ///< value pointer
            charstr valuebuf;           ///< buffer for value if needed

            void swap( Value& other )
            {
                valuebuf.swap( other.valuebuf );
                std::swap( value, other.value );
            }

            Value() { value.set_empty(); }
        };

        token name;
        enum { UNKNOWN, DEFAULT, INLINE, OPEN, COND_POS, COND_NEG } cond;
        Value value;
        int depth;


        bool is_condition() const       { return cond >= COND_POS; }
        bool is_open() const            { return cond == OPEN; }

        bool parse( MtgLexer& lex )
        {
            //attribute can be
            // [!]<name>(.<name>)* [?!] [= "value"]
            const lextoken& tok = lex.last();

            bool condneg = false;
            bool special = false;
            if( tok == '!' ) {
                condneg = true;
                lex.next();
            }

            if( tok.id != lex.IDENT )  return false;
            name = tok.tok;

            depth = 0;
            const char* pc = name.ptr();
            const char* pce = name.ptre();

            while( *pc == '.' )  ++pc;  //skip leading .. (not counted to depth)

            for( ; pc<pce; ++pc )
                if( *pc == '.' )  ++depth;

            cond = UNKNOWN;
            lex.next();
            if( tok.tok == '?' )        cond = condneg ? COND_NEG : COND_POS;
            else if( tok.tok == '!' )   cond = COND_NEG;
            else lex.push_back();

            lex.next();
            if( tok == '=' )
            {
                lex.next();

                if( tok.id != lex.DQSTRING ) {
                    lex.set_err() << "expected attribute value string";
                    throw lex.exception();
                }

                value.value = const_cast<lextoken&>(tok).swap_to_token_or_string( value.valuebuf );
                if(!cond)
                    cond = INLINE;

                lex.next();
            }

            if( cond && depth==0 )
                depth = 1;          //force find_child
            else if(!cond)
                cond = OPEN;

            if( name == "default" )
            {
                if( cond == INLINE )  cond = DEFAULT;
                else if( cond != OPEN ) {
                    lex.set_err() << "'default' attribute can only be open or inline";
                    throw lex.exception();
                }
            }

            return true;
        }

        bool eval( metagen& mg, const Varx& var ) const
        {
            //if the depth is set, the attribute reffers to a descendant
            token n = name;
            Varx v;
            bool defined = true;

            if( depth > 0 )
                defined = var.find_descendant(name, v, true, &n);
            else
                v = var;

            bool inv;
            if( cond == COND_POS )      inv = false;
            else if( cond == COND_NEG ) inv = true;
            else return true;

            bool val;

            if( !value.value.is_empty() )
                val = defined  &&  value.value == v.write_buf(mg);
            else if( n == "defined" )
                val = defined;
            else if( n.is_empty()  ||  n == "nonzero"  ||  n == "true" )
                val = defined && v.is_nonzero();
            else if( n == "empty"  ||  n == "false" )
                val = !(defined && v.is_nonzero());
            else
                val = false;

            return val ^ inv;
        }

        void swap( Attribute& attr )
        {
            std::swap( name, attr.name );
            std::swap( cond, attr.cond );
            value.swap( attr.value );
            std::swap( depth, attr.depth );
        }
    };

    ///
    struct ParsedTag
    {
        token varname;
        dynarray<Attribute> attr;
        uint16 flags;
        char brace;                     ///< brace type (character)
        int8 depth;                     ///< tag depth from current level (number of dots before name)

        enum { fTRAILING = 1, rEAT_LEFT = 8, rEAT_RIGHT = 12, xEAT = 0xf };

        ParsedTag() : flags(0) {}

        uint eat_left() const {
            return (flags >> rEAT_LEFT) & xEAT;
        }

        uint eat_right() const {
            return (flags >> rEAT_RIGHT) & xEAT;
        }

        bool same_group( const ParsedTag& p ) const
        {
            if( p.brace == '(' ) {
                return brace == p.brace
                    && (((flags&fTRAILING) && varname == "if") || varname == "elif");
            }

            return brace == p.brace
                && varname == p.varname;
        }

        void set_empty()    { varname.set_empty();  flags=0;  brace=0;  depth=0; }

        bool parse( MtgLexer& lex )
        {
            //tag content
            // <name> (attribute)*
            if( 0 == lex.next(0) )
                return false;

            const lextoken& tok = lex.last();

            flags = 0;
            while( tok.tok == '-' ) {
                flags += 1 << rEAT_LEFT;
                lex.next(0);
            }

            if( tok.tok != '{'  &&  tok.tok != '['  &&  tok.tok != '(' ) {
                brace = 0;
                lex.push_back();
            }
            else
                brace = tok.tok[0];

            lex.next();

            if( tok.tok == '/' ) {
                flags |= fTRAILING;
                lex.next();
            }

            if( tok.id != lex.IDENT ) {
                lex.set_err() << "Expected identifier";
                throw lex.exception();
            }
            varname = tok.tok;

            depth = 0;
            const char* p = tok.tok.ptr();
            const char* pe = tok.tok.ptre();
            for( ; p<pe; ++p )
                if( *p == '.' )  ++depth;

            lex.next();

            int def = -1;
            bool lastopen = false;

            attr.reset();

            Attribute at;
            while( at.parse(lex) )
            {
                if(lastopen) {
                    lex.set_err() << "An open attribute followed by another";
                    throw lex.exception();
                }

                lastopen = at.is_open();

                //place default attribute on the beginning
                if( at.cond == Attribute::DEFAULT )
                    attr.ins(0)->swap(at);
                else
                    attr.add()->swap(at);
            }

            if(brace)
            {
                if( brace == '('  &&  tok.tok != ')' ) {
                    lex.set_err() << "Expecting )";
                    throw lex.exception();
                }
                if( brace == '{'  &&  tok.tok != '}' ) {
                    lex.set_err() << "Expecting }";
                    throw lex.exception();
                }
                if( brace == '['  &&  tok.tok != ']' ) {
                    lex.set_err() << "Expecting ]";
                    throw lex.exception();
                }

                lex.next(0);
            }

            while( tok.tok == '-' ) {
                flags += 1 << rEAT_RIGHT;
                lex.next(0);
            }

            if( tok.tok != '$' ) {
                lex.set_err() << "Expecting end of tag $";
                throw lex.exception();
            }

            return true;
        }
    };

    ///Basic tag definition
    struct Tag
    {
        token varname;              ///< variable name
        int depth;                  ///< variable depth from parent
        token stext;                ///< static text after the tag


        virtual ~Tag() {}

        bool find_var( const Varx& par, Varx& var, token& attrib ) const
        {
            return depth<1
                ? par.find_child( varname, var )
                : par.find_descendant( varname, var, false, &attrib );
        }

        void process( metagen& mg, const Varx& var ) const
        {
            if( !varname.is_empty() )
                process_content(mg, var);

            if( !stext.is_empty() )
                mg.bin->xwrite_raw( stext.ptr(), stext.len() );
        }

        bool parse( MtgLexer& lex, ParsedTag& hdr )
        {
            varname = hdr.varname;
            depth = hdr.depth;

            parse_content(lex, hdr);

            try { lex.next_as_string(lex.STEXT); }
            catch( lexer::lexception& ) {
                return false;
            }

            stext = lex.last().tok;

            uint nr = hdr.eat_right();
            while(nr--) {
                stext.skip_ingroup(" \t");

                uint len = stext.len();
                stext.skip_newline();
                if( len == stext.len() )
                    break;
            }

            return true;
        }

        ///Write inline default attribute if there's one
        static void write_default( metagen& mg, const dynarray<Attribute>& attr )
        {
            if( attr.size()>0  &&  attr[0].cond == Attribute::DEFAULT )
                mg.bin->xwrite_raw( attr[0].value.value.ptr(), attr[0].value.value.len() );
        }

        virtual void process_content( metagen& mg, const Varx& var ) const = 0;
        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr ) = 0;
    };

    ///Empty tag for tagless leading static text 
    struct TagEmpty : Tag
    {
        virtual void process_content( metagen& mg, const Varx& var ) const  {}
        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr ) {}

        bool parse( MtgLexer& lex, uint skip_newline )
        {
            varname.set_empty();
            depth = 0;

            try { lex.next_as_string( lex.STEXT ); }
            catch( lexer::lexception& ) {
                return false;
            }

            stext = lex.last().tok;

            while(skip_newline--) {
                stext.skip_ingroup(" \t");

                uints len = stext.len();
                stext.skip_newline();
                if( len == stext.len() )
                    break;
            }

            return true;
        }
    };

    ///Simple substitution tag
    struct TagSimple : Tag
    {
        dynarray<Attribute> attr;       ///< conditions and attributes

        ///Process the variable, default code does simple substitution
        virtual void process_content( metagen& mg, const Varx& var ) const
        {
            token attrib;
            Varx v;

            if( find_var(var,v,attrib) ) {
                if(!attrib.is_empty())
                    write_special_value(mg, attrib, v);
                else
                    v.write_var(mg);
            }
            else
                write_default( mg, attr );
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            attr.swap( hdr.attr );

            if( attr.size()>0 && attr.last()->is_open() ) {
                lex.set_err() << "Simple tags cannot contain open attribute";
                throw lex.exception();
            }
        }

        bool write_special_value( metagen& mg, const token& attrib, Varx& v ) const
        {
            if(attrib == "@index") {
                DASSERT( v.is_array() );
                if(v.is_array())
                    mg.write_as_string(v.index);
            }
            else
                return false;
            return true;
        }
    };

    ///Linear sequence of chunks to process
    struct TagRange
    {
        typedef local<Tag>              LTag;
        dynarray<LTag> sequence;        ///< either a tag sequence to apply
        Attribute::Value value;         ///< or an attribute string


        void set_attribute( Attribute& at )
        {
            value.swap( at.value );
        }

        bool is_set() const
        {
            return !value.value.is_empty()  ||  sequence.size()>0;
        }

        void process( metagen& mg, const Varx& var ) const
        {
            if( !value.value.is_empty() )
                mg.bin->xwrite_raw( value.value.ptr(), value.value.len() );

            const LTag* ch = sequence.ptr();
            for( uints n=sequence.size(); n>0; --n,++ch )
                (*ch)->process( mg, var );
        }

        bool parse( MtgLexer& lex, ParsedTag& tout, const ParsedTag& par )
        {
            TagEmpty* etag = new TagEmpty;
            *sequence.add() = etag;

            bool succ = etag->parse( lex, tout.eat_right() );
            if(!succ) return succ;

            do {
                if(!tout.parse(lex))
                    break;

                uint nl = tout.eat_left();
                while(nl--) {
                    token& stext = (*sequence.last())->stext;
                    stext.truncate( stext.count_ingroup_back(" \t") );

                    uints len = stext.len();
                    stext.trim_newline();
                    if( len == stext.len() )
                        break;
                }

                if( tout.flags & ParsedTag::fTRAILING ) {
                    if( !tout.same_group(par) ) {
                        lex.set_err() << "Mismatched closing tag";
                        throw lex.exception();
                    }
                    return succ;
                }
                if( tout.same_group(par) )
                    return succ;

                //we have a new tag here
                Tag* ptag;
                if( tout.brace == '(' )
                    ptag = new TagCondition;
                else if( tout.brace == '{' )
                    ptag = new TagStruct;
                else if( tout.brace == '[' )
                    ptag = new TagArray;
                else
                    ptag = new TagSimple;

                *sequence.add() = ptag;
                succ = ptag->parse(lex, tout);
            }
            while(succ);

            if( !par.varname.is_empty() ) {
                lex.set_err() << "End of file before the closing tag";
                throw lex.exception();
            }
            return succ;
        }

        void swap( TagRange& other )        { sequence.swap(other.sequence); }
    };

    ///Conditional tag
    struct TagCondition : Tag
    {
        ///Conditional clause
        struct Clause {
            dynarray<Attribute> attr;
            TagRange rng;

            bool eval( metagen& mg, const Varx& var ) const
            {
                const Attribute* p = attr.ptr();
                const Attribute* pe = attr.ptre();

                for( ; p<pe; ++p )
                    if( !p->eval(mg, var) )  return false;

                rng.process( mg, var );
                return true;
            }
        };

        dynarray<Clause> clause;        ///< conditional sections


        virtual void process_content( metagen& mg, const Varx& var ) const
        {
            const Clause* p = clause.ptr();
            const Clause* pe = clause.ptre();

            for( ; p<pe; ++p )
                if( p->eval(mg,var) )  return;
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            if( hdr.varname != "if" ) {
                lex.set_err() << "Unknown code block: " << hdr.varname;
                throw lex.exception();
            }

            ParsedTag tmp;
            tmp.attr.swap( hdr.attr );
            tmp.flags = hdr.flags;

            do {
                Clause* c = clause.add();
                c->attr.swap( tmp.attr );

                c->rng.parse( lex, tmp, hdr );

                if( tmp.flags & ParsedTag::fTRAILING )
                    break;
            }
            while(1);
        }
    };

    ///Tag operating with array-type variables
    struct TagArray : Tag
    {
        TagRange atr_first, atr_rest;
        TagRange atr_body;
        TagRange atr_after;

        dynarray<Attribute> cond;

        bool eval_cond( metagen& mg, const Varx& var ) const
        {
            const Attribute* p = cond.ptr();
            const Attribute* pe = cond.ptre();
            for( ; p<pe; ++p )
                if( !p->eval(mg, var) )  return false;
            return true;
        }

        void process_content( metagen& mg, const Varx& var ) const
        {
            token attrib;
            Varx v;
            if( find_var(var,v,attrib) )
            {
                if( !v.var->is_array() )
                    return;

                VarxElement ve;
                uint n = ve.first(v);
                if(!n)
                    return;

                bool evalcond = cond.size()>0;

                int i=0;
                for( ; n>0; --n,ve.next() )
                {
                    v.index = i;

                    if( evalcond && !eval_cond(mg, ve) )
                        continue;

                    if(i==0)  atr_first.process( mg, ve );
                    else      atr_rest.process( mg, ve );

                    atr_body.process( mg, ve );
                    ++i;
                }

                //the 'after' statement is evaluated only if there were some array items evaluated too
                if(i>0)
                    atr_after.process( mg, v );
            }
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            ParsedTag tmp;
            tmp.attr.swap( hdr.attr );
            tmp.flags = hdr.flags;

            do {
                TagRange& rng = bind_attributes( lex, tmp.attr );

                rng.parse( lex, tmp, hdr );
                if( tmp.flags & ParsedTag::fTRAILING ) {
                    hdr.flags &= ~(ParsedTag::fTRAILING | (ParsedTag::xEAT<<ParsedTag::rEAT_RIGHT));
                    hdr.flags = tmp.flags & (ParsedTag::xEAT<<ParsedTag::rEAT_RIGHT);
                    break;
                }
            }
            while(1);
        }

    private:
        TagRange* section( const token& name )
        {
            if( name == "first" )  return &atr_first;
            if( name == "rest" )   return &atr_rest;
            if( name == "body" )   return &atr_body;
            if( name == "after" )  return &atr_after;
            return 0;
        }

        TagRange& bind_attributes( MtgLexer& lex, dynarray<Attribute>& attr )
        {
            TagRange* sec = 0;

            Attribute* p = attr.ptr();
            Attribute* pe = attr.ptre();
            for( ; p<pe; ++p )
            {
                if( p->is_condition() ) {
                    cond.add()->swap(*p);
                    continue;
                }

                TagRange* tr = section( p->name );

                if(!tr) {
                    lex.set_err() << "Unknown attribute of an array tag: " << p->name;
                    throw lex.exception();
                }

                if( tr->is_set() ) {
                    lex.set_err() << "Section already assigned";
                    throw lex.exception();
                }

                if( p->cond == Attribute::INLINE )
                    tr->set_attribute(*p);
                else if( p->cond == Attribute::OPEN )
                    sec = tr;
                else {
                    lex.set_err() << "Array attribute can be only inline or open";
                    throw lex.exception();
                }
            }

            if(!sec) {
                if( atr_body.is_set() ) {
                    lex.set_err() << "Section already assigned";
                    throw lex.exception();
                }
                sec = &atr_body;
            }

            return *sec;
        }
    };

    ///Structure tag changes the current variable scope to some descendant
    struct TagStruct : Tag
    {
        TagRange seq;

        void process_content( metagen& mg, const Varx& var ) const
        {
            token attrib;
            Varx v;
            bool valid = find_var(var,v,attrib);

            if(valid)
                seq.process( mg, v );
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            ParsedTag tmp;
            tmp.attr.swap( hdr.attr );
            tmp.flags = hdr.flags;

            do {
                if( tmp.attr.size() > 0 ) {
                    lex.set_err() << "Unknown attribute for structural tag";
                    throw lex.exception();
                }

                TagRange rng;
                rng.parse( lex, tmp, hdr );
                if( tmp.flags & ParsedTag::fTRAILING )
                    break;
            }
            while(1);
        }
    };

public:
    metagen()
    {
        bin = 0;
    }

    bool parse( binstream& bin )
    {
        patbuf.transfer_from(bin);
        const token& tok = patbuf;

        lex.bind(tok);

        ParsedTag tmp;
        try {
            ParsedTag empty;
            empty.set_empty();

            tags.parse( lex, tmp, empty );
            return true;
        }
        catch( const lexer::lexception& ) {
            return false;
        }
    }

    const charstr& err() const {
        return lex.err();
    }

    template<class T>
    void generate( const T& obj, binstream& bot )
    {
        tmpx.reset_all();
        fmtx.bind(tmpx);

        meta.bind_formatting_stream(fmtx);

        meta.stream_out(obj);
        meta.stream_flush();

        meta.cache_in<T>();
        meta.stream_acknowledge();

        const uchar* cachedata;
        const MetaDesc::Var& root = meta.get_root_var( cachedata );

        generate( bot, root, cachedata );
    }

    void generate( binstream& bin, const Var& var, const uchar* data )
    {
        this->bin = &bin;

        Varx v( &var, data );
        tags.process( *this, v );
    }

    ///Set prefix to be displayed when reporting errors
    void set_source_path( const token& path ) {
        lex.set_current_file(path);
    }

    ///Write a string value to output
    template<class T>
    void write_as_string( T val ) {
        buf = val;
        bin->xwrite_raw(buf.ptr(), buf.len());
        buf.reset();
    }


/*
    const char* error_text() const  { return err_lex; }

    charstr& error_location( charstr& buf, const token& file )
    {
        const lextoken& last = lex.last();
        token line;
        uint col;
        uint row = lex.current_line( &line, &col );

        buf << file << ":" << (row+1) << ": " << err_lex << "\n";

        if( !line.is_empty() ) {
            buf << line << "\n";
            buf.appendn( col, ' ' );
            buf << "^\n";
        }

        return buf;
    }
*/
private:
    charstr buf;                    ///< helper buffer
    binstream* bin;                 ///< output stream

    binstreambuf patbuf;            ///< buffered pattern file

    metastream meta;
    binstreambuf tmpx;
    fmtstreamcxx fmtx;

    //const char* err_lex;

    MtgLexer lex;                   ///< lexer used to parse the template file
    TagRange tags;                  ///< top level tag array
};

////////////////////////////////////////////////////////////////////////////////
inline token metagen::Varx::write_buf( metagen& mg ) const
{
    typedef bstype::kind    type;

    const uchar* p = data;
    type t = var->desc->btype;

    if( var->is_array() )
        return token( (const char*)p+sizeof(uint), *(uint*)p );

    charstr& buf = mg.buf;
    buf.reset();

    switch(t.type)
    {
        case type::T_INT:
            buf.append_num_int( 10, p, t.get_size() );
            break;

        case type::T_UINT:
            buf.append_num_uint( 10, p, t.get_size() );
            break;

        case type::T_FLOAT:
            switch( t.get_size() ) {
            case 4:
                buf += *(const float*)p;
                break;
            case 8:
                buf += *(const double*)p;
                break;
            }
        break;

        case type::T_CHAR: {
            char c = *(char*)p;
            if(c) buf.append(c);
        } break;

        case type::T_BOOL:
            if( *(bool*)p ) buf << "true";
            else            buf << "false";
        break;

        case type::T_TIME: {
            buf.append('"');
            buf.append_date_local( *(const timet*)p );
            buf.append('"');
        } break;

        case type::T_ERRCODE:
            {
                opcd e = (const opcd::errcode*)p;
                token t;
                t.set( e.error_code(), token::strnlen( e.error_code(), 5 ) );

                buf << "\"[" << t;
                if(!e)  buf << "]\"";
                else {
                    buf << "] " << e.error_desc();
                    const char* text = e.text();
                    if(text[0])
                        buf << ": " << e.text() << "\"";
                    else
                        buf << char('"');
                }
            }
        break;
    }

    return buf;
}


COID_NAMESPACE_END


#endif //__COID_COMM_METAGEN__HEADER_FILE__
