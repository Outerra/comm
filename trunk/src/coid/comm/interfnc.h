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

#ifndef __COID_COMM_INTERFNC__HEADER_FILE__
#define __COID_COMM_INTERFNC__HEADER_FILE__

#include "namespace.h"

#include "version.h"
#include "str.h"
#include "singleton.h"
#include "sync/guard.h"

#include "hash/hashkeyset.h"


COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
/// Interface register template
template <class IFACE>
class ClassRegister
{

    struct element_data
    {
        IFACE* _ifc;
        const element_data* _par;
        void*  _ctx;

        operator token () const             { return get_class_name();  }

        token get_class_name() const        { return _ifc->get_class_name(_ctx);  }
        const version& get_version() const  { return _ifc->get_version(_ctx);  }


        element_data( IFACE* ifc, void* ctx ) : _ifc(ifc),_par(0),_ctx(ctx)  {}
        element_data() : _ifc(0),_par(0),_ctx(0) {}
    };

public:

    class element
    {
        const element_data* eld;

    public:
        element() : eld(0)                  {}
        //element( IFACE* ifc, void* ctx ) : eld(ifc,ctx) {}
        element(const element_data* e) : eld(e) {}
        element(const element_data& e) : eld(&e) {}

        void clear()    { eld=0; }

        //operator token () const             { return token(*eld->_ifc);  }
        //operator const version& () const    { return (const version&)(*eld->_ifc);  }

        token get_class_name() const        { return eld->get_class_name();  }
        const version& get_version() const  { return eld->get_version();  }

        IFACE* get_iface() const            { return eld->_ifc; }

        IFACE* operator -> ()               { return eld->_ifc; }
        const IFACE* operator -> () const   { return eld->_ifc; }

        IFACE& operator * ()                { return *eld->_ifc; }

        //bool operator == (const element e) const    { return _ifc == e._ifc; }
        //bool operator != (const element e) const    { return _ifc != e._ifc; }


        bool is_of_type( const element e ) const
        {
            for( const element_data* t=eld; t; t=t->_par )
			{
                if( t == e.eld  ||  t->get_class_name() == e.eld->get_class_name() )  return true;
			}

            return false;
        }

        bool is_of_type( const element e, const version* ver ) const
        {
            for( const element_data* t=eld; t; t=t->_par )
            {
                if( e.get_class_name() != t->get_class_name() )  continue;
                if( ver  &&  !ver->can_use_provider(t->get_version()) ) continue;
                return true;
            }
            return false;
        }

        bool is_of_type( const token& n, const version* ver ) const
        {
            for( const element_data* t=eld; t; t=t->_par )
            {
                if( n != t->get_class_name() )  continue;
                if( ver  &&  !ver->can_use_provider(t->get_version()) ) continue;
                return true;
            }
            return false;
        }

        bool is_set() const                 { return eld != 0; }

        friend binstream& operator << (binstream& bin, const element& e)
        {
            if (! e.is_set())  return bin << 0L;
            return bin << e.get_class_name();
        }

        friend binstream& operator >> (binstream& bin, element& e)
        {
            charstr name;
            bin >> name;
            if( name.is_empty() ) return bin;
            e = SINGLETON(ClassRegister<IFACE>).find_only(name);
            if( !e.is_set() )
                throw ersNOT_FOUND "class to stream object";
            return bin;
        }
    };

    struct Any
    {
        static element GET_INTERFACE()          { return element(); }
        static element GET_VIRTUAL_INTERFACE()  { return element(); }
    };


    typedef element         NodeClass;


    element add( IFACE* svc, void* ctx=0, element parent = Any::GET_INTERFACE() )
    {
        element_data e(svc,ctx);
        GUARDME;
        if( parent.is_set() )
        {
            const element_data* ed = _hash.find_value(parent.get_class_name());
            RASSERTX( ed, "parent class not registered" );
            e._par = ed;
        }
        return _hash.insert_value(e);
    }

    bool del( token e, const version* ver = 0 )
    {
        GUARDME;
        return 0 != _hash.erase(e);
    }

    element find_or_create( IFACE* ifc, void* ctx=0, element parent = Any::GET_INTERFACE() )
    {
        GUARDME;

        element_data e(ifc,ctx);
        if( parent.is_set() )
        {
            const element_data* ed = _hash.find_value(parent.get_class_name());
            RASSERTX( ed, "parent class not registered" );
            e._par = ed;
        }

        token name = e.get_class_name();
        const version& ver = e.get_version();

        t_ci ci = _hash.find(name);
        t_ci ce = _hash.end();

        for( ; ci!=ce; ++ci )
        {
            if( (*ci).get_class_name() != name )  break;
            if( (*ci).get_version() == ver )  return element(*ci);
        }

        const element_data* pe = _hash.insert_value(e);
        return element(*pe);
        //return element( *_hash.insert_value(e) );
    }

    element find_best( const token& name, const version& ver, version* verout=0 ) const
    {
        GUARDME;
        
        uint minminor=UMAX, minbuild=UMAX;
        
        version verx;
        bool sciset = false;
        t_ci sci;

        t_ci ci = _hash.find(name);
        t_ci ce = _hash.end();

        for( ; ci!=ce; ++ci )
        {
            const element_data& psc = *ci;
            if( psc.get_class_name() != name )
                break;

            const version& veri = psc.get_version();
            if( ver._build == UMAX )
            {
                if( !sciset  ||  veri > verx )
                {
                    verx = veri;
                    sci = ci;
                    sciset = true;
                }
            }
            else if( ver.can_use_provider( veri ) )
            {
                //only minor versions differ or no particular version was queried (build=UMAX)

                int m = ver.get_minor() - veri.get_minor();
                if( m < 0 )  m = -m;
                int b = ver._build - veri._build;
                if( b < 0 )  b = -b;

                if( (uint)m < minminor )
                {
                    sci = ci;
                    sciset = true;
                    minminor = m;
                    minbuild = b;
                }
                else if( (uint)m == minminor  &&  (uint)b < minbuild )
                {
                    sci = ci;
                    sciset = true;
                    minbuild = b;
                }
            }
        }

        if( sciset )
        {
            if( verout )
                *verout = (*sci).get_version();
            return element(*sci);
        }

        return element();
    }

    element find_exact( const token& name, const version& ver ) const
    {
        GUARDME;

        t_ci ci = _hash.find(name);
        t_ci ce = _hash.end();

        for( ; ci!=ce; ++ci )
        {
            if( (*ci).get_class_name() != name )  break;
            if( (*ci).get_version() == ver )
                return element(*ci);
        }

        return element();
    }

    element find_only( const token& name ) const
    {
        GUARDME;

        t_ci ci = _hash.find(name);
        t_ci ce = _hash.end();

        if( ci != ce  &&  (*ci).get_class_name() == name )
            return element(*ci);
        return element();
    }

    uints find_all( const token& name, dynarray<element>& col ) const
    {
        GUARDME;

        t_ci ci = name.is_empty() ? _hash.begin() : _hash.find(name);
        t_ci ce = _hash.end();

        for( ; ci!=ce; ++ci )
        {
            if( !name.is_empty() && (*ci).get_class_name() != name )  break;
            *col.add() = element(*ci);
        }

        return col.size();
    }


    typedef hash_multikeyset<token,element_data,_Select_Copy<element_data,token> >  t_hash;
    typedef typename t_hash::const_iterator  t_ci;

    
    class ClassRegisterIterator : public std::iterator<std::forward_iterator_tag, IFACE>
    {
        t_ci _ci;
        ClassRegister<IFACE>* _cr;
    
    public:
        typedef typename t_ci::value_type       value_type;

        typedef size_t                 size_type;
        typedef ptrdiff_t              difference_type;
        typedef IFACE*                 pointer;
        typedef const IFACE*           const_pointer;
        typedef IFACE&                 reference;
        typedef const IFACE&           const_reference;

        ClassRegisterIterator( const ClassRegister<IFACE>& ht, t_ci it ) : _ci(it), _cr((ClassRegister<IFACE>*)&ht)
        {
        }
        ClassRegisterIterator() : _cr(0) {}

        ClassRegisterIterator( const ClassRegisterIterator& p )
        {
            _ci = p._ci;
            _cr = p._cr;
        }

        ClassRegisterIterator& operator = (const ClassRegisterIterator& p)
        {
            _ci = p._ci;
            _cr = p._cr;
            return *this;
        }

        bool operator == (const ClassRegisterIterator& p) const      { return _ci == p._ci; }
        bool operator != (const ClassRegisterIterator& p) const      { return _ci != p._ci; }

        const_reference operator*() const           { return *(*_ci)._ifc; }
        const_pointer operator ->()                 { return (*_ci)._ifc; }

        ClassRegisterIterator& operator++()
        {
            MXGUARD(_cr->_mutex);
            ++_ci;
            return *this;
        }
        
        inline ClassRegisterIterator operator++(int)
        {
            MXGUARD(_cr->_mutex);
            _ci++;
            return *this;
        } 
    };

    friend class ClassRegisterIterator;


    ClassRegisterIterator begin() const     { return ClassRegisterIterator(*this,_hash.begin()); }
    ClassRegisterIterator end() const       { return ClassRegisterIterator(*this,_hash.end()); }


    ClassRegister() {
        _mutex.set_name( "ClassRegister" );
    }


    typedef ClassRegisterIterator       const_iterator;

private:
    t_hash _hash;

    mutable comm_mutex _mutex;

//    static element get_element( const element_data* e )     { element x;  x.eld = e;  return x; }
};


//////////////////////////////////////////////////////////////////////////
#define INTERFACE_REGISTER(iface) \
    singleton< ClassRegister<iface> >::instance()

/// Begins implementation of interface within a class
#define IMPLEMENTS_INTERFACE_BEGIN(iface,classname,ver,baseclass) \
    static ClassRegister<iface>::NodeClass GET_INTERFACE() { return SINGLETON(__##iface)._class; } \
    struct __##iface : iface { ClassRegister<iface>::NodeClass _class; \
    static token _class_name() { static token _T = #classname;  return _T; } \
    static const version& _get_version() { static version _V(ver);  return _V; } \
    __##iface() { _class = SINGLETON(ClassRegister<iface>).find_or_create(this,0,baseclass::GET_INTERFACE()); }

/// Begins implementation of interface within a class
#define IMPLEMENTS_INTERFACE_ROOT_BEGIN(iface,classname,ver) \
    static ClassRegister<iface>::NodeClass GET_INTERFACE() { return SINGLETON(__##iface)._class; } \
    struct __##iface : iface { ClassRegister<iface>::NodeClass _class; \
    static token _class_name() { static token _T = #classname;  return _T; } \
    static const version& _get_version() { static version _V(ver);  return _V; } \
    __##iface() { _class = SINGLETON(ClassRegister<iface>).find_or_create(this,0); }

/// Ends implementation of interface within a class
#define IMPLEMENTS_INTERFACE_END \
    };

/// Begins implementation of interface within a class
#define IMPLEMENTS_VIRTUAL_INTERFACE_BEGIN(iface,classname,ver,baseclass) \
    virtual ClassRegister<iface>::NodeClass GET_VIRTUAL_INTERFACE() const { return GET_INTERFACE(); } \
    IMPLEMENTS_INTERFACE_BEGIN(iface,classname,ver,baseclass)

/// Begins implementation of interface within a class
#define IMPLEMENTS_VIRTUAL_INTERFACE_ROOT_BEGIN(iface,classname,ver) \
    virtual ClassRegister<iface>::NodeClass GET_VIRTUAL_INTERFACE() const { return GET_INTERFACE(); } \
    IMPLEMENTS_INTERFACE_ROOT_BEGIN(iface,classname,ver)

/// Ends implementation of interface within a class
#define IMPLEMENTS_VIRTUAL_INTERFACE_END \
    };

/*
/// Registers interface for given class
#define IMPLEMENTS_INTERFACE_REGISTER(classname,iface) \
static const iface& __iface_initializer__##classname = INTERFACE_REGISTER(iface).add( #classname, classname::GET_INTERFACE() );

/// Registers null interface for given class
#define IMPLEMENTS_INTERFACE_REGISTER_NULL(classname,iface) \
static const ClassRegister<iface>::Element& __iface_initializer__##classname = INTERFACE_REGISTER(iface).add( #classname, ClassRegister<iface>::Element(0,"0.0.0.0") );
*/
////////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_END

#endif //__COID_COMM_INTERFNC__HEADER_FILE__
