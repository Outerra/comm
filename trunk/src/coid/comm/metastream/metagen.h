
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


COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Class metagen - generating complex documents from objects using document template.
/**
    It serves as a formatting stream for the metastream class too, so it should
    inherit from binstream base class

    Parsed data layout:
    - all <len> marks are 4b containing actual length of consequent string
    - all strings are padded with space to nearest 4b boundary
    - varname's first character describes the variable type - assoc,array,simple

    primitive:          <len>varname<len>value
    primitive array:    <size><len>value0<len>value1...
    assoc:              <size>[]
**/
class metagen : public binstream
{


    ///Base class for managed objects
    struct Node
    {
    private:

    public:

        ///Name (key) of the object stored.
        /** One more character is appended to the name and used also to
            determine the node type (CODE_* enum values).
        **/
        charstr name;

        ///Value placeholder
        void* place;


        bool operator < (const token& k) const  { return name < k; }


        Node() {}
        ~Node() { destroy(); }

        ///Return value if the node contains a string value
        /// (an empty token otherwise)
        token value() const
        {
            char c = name.last_char();
            if( c == CODE_STRING )      return token(*(const charstr*)&place);
            else if( c == CODE_VALUE ) {
                const charstr& s = *(const charstr*)&place;
                return token( s.ptr(), s.len()-1 );
            }
            return token::empty();
        }

        ///Return true if the node has a simple string value
        bool is_string() const          { return name.last_char() == CODE_STRING; }

        ///Return true if this node is either an array or an associative
        /// container
        ///@note both NodeArray and NodeAssoc qualify as arrays, to determine
        /// whether node is array but not an associative array, additional
        /// !is_assoc() check must be used.
        bool is_array() const           { return name.last_char() <= CODE_ARRAY; }

        ///Return true if this node is an associative container
        bool is_assoc() const           { return name.last_char() == CODE_ASSOC; }


        ///Set up node to contain a named value
        void set( const token& key, const token& value )
        {
            name = key;
            name.append(CODE_STRING);

            charstr& v = reset_to_string();
            v = value;
        }

        ///Setup node to contain a simple unnamed value
        void set( const token& value )
        {
            name = value;
            name.append(CODE_VALUE);

            destroy();
        }

        ///Setup node to contain an array
        void set_array( const token& key )
        {
            name = key;
            name.append(CODE_ARRAY);

            reset_to_array();
        }

        ///Setup node to contain an associative array
        void set_assoc( const token& key )
        {
            name = key;
            name.append(CODE_ASSOC);

            reset_to_array();
        }

        ///Append simple value to an array
        void append( const token& value )
        {
            TANodes& a = array();

            a.add()->set(value);
        }

        ///Insert simple node to an associative array
        Node& insert( const token& key, const token& value )
        {
            TANodes& a = assoc();

            Node* n = a.add_sortT(key);
            n->set( key, value );
            
            return *n;
        }

        ///Insert array node to an associative array
        Node& insert_array( const token& key )
        {
            TANodes& a = assoc();

            Node* n = a.add_sortT(key);
            n->set_array(key);
            
            return *n;
        }

        ///Insert associative node to an associative array
        Node& insert_assoc( const token& key )
        {
            TANodes& a = assoc();

            Node* n = a.add_sortT(key);
            n->set_assoc(key);
            
            return *n;
        }


    private:
        typedef dynarray<Node>          TANodes;

        void destroy()
        {
            char c = name.last_char();
            if( c == 0 )
                return;     //unused
            if( c <= CODE_ARRAY )
                array().~TANodes();
            else if( c == CODE_STRING )
                string().~charstr();
        }

        TANodes& reset_to_array()
        {
            char c = name.last_char();
            if( c == CODE_STRING ) {
                string().~charstr();
                c = 0;
            }

            if(!c)
                new((void*)&place) TANodes;
            else
                array().reset();
    
            return array();
        }

        charstr& reset_to_string()
        {
            char c = name.last_char();
            if( c <= CODE_ARRAY ) {
                array().~TANodes();
                c = 0;
            }

            if(!c)
                new((void*)&place) charstr;
            else
                string().reset();

            return string();
        }

        ///Cast value to dynarray<Node> type
        ///@note only in the debug mode checks whether the node can be cast
        TANodes& array()                { DASSERT(is_array());  return *(TANodes*)&place; }
        const TANodes& array() const    { DASSERT(is_array());  return *(const TANodes*)&place; }

        ///Cast value to dynarray<Node> type
        ///@note only in the debug mode checks whether the node can be cast
        TANodes& assoc()                { DASSERT(is_assoc());  return *(TANodes*)&place; }
        const TANodes& assoc() const    { DASSERT(is_assoc());  return *(const TANodes*)&place; }

        ///Cast value to charstr type
        ///@note only in the debug mode checks whether the node can be cast
        charstr& string()               { DASSERT(is_string());  return *(charstr*)&place; }
        const charstr& string() const   { DASSERT(is_string());  return *(const charstr*)&place; }
    };



};

COID_NAMESPACE_END


#endif //__COID_COMM_METAGEN__HEADER_FILE__
