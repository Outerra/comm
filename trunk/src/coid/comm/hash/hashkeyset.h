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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
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

#ifndef __COID_COMM_HASHKEYSET__HEADER_FILE__
#define __COID_COMM_HASHKEYSET__HEADER_FILE__


#include "../namespace.h"
#include "hashtable.h"
#include <functional>

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
//@{ EXTRACTKEY templates

///Extracts key object from the value by casting operator
template <class VAL, class KEY>
struct _Select_Copy
{
    typedef KEY retType;

    retType operator()(const VAL& t) const   { return (KEY)t; }
};

///Extracts key object from the value pointer by dereferencing and casting
template <class VAL, class KEY>
struct _Select_CopyPtr
{
    typedef KEY retType;

    retType operator()(const VAL* t) const   { return (KEY)*t; }
};

///Extracts key reference from the value by casting operator
template <class VAL, class KEY>
struct _Select_GetRef
{
    typedef const KEY& retType;

    retType operator()(const VAL& t) const   { return (const KEY&)t; }
};

///Extracts key reference from the value pointer by dereferencing and casting
template <class VAL, class KEY>
struct _Select_GetRefPtr
{
    typedef const KEY& retType;

    retType operator()(const VAL* t) const   { return (const KEY&)*t; }
};

//@} EXTRACTKEY templates

////////////////////////////////////////////////////////////////////////////////
///
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC=hash<KEY>, class EQFUNC=equal_to<KEY,typename HASHFUNC::type_key>, class ALLOC=comm_allocator<VAL> >
class hash_keyset : public hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>
{
    typedef hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>  _HT;

public:

    typedef typename _HT::KEY                       key_type;
    typedef VAL                                     value_type;
    typedef EXTRACTKEY                              extractor;
    typedef HASHFUNC                                hasher;
    typedef EQFUNC                                  key_equal;

    typedef size_t                                  size_type;
    typedef ptrdiff_t                               difference_type;
    typedef value_type*                             pointer;
    typedef const value_type*                       const_pointer;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;

    typedef typename _HT::iterator                  iterator;
    typedef typename _HT::const_iterator            const_iterator;

    std::pair<iterator, bool> insert( const value_type& val )
    {   return insert_unique(val);    }

    void insert( const value_type* f, const value_type* l )
    {   insert_unique( f, l );   }

    void insert( const_iterator f, const_iterator l ) 
    {   insert_unique( f, l );   }

    ///Insert value if it's got an unique key
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return NULL if the value could not be inserted, or a constant pointer to the value
    const VAL* insert_value( const value_type& val )
    {
        typename _HT::Node** v = _insert_unique<false>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert value if it's got an unique key, swapping the provided value
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return NULL if the value could not be inserted, or a constant pointer to the value
    const VAL* swap_insert_value( value_type& val )
    {
        typename _HT::Node** v = _insert_unique<true>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert new value or override the existing one under the same key.
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return constant pointer to the value
    const VAL* insert_or_replace_value( const value_type& val )
    {
        typename _HT::Node** v = _insert_unique__replace<false>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Insert new value or override the existing one under the same key, swapping the content.
    ///@note the key needed for the insertion is extracted from the value using the extractor object provided in the constructor
    ///@return constant pointer to the value
    const VAL* swap_insert_or_replace_value( value_type& val )
    {
        typename _HT::Node** v = _insert_unique__replace<true>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Create an empty entry for value object that will be initialized by the caller afterwards
    ///@note the value object should be initialized so that it would return the same key as the one passed in here
    ///@param key the key under which the value object should be created
    VAL* insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _insert_unique_slot(key);
        return v  ?  &(*v)->_val  :  0;
    }

    ///Find or create an empty entry for value object that will be initialized by the caller afterwards
    ///@note the value object should be initialized so that it would return the same key as the one passed in here
    ///@param key the key under which the value object should be created
    VAL* find_or_insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _find_or_insert_slot(key);
        return &(*v)->_val;
    }


    ///Find value object corresponding to given key
    const VAL* find_value( const key_type& k ) const
    {
        const typename _HT::Node* v = find_node(k);
        return v ? &v->_val : 0;
    }

    ///Find value object corresponding to given key
    const VAL* find_value( uint hash, const key_type& k ) const
    {
        const typename _HT::Node* v = find_node(hash,k);
        return v ? &v->_val : 0;
    }


    hash_keyset()
        : _HT( 128, hasher(), key_equal(), extractor() ) {}

    explicit hash_keyset( const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex ) {}
    hash_keyset( const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex ) {}
    hash_keyset( const extractor& ex, const hasher& hf, const key_equal& eql, size_type n=128 )
        : _HT( n, hf, eql, ex ) {}



    hash_keyset( const value_type* f, const value_type* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }


    hash_keyset( const_iterator* f, const_iterator* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_keyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }

    static metastream& stream_meta( metastream& m )
    {
        MTEMPL_OPEN(m);
        MT(m, KEY);
        MT(m, VAL);
        MTEMPL_CLOSE(m);

        MSTRUCT_OPEN(m, "hash_keyset");
        MMAT(m, "elements", value_type );
        MSTRUCT_CLOSE(m);
    }
};


////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC=hash<KEY>, class EQFUNC=equal_to<KEY,typename HASHFUNC::type_key>, class ALLOC=comm_allocator<VAL> >
class hash_multikeyset : public hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>
{
    typedef hashtable<VAL,HASHFUNC,EQFUNC,EXTRACTKEY,ALLOC>  _HT;

public:

    typedef typename _HT::KEY                       key_type;
    typedef VAL                                     value_type;
    typedef EXTRACTKEY                              extractor;
    typedef HASHFUNC                                hasher;
    typedef EQFUNC                                  key_equal;

    typedef size_t                                  size_type;
    typedef ptrdiff_t                               difference_type;
    typedef value_type*                             pointer;
    typedef const value_type*                       const_pointer;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;

    typedef typename _HT::iterator                  iterator;
    typedef typename _HT::const_iterator            const_iterator;
    
    iterator insert( const value_type& val )
    {   return insert_equal(val);    }

    void insert( const value_type* f, const value_type* l )
    {   insert_equal( f, l );   }

    void insert( const_iterator f, const_iterator l ) 
    {   insert_equal( f, l );   }

    const VAL* insert_value( const value_type& val )
    {
        typename _HT::Node** v = _insert_equal<false>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    const VAL* swap_insert_value( value_type& val )
    {
        typename _HT::Node** v = _insert_equal<true>(val);
        return v  ?  &(*v)->_val  :  0;
    }

    VAL* insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _insert_equal_slot(key);
        return v  ?  &(*v)->_val  :  0;
    }

    VAL* find_or_insert_value_slot( const key_type& key )
    {
        typename _HT::Node** v = _find_or_insert_slot(key);
        return &(*v)->_val;
    }


    const VAL* find_value( const key_type& k ) const
    {
        const typename _HT::Node* v = find_node(k);
        return v ? &v->_val : 0;
    }


    hash_multikeyset()
        : _HT( 128, hasher(), key_equal(), extractor() ) {}

    explicit hash_multikeyset( const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex ) {}
    hash_multikeyset( const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex ) {}
    hash_multikeyset( const extractor& ex, const hasher& hf, const key_equal& eql, size_type n=128 )
        : _HT( n, hf, eql, ex ) {}



    hash_multikeyset( const value_type* f, const value_type* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const value_type* f, const value_type* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }


    hash_multikeyset( const_iterator* f, const_iterator* l, size_type n=128 )
        : _HT( n, hasher(), key_equal(), extractor() )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, size_type n=128 )
        : _HT( n, hasher(), key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, size_type n=128 )
        : _HT( n, hf, key_equal(), ex )
    {
        insert_unique( f, l );
    }
    hash_multikeyset( const_iterator* f, const_iterator* l,
        const extractor& ex, const hasher& hf, const key_equal& eqf, size_type n=128 )
        : _HT( n, hf, eqf, ex )
    {
        insert_unique( f, l );
    }

    static metastream& stream_meta( metastream& m )
    {
        MTEMPL_OPEN(m);
        MT(m, KEY);
        MT(m, VAL);
        MTEMPL_CLOSE(m);

        MSTRUCT_OPEN(m, "hash_multikeyset");
        MMAT(m, "elements", value_type );
        MSTRUCT_CLOSE(m);
    }
};


////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator << ( binstream& bin, const hash_keyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_out(bin);    }

////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator >> ( binstream& bin, hash_keyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_in(bin);    }

////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator << ( binstream& bin, const hash_multikeyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_out(bin);    }

////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline binstream& operator >> ( binstream& bin, hash_multikeyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_in(bin);    }


////////////////////////////////////////////////////////////////////////////////
template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator << ( metastream& bin, const hash_keyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_meta(bin);    }

template <class KEY, class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, class ALLOC>
inline metastream& operator << ( metastream& bin, const hash_multikeyset<KEY,VAL,EXTRACTKEY,HASHFUNC,EQFUNC,ALLOC>& a )
{   return a.stream_meta(bin);    }


COID_NAMESPACE_END

#endif //__COID_COMM_HASHKEYSET__HEADER_FILE__
