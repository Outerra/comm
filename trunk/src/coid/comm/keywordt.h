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

#ifndef __COID_COMM_KEYWORDT__HEADER_FILE__
#define __COID_COMM_KEYWORDT__HEADER_FILE__

#include "namespace.h"
/*
#ifdef SYSTYPE_MSVC
#pragma warning ( disable : 4786 )
#include <hash_map>
#include <hash_set>
#else
#include <ext/hash_map>
#include <ext/hash_set>
#endif
*/

#include "hash/hashset.h"
#include "hash/hashmap.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
#ifndef SYSTYPE_MSVC
template<class K, class V, class H> class keywordmap;
template<class K, class V, class H> binstream& operator << (binstream &, const keywordmap<K,V,H> &);
template<class K, class V, class H> binstream& operator >> (binstream &, keywordmap<K,V,H> &);
#endif //SYSTYPE_MSVC


///keyword table
/*!
*/
template <class KEY, class VAL, class HASH = hash<KEY> >
class keywordmap
{
public:
    typedef typename HASH::key_type                     t_lookup;
    typedef hash_map<KEY, VAL, HASH>                    t_hash;
    typedef typename t_hash::const_iterator             const_iterator;
    typedef typename t_hash::iterator                   iterator;

    t_hash _hash;

    keywordmap() {}

    explicit keywordmap (uints nkey) : _hash(nkey) {
    }

    bool insert( const t_lookup& key, const VAL& val )
    {
        return 0 != _hash.insert_value( std::make_pair(key,val) );
    }

    bool erase( const t_lookup& key )
    {
        size_t v = _hash.erase(key);
        return v>0;
    }

    VAL* find( const t_lookup& key ) const
    {
        return _hash.find_value(key);
    }

    void discard()
    {
        _hash.clear();
    }

    uints size() const                  { return _hash.size(); }

    const_iterator begin() const        { return _hash.begin(); }
    iterator begin()                    { return _hash.begin(); }

    const_iterator end() const          { return _hash.end(); }
    iterator end()                      { return _hash.end(); }

    friend binstream& operator << TEMPLFRIEND(binstream &out, const keywordmap<KEY,VAL,HASH> &k);
    friend binstream& operator >> TEMPLFRIEND(binstream &in, keywordmap<KEY,VAL,HASH> &k);
};

////////////////////////////////////////////////////////////////////////////////
template <class KEY, class HASH = hash<KEY> >
class keywordset
{
public:
    typedef hash_set<KEY, HASH>                         t_hash;
    typedef typename t_hash::const_iterator             const_iterator;
    typedef typename t_hash::iterator                   iterator;

    t_hash _hash;

    keywordset() {}

    explicit keywordset (uints nkey) : _hash(nkey) {
    }

    bool insert( const KEY& key )
    {
        return _hash.insert_value(key);
    }

    bool erase( const KEY& key )
    {
        size_t v = _hash.erase(key);
        return v>0;
    }

    bool find( const KEY& key ) const
    {
        return 0 != _hash.find_value(key);
    }

    void discard()
    {
        _hash.clear();
    }

    uints size() const                  { return _hash.size(); }


    const_iterator begin() const        { return _hash.begin(); }
    iterator begin()                    { return _hash.begin(); }

    const_iterator end() const          { return _hash.end(); }
    iterator end()                      { return _hash.end(); }

    friend binstream& operator << TEMPLFRIEND(binstream &out, const keywordset<KEY,HASH> &k);
    friend binstream& operator >> TEMPLFRIEND(binstream &in, keywordset<KEY,HASH> &k);
};

////////////////////////////////////////////////////////////////////////////////
template <class T, class H>
binstream& operator << (binstream &bot, const keywordset<T,H> &k)
{
    typename keywordset<T,H>::const_iterator cb = k.begin();
    typename keywordset<T,H>::const_iterator ce = k.end();

    bot << k.size();

    for( ; cb != ce; ++cb )
        bot << *cb;

    return bot;
}

template <class T, class H>
binstream& operator >> (binstream &bin, keywordset<T,H> &k)
{
    uints n;
    bin >> n;

    for( uints i=0; i<n; ++i )
    {
        T t;
        bin >> t;
        k.insert(t);
    }

    return bin;
}

////////////////////////////////////////////////////////////////////////////////
template <class K, class T, class H>
binstream& operator << (binstream &bot, const keywordmap<K,T,H> &k)
{
    typename keywordmap<K,T,H>::const_iterator cb = k.begin();
    typename keywordmap<K,T,H>::const_iterator ce = k.end();

    bot << k.size();

    for( ; cb != ce; ++cb )
        bot << (*cb).first << (*cb).second;

    return bot;
}

template <class K, class T, class H>
binstream& operator >> (binstream &bin, keywordmap<K,T,H> &km)
{
    uints n;
    bin >> n;

    for( uints i=0; i<n; ++i )
    {
        K k;
        T t;
        bin >> k >> t;
        km.insert( k, t );
    }

    return bin;
}


COID_NAMESPACE_END

#endif // __COID_COMM_KEYWORDT__HEADER_FILE__
