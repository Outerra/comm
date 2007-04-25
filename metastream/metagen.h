
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
#include "../binstream.h"
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
        int IDENT,NUM,PREFIX,DQSTRING,STEXT;

        struct Exception {
            const char* errtext;
            Exception(const char* t) : errtext(t) {}
        };

        MtgLexer()
        {
            def_group( "ignore", " \t\n\r" );
            IDENT   = def_group( "identifier", "a..zA..Z_", ".a..zA..Z_0..9" );
            NUM     = def_group( "number", "0..9" );
            def_group_single( "separator", "?!=()[]{}/$" );
            PREFIX  = def_group( "prefix", "+" );

            int ie = def_escape( "escape", '\\', 0 );
            def_escape_pair( ie, "\\", "\\" );
            def_escape_pair( ie, "n", "\n" );
            def_escape_pair( ie, "\n", 0 );

            DQSTRING = def_string( "dqstring", "\"", "\"", "escape" );

            //static text between tags
            STEXT = def_string( "stext", "$", "$", "" );
            enable( STEXT, false );
        }

        void throw_error( const char* text )
        {
            throw Exception(text);
        }
    };

    typedef lexer::lextoken         lextoken;


    ///Variable from cache
    struct Varx
    {
        const Var* var;             ///< associated variable
        Varx* varparent;
        const uchar* data;          ///< cache position


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
        bool find_descendant( token name, Varx& ch ) const
        {
            ch = *this;

            for( token part; !name.is_empty(); )
            {
                part = name.cut_left('.',1);
                if( !ch.find_child( part, ch ) )
                    return false;
            }

            return true;
        }

        uint array_size() const
        {
            DASSERT( var->is_array() );
            return *(uint*)data;
        }

        void write_var( metagen& mg ) const;

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
        uint size;                  ///< element byte size


        ///First array element, return count
        uint first( Varx& ary )
        {
            DASSERT( ary.var->is_array() );
            var = ary.var->element();
            data = ary.data + sizeof(uint);
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
            charstr valuebuf;           ///< buffer for value if needed
            token value;                ///< value pointer

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


        bool is_condition() const       { return cond >= COND_POS; }
        bool is_open() const            { return cond == OPEN; }

        bool parse( MtgLexer& lex )
        {
            //attribute can be
            // <name> [?!] [= "value"]
            const lextoken& tok = lex.last();
            
            if( tok.id != lex.IDENT )  return false;
            name = tok.tok;


            cond = UNKNOWN;
            lex.next();
            if( tok.tok == '?' )        cond = COND_POS;
            else if( tok.tok == '!' )   cond = COND_NEG;
            else lex.push_back();

            lex.next();
            if( tok == '=' )
            {
                lex.next();
                if( tok.id != lex.DQSTRING )
                    lex.throw_error("expected attribute value string");

                const_cast<lextoken&>(tok).swap_to_token_or_string( value.value, value.valuebuf );
                if(!cond)
                    cond = INLINE;

                lex.next();
            }
            else if(!cond)
                cond = OPEN;

            if( name == "default" )
            {
                if( cond == INLINE )  cond = DEFAULT;
                else if( cond != OPEN )
                    lex.throw_error("'default' attribute can only be open or inline");
            }

            return true;
        }

        bool eval( const Varx& var, bool defined ) const
        {
            bool inv;
            if( cond == COND_POS )       inv = false;
            else if( cond == COND_NEG ) inv = true;
            else return true;

            bool val;

            if( name == "defined" )
                val = defined;
            else if( name == "nonzero" )
                val = defined && var.is_nonzero();
            else if( name == "always" )
                val = true;
            else
                val = false;

            return val ^ inv;
        }

        void swap( Attribute& attr )
        {
            std::swap( name, attr.name );
            std::swap( cond, attr.cond );
            value.swap( attr.value );
        }
    };

    ///
    struct ParsedTag
    {
        token varname;
        dynarray<Attribute> attr;
        char brace;
        char flags;
        int8 prefixlen;
        int8 depth;

        enum { fTRAILING = 1, fEAT_LEFT = 2, fEAT_RIGHT = 4, };


        bool same_group( const ParsedTag& p ) const
        {
            return brace == p.brace
                && prefixlen == p.prefixlen
                && varname == p.varname;
        }

        void set_empty()    { varname.set_empty(); }

        void parse( MtgLexer& lex )
        {
            //tag content
            // <name> (attribute)*
            lex.next(0);
            const lextoken& tok = lex.last();

            flags = 0;
            if( tok.tok == '-' ) {
                flags |= fEAT_LEFT;
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

            if( tok.id != lex.PREFIX )
                prefixlen = 0;
            else {
                prefixlen = (int8)tok.tok.len();
                lex.next();
            }

            if( tok.id != lex.IDENT )
                lex.throw_error("Expected identifier");
            varname = tok.tok;

            depth = 1;
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
                if( lastopen )
                    lex.throw_error("An open attribute followed by another");

                lastopen = at.is_open();

                //place default attribute on the beginning
                if( at.cond == Attribute::DEFAULT )
                    attr.ins(0)->swap(at);
                else
                    attr.add()->swap(at);
            }

            if(brace)
            {
                if( brace == '('  &&  tok.tok != ')' )
                    lex.throw_error("Expecting )");
                if( brace == '{'  &&  tok.tok != '}' )
                    lex.throw_error("Expecting }");
                if( brace == '['  &&  tok.tok != ']' )
                    lex.throw_error("Expecting ]");
                lex.next(0);
            }

            if( tok.tok == '-' ) {
                flags |= fEAT_RIGHT;
                lex.next(0);
            }

            if( tok.tok != '$' )
                lex.throw_error("Expecting end of tag $");
        }
    };

    ///Basic tag definition
    struct Tag
    {
        token varname;              ///< variable name
        int depth;                  ///< variable depth from parent
        token stext;                ///< static text after the tag


        virtual ~Tag() {}

        bool find_var( const Varx& par, Varx& var ) const
        {
            return depth>1
                ? par.find_child( varname, var )
                : par.find_descendant( varname, var );
        }

        void process( metagen& mg, const Varx& var ) const
        {
            if( !varname.is_empty() )
                process_content( mg, var );

            if( !stext.is_empty() )
                mg.bin->xwrite_raw( stext.ptr(), stext.len() );
        }

        bool parse( MtgLexer& lex, ParsedTag& hdr )
        {
            varname = hdr.varname;
            depth = hdr.depth;

            parse_content( lex, hdr );

            bool succ = lex.next_string_or_block( lex.STEXT );
            stext = lex.last().tok;

            if( hdr.flags & ParsedTag::fEAT_RIGHT )
                stext.skip_newline();

            return succ;
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

        bool parse( MtgLexer& lex, bool skip_newline )
        {
            varname.set_empty();
            depth = 0;

            bool succ = lex.next_string_or_block( lex.STEXT );
            stext = lex.last().tok;

            if(skip_newline)
                stext.skip_newline();

            return succ;
        }
    };

    ///Simple substitution tag
    struct TagSimple : Tag
    {
        dynarray<Attribute> attr;       ///< conditions and attributes

        ///Process the variable, default code does simple substitution
        virtual void process_content( metagen& mg, const Varx& var ) const
        {
            Varx v;
            if( find_var(var,v) )
                v.write_var(mg);
            else
                write_default( mg, attr );
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            attr.swap( hdr.attr );

            if( attr.size()>0 && attr.last()->is_open() )
                lex.throw_error("Simple tags cannot contain open attribute");
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

            bool succ = etag->parse( lex, (tout.flags & ParsedTag::fEAT_RIGHT)!=0 );
            if(!succ) return succ;

            do {
                tout.parse(lex);

                if( (tout.flags & ParsedTag::fEAT_LEFT) )
                    (*sequence.last())->stext.trim_newline();

                if( tout.flags & ParsedTag::fTRAILING ) {
                    if( !tout.same_group(par) )
                        lex.throw_error("Mismatched closing tag");
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
                succ = ptag->parse( lex, tout );
            }
            while(succ);

            if( !par.varname.is_empty() )
                lex.throw_error("End of file before the closing tag");
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

            bool eval( metagen& mg, const Varx& par, const Varx& var, bool defined ) const
            {
                const Attribute* p = attr.ptr();
                const Attribute* pe = attr.ptre();

                if( p == pe  &&  !defined )
                    return defined;

                for( ; p<pe; ++p )
                    if( !p->eval(var,defined) )  return false;

                rng.process( mg, par );
                return true;
            }
        };

        dynarray<Clause> clause;        ///< conditional sections


        virtual void process_content( metagen& mg, const Varx& var ) const
        {
            Varx v;
            bool found = find_var(var,v);

            const Clause* p = clause.ptr();
            const Clause* pe = clause.ptre();

            for( ; p<pe; ++p )
                if( p->eval(mg,var,v,found) )  return;
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
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

        void process_content( metagen& mg, const Varx& var ) const
        {
            Varx v;
            if( find_var(var,v) )
            {
                if( !v.var->is_array() )
                    return;

                VarxElement ve;
                uint n = ve.first(v);
                if(!n)
                    return;

                atr_first.process( mg, ve );
                atr_body.process( mg, ve );

                for( ; n>1; --n ) {
                    atr_rest.process( mg, ve.next() );
                    atr_body.process( mg, ve );
                }

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
                if( tmp.flags & ParsedTag::fTRAILING )
                    break;
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
                TagRange* tr = section( p->name );

                if(!tr)
                    lex.throw_error("Unknown attribute of an array tag");

                if( tr->is_set() )
                    lex.throw_error("Section already assigned");

                if( p->cond == Attribute::INLINE )
                    tr->set_attribute(*p);
                else if( p->cond == Attribute::OPEN )
                    sec = tr;
                else
                    lex.throw_error("Array attribute can be only inline or open");
            }

            if(!sec) {
                if( atr_body.is_set() )
                    lex.throw_error("Section already assigned");
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
            Varx v;
            bool valid = find_var(var,v);

            if(valid)
                seq.process( mg, v );
        }

        virtual void parse_content( MtgLexer& lex, ParsedTag& hdr )
        {
            ParsedTag tmp;
            tmp.attr.swap( hdr.attr );
            tmp.flags = hdr.flags;

            do {
                if( tmp.attr.size() > 0 )
                    lex.throw_error("Unknown attribute for structural tag");

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
        patbuf.read_from(bin);
        const token& tok = patbuf;

        lex.bind(tok);
        err_lex = 0;

        ParsedTag tmp;
        try {
            ParsedTag empty;
            empty.set_empty();

            tags.parse( lex, tmp, empty );
            return true;
        }
        catch(MtgLexer::Exception e) {
            err_lex = e.errtext;
            return false;
        }
    }

    template<class T>
    void generate( const T& obj, binstream& bot )
    {
        tmpx.reset_all();
        fmtx.bind(tmpx);

        meta.bind_formatting_stream(fmtx);

        meta.stream_out(obj);
        meta.stream_flush();

        meta.stream_in_cache<T>();
        meta.stream_acknowledge();

        generate( bot, meta.get_root_var(), meta.get_cache() );
    }

    void generate( binstream& bin, const Var& var, const uchar* data )
    {
        this->bin = &bin;

        Varx v( &var, data );
        tags.process( *this, v );
    }

    const char* error_text() const  { return err_lex; }

private:
    charstr buf;                    ///< helper buffer
    binstream* bin;                 ///< output stream

    binstreambuf patbuf;            ///< buffered pattern file

    metastream meta;
    binstreambuf tmpx;
    fmtstreamcxx fmtx;

    const char* err_lex;

    MtgLexer lex;                   ///< lexer used to parse the template file
    TagRange tags;                  ///< top level tag array
};

////////////////////////////////////////////////////////////////////////////////
void metagen::Varx::write_var( metagen& mg ) const
{
    typedef bstype::type    type;

    const uchar* p = data;
    type t = var->desc->btype;

    if( var->is_array() )
    {
        mg.bin->xwrite_raw( (const char*)p+sizeof(uint), *(uint*)p );
        return;
    }

    charstr& buf = mg.buf;
    buf.reset();

    switch(t._type)
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
            case 16:
                buf += *(const long double*)p;
                break;
            }
        break;

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

    if( !buf.is_empty() )
        mg.bin->xwrite_raw( buf.ptr(), buf.len() );
}


COID_NAMESPACE_END


#endif //__COID_COMM_METAGEN__HEADER_FILE__
