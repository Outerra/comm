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

#ifndef __COMM_NODECLASS__HEADER_FILE__
#define __COMM_NODECLASS__HEADER_FILE__

#include "../namespace.h"

#include "../retcodes.h"
#include "../keywordt.h"
#include "../version.h"
#include "../interfnc.h"

COID_NAMESPACE_BEGIN



////////////////////////////////////////////////////////////////////////////////
#define IMPLEMENTS_INTERFACE_ttree_ifc_BEGIN(classn,ver,baseclass) \
    IMPLEMENTS_INTERFACE_BEGIN(ttree_ifc,classn,ver,baseclass) \
    virtual void* create() const                { return new classn; } \
    virtual void* copy(const void* p) const     { return new classn(*(const classn*)p); } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const { out << *(const classn*)p; } \
    virtual void  stream_in(binstream& in, void* p) const         { in >> *(classn*)p; } \
    virtual void* stream_in_new(binstream& in) const              { classn * tmp = new classn; in >> *tmp; return tmp;}

#define IMPLEMENTS_VIRTUAL_INTERFACE_ttree_ifc_BEGIN(classn,ver,baseclass) \
    IMPLEMENTS_VIRTUAL_INTERFACE_BEGIN(ttree_ifc,classn,ver,baseclass) \
    virtual void* create() const                { return new classn; } \
    virtual void* copy(const void* p) const     { return new classn(*(const classn*)p); } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const { out << *(const classn*)p; } \
    virtual void  stream_in(binstream& in, void* p) const         { in >> *(classn*)p; } \
    virtual void* stream_in_new(binstream& in) const              { classn * tmp = new classn; in >> *tmp; return tmp;}

#define IMPLEMENTS_INTERFACE_ttree_ifc_BEGIN_PURE(classn,ver,baseclass) \
    IMPLEMENTS_INTERFACE_BEGIN(ttree_ifc,classn,ver,baseclass) \
    virtual void* create() const                { return 0; } \
    virtual void* copy(const void* p) const     { return 0; } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const {  } \
    virtual void  stream_in(binstream& in, void* p) const         {  } \
    virtual void* stream_in_new(binstream& in) const              { return 0; }

#define IMPLEMENTS_VIRTUAL_INTERFACE_ttree_ifc_BEGIN_PURE(classn,ver,baseclass) \
    IMPLEMENTS_VIRTUAL_INTERFACE_BEGIN(ttree_ifc,classn,ver,baseclass) \
    virtual void* create() const                { return 0; } \
    virtual void* copy(const void* p) const     { return 0; } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const {  } \
    virtual void  stream_in(binstream& in, void* p) const         {  } \
    virtual void* stream_in_new(binstream& in) const              { return 0; }

#define IMPLEMENTS_VIRTUAL_INTERFACE_ttree_ifc_CUST_BEGIN_PURE(classn,ver,baseclass) \
    IMPLEMENTS_VIRTUAL_INTERFACE_BEGIN(ttree_ifc,classn,ver,baseclass) \
    virtual void* create() const                { return 0; } \
    virtual void* copy(const void* p) const     { return 0; } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual void  stream_out(binstream& out, const void* p) const {  } \
    virtual void  stream_in(binstream& in, void* p) const         {  } \
    virtual void* stream_in_new(binstream& in) const              { return 0; }

#define IMPLEMENTS_INTERFACE_ROOT_ttree_ifc_BEGIN(classn,ver) \
    IMPLEMENTS_INTERFACE_ROOT_BEGIN(ttree_ifc,classn,ver) \
    virtual void* create() const                { return new classn; } \
    virtual void* copy(const void* p) const     { return new classn(*(const classn*)p); } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const { out << *(const classn*)p; } \
    virtual void  stream_in(binstream& in, void* p) const         { in >> *(classn*)p; } \
    virtual void* stream_in_new(binstream& in) const              { classn * tmp = new classn; in >> *tmp; return tmp;}

#define IMPLEMENTS_VIRTUAL_INTERFACE_ROOT_ttree_ifc_BEGIN(classn,ver) \
    IMPLEMENTS_VIRTUAL_INTERFACE_ROOT_BEGIN(ttree_ifc,classn,ver) \
    virtual void* create() const                { return new classn; } \
    virtual void* copy(const void* p) const     { return new classn(*(const classn*)p); } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const { out << *(const classn*)p; } \
    virtual void  stream_in(binstream& in, void* p) const         { in >> *(classn*)p; } \
    virtual void* stream_in_new(binstream& in) const              { classn * tmp = new classn; in >> *tmp; return tmp;}

#define IMPLEMENTS_INTERFACE_ROOT_ttree_ifc_BEGIN_PURE(classn,ver) \
    IMPLEMENTS_INTERFACE_ROOT_BEGIN(ttree_ifc,classn,ver) \
    virtual void* create() const                { return 0; } \
    virtual void* copy(const void* p) const     { return 0; } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const {  } \
    virtual void  stream_in(binstream& in, void* p) const         {  } \
    virtual void* stream_in_new(binstream& in) const              { return 0; }

#define IMPLEMENTS_VIRTUAL_INTERFACE_ROOT_ttree_ifc_BEGIN_PURE(classn,ver) \
    IMPLEMENTS_VIRTUAL_INTERFACE_ROOT_BEGIN(ttree_ifc,classn,ver) \
    virtual void* create() const                { return 0; } \
    virtual void* copy(const void* p) const     { return 0; } \
    virtual void  destroy(void* p) const        { delete (classn*)p; } \
    virtual token get_class_name(const void*p) const            { return _class_name(); } \
    virtual const version& get_version(const void*p) const      { return _get_version(); } \
    virtual void  stream_out(binstream& out, const void* p) const {  } \
    virtual void  stream_in(binstream& in, void* p) const         {  } \
    virtual void* stream_in_new(binstream& in) const              { return 0; }


#define IMPLEMENTS_INTERFACE_ttree_ifc_END \
    IMPLEMENTS_INTERFACE_END

#define IMPLEMENTS_VIRTUAL_INTERFACE_ttree_ifc_END \
    IMPLEMENTS_VIRTUAL_INTERFACE_END

////////////////////////////////////////////////////////////////////////////////
///Exposes class-characteristic behaviour properties of classes
struct ttree_ifc
{
    // pure stuff
    virtual void* create() const                = 0;
    virtual void* copy(const void* p) const     = 0;
    virtual void  destroy(void* p) const        = 0;
    virtual token get_class_name(const void*p) const  = 0;
    virtual const version& get_version(const void*p) const  = 0;
    virtual void  stream_out(binstream& out, const void* p) const = 0;
    virtual void  stream_in(binstream& in, void* p) const         = 0;
    virtual void* stream_in_new(binstream& in) const         = 0;

    // virtual stuff
    ///called before destruction of children or else before detruction of object
    virtual opcd predestroy (void* p) const     { return 0; }

    //operator token() const  { return class_name(0); }
    //operator const version& () const { return get_version(0); }


	/// ipkiss only
    virtual opcd get_children_classes (dynarray< token >& out) const        { return ersNOT_IMPLEMENTED; }
    virtual opcd get_referable_classes (dynarray< token >& out) const       { return ersNOT_IMPLEMENTED; }

    bool can_be_child (const token & name) const
    {
        dynarray< token > list;
        if (get_children_classes (list))  return false;
        for (ulong i=0; i<list.size(); ++i)
            if (name == list[i])  return true;
//            if (0 == strcmp(name, list[i]))  return true;
        return false;
    }
    bool can_be_referred (const token & name) const
    {
        dynarray< token > list;
        if (get_referable_classes (list))  return false;
        for (ulong i=0; i<list.size(); ++i)
            if (name == list[i])  return true;
//          if (0 == strcmp(name, list[i]))  return true;
        return false;
    }


protected:
    virtual ~ttree_ifc() {}
};

////////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_END

#endif //__COMM_NODECLASS__HEADER_FILE__
