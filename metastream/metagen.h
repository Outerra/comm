
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

#include "metavar.h"


COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Class metagen - generating complex documents from objects using document template.
/**
    It serves as a formatting stream for the metastream class too, so it should
    inherit from binstream base class

    
**/
class metagen : public binstream
{
    struct Tag;
    typedef local<Tag>      Ltag;
    typedef MetaDesc::Var   Var;

    charstr buf;
    binstream* bin;


    ///Variable from cache
    struct Varx
    {
        Var* var;
        const uchar* data;

        Varx() { var=0; data=0; }
        Varx( Var* v, const uchar* d ) : var(v), data(d) {}

        ///Find member variable and its position in the cache
        bool find_child( const token& name, Varx& ch ) const
        {
            int i = var->desc->find_child_pos(name);
            if(i<0)  return false;

            ch.var = &var->desc->children[i];
            ch.data = data + *(const int*)data;

            return true;
        }

        ///Find descendant variable and its position in the cache
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

        ///First array element, return size
        uint first( Varx& ch )
        {
            DASSERT( var->is_array() );
            ch.var = var->element();
            ch.data = data + sizeof(uint);
            return *(uint*)data;
        }

        void next( Varx& ch )
        {
            ch.data += ch.element_size();
        }

        ///Return size of the variable in cache
        int element_size() const
        {
            if( var->is_primitive() )
                return var->is_array()
                    ? sizeof(uint) + *(uint*)data * var->get_size()
                    : var->get_size();
            else
                return *(int*)data;
        }

        void write_var( metagen& mg ) const;
    };

    ///Basic tag definition
    struct Tag
    {
        token varname;              ///< variable name
        token defval;               ///< default value if the variable doesn't exist

        virtual ~Tag() {}
        virtual void process( metagen& mg, const Varx& var ) const
        {
            Varx v;
            if( var.find_child( varname, v ) )
                v.write_var(mg);
            else if( !defval.is_empty() )
                mg.bin->write_token( defval );
        }
    };

    ///
    struct Chunk
    {
        token stext;                ///< static text before a tag
        Ltag ptag;                  ///< subsequent Tag object

        void process( metagen& mg, const Varx& var ) const
        {
            if( !stext.is_empty() )
                mg.bin->write_token(stext);

            ptag->process( mg, var );
        }
    };

    ///Linear sequence of chunks to process
    struct TagRange
    {
        dynarray<Chunk> sequence;

        void process( metagen& mg, const Varx& var ) const
        {
            const Chunk* ch = sequence.ptr();
            for( uints n=sequence.size(); n>0; --n,++ch )
                ch->process( mg, var );
        }
    };

    ///Tag operating with array-type variables
    struct TagArray : Tag
    {
        TagRange atr_before, atr_after;
        TagRange atr_first, atr_rest;

        void process( metagen& mg, const Varx& var ) const
        {
            Varx v;
            if( var.find_child( varname, v ) )
            {
                if( !v.var->is_array() )
                    return;

                uint n = v.array_size();
                if(!n)
                    return;

                atr_first.process( mg, v );

                for( uint i=0; i<n; ++i )
                {
                    
                }
            }
        }
    };

    ///Structure tag changes the current variable to some descendant
    struct TagStruct : Tag
    {
        int depth;              ///< nesting depth (1 immediate member)
        TagRange seq;

        void process( metagen& mg, const Varx& var ) const
        {
            Varx v;
            bool valid = depth>1
                ? var.find_child( varname, v )
                : var.find_descendant( varname, v );

            if(valid)
                seq.process( mg, v );
        }
    };

};

////
void metagen::Varx::write_var( metagen& mg ) const
{
    typedef bstype::type    type;

    const uchar* p = data;
    type t = var->desc->btype;

    if( var->is_array() )
    {
        token str( (const char*)p+sizeof(uint), *(uint*)p );

        mg.bin->write_token(str);
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
        mg.bin->write_token(buf);
}


COID_NAMESPACE_END


#endif //__COID_COMM_METAGEN__HEADER_FILE__
