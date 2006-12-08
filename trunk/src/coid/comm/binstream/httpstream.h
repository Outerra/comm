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

#ifndef __COID_COMM_HTTPSTREAM__HEADER_FILE__
#define __COID_COMM_HTTPSTREAM__HEADER_FILE__

#include "coid/comm/namespace.h"
#include "coid/comm/str.h"
#include "coid/comm/rnd.h"
#include "coid/comm/net.h"
#include "coid/comm/txtconv.h"

#include "coid/comm/binstream/cachestream.h"
#include "coid/comm/binstream/binstreambuf.h"
#include "coid/comm/binstream/filestream.h"
#include "coid/comm/binstream/txtstream.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class httpstream : public binstream
{
public:
    virtual void set_content_type( const char* ct )    {}

    virtual uint64 get_session_id() const = 0;
    virtual void set_session_id( uint64 ssid ) = 0;
};

////////////////////////////////////////////////////////////////////////////////
class httpstreamcoid : public httpstream
{
public:
    ///Http header
    struct header
    {
        uint64 _ssid;
        uint _ssidpathlen;          ///< length of path string of sessioncoid cookie, pick the longer

        uints _content_length;
        int _errcode;

        timet _if_mdf_since;

        enum { TE_CHUNKED = 0x01, TE_TRAILERS = 0x02, TE_DEFLATE = 0x04, TE_GZIP = 0x08, TE_IDENTITY = 0x10, };
        uint _te;

        bool _bclose;
        bool _bhttp10;
        bool _isdir;
        bool _isfile;

        charstr _method;
        charstr _fullpath;

        token _query;               ///< query part (after ?)
        token _relpath;             ///< relative path


        header()
        {
            _errcode = 0;
            _content_length = UMAX;
            _bclose = _bhttp10 = false;
            _te = 0;
        }

        opcd decode( bool breq, cachestream& bin, binstream* log )
        {
            binstreambuf buf;

            _ssid = 0;
            _bclose = true;
            _te = 0;
            _content_length = UMAX;
            _if_mdf_since = -1;

            //skip any nonsense 
            opcd e;
            for(;;)
            {
                e = bin.read_until( substring_rn(), &buf );
                if(e)
                    return e;

                token t = buf;

                if(log)
                    *log << t << "\r\n";

                if( breq  ||  t.begins_with_icase("http/") ) break;

                buf.reset();
            }
             
            token n, tok = buf;
            tok.skip_char(' ');

            if(breq)
            {
                token meth = tok.cut_left(' ',1);
                _method = meth;

                token path = tok.cut_left(' ',1);

                static token _HTTP = "http://";

                //check the path
                if( path.begins_with_icase(_HTTP) )
                {
                    path += _HTTP._len;

                    path.skip_notchar('/');
                }

                //skip leading /.
                path.skip_ingroup("/.");

                _fullpath = path;
                path = _fullpath;

                _relpath = path.cut_left('?',1);
                _query = path;


                token proto = tok.cut_left(' ',1);
                if( proto.cmpeqi("http/1.0") )
                    _bhttp10 = _bclose = true;
                else if( proto.cmpeqi("http/1.1") )
                    _bhttp10 = _bclose = false;
                else
                    return ersFE_UNRECG_REQUEST "unknown protocol";
            }
            else
            {
                token proto= tok.cut_left(' ',1);
                if( proto.cmpeqi("http/1.0") )
                    _bhttp10 = _bclose = true;
                else if( proto.cmpeqi("http/1.1") )
                    _bhttp10 = _bclose = false;
                else
                    return ersFE_UNRECG_REQUEST "unknown protocol";

                token errcd = tok.cut_left(' ',1);
                _errcode = errcd.toint();

                if( _errcode != 200 )
                    return ersFE_UNKNOWN_ERROR;
            }

            _ssidpathlen = 0;
            //read remaining headers
            for(;;)
            {
                buf.reset();

                e = bin.read_until( substring_rn(), &buf );
                if(e)
                    return e;

                if( buf.is_empty() )
                    return 0;

                token h = buf;

                if(log)
                    *log << h << "\r\n";

                token h1 = h.cut_left(':',1);
                h.skip_char(' ');
/*
                if( h1.cmpeqi("Cookie") )
                {
                    uint64 ssid=0;
                    bool bset=true;
                    for(;;)
                    {
                        token par = h.cut_left("; ",-1);

                        token k = par.cut_left('=',1,true);
                        if( k.is_empty() )  break;

                        if( k.begins_with_icase("sessioncoid") )
                        {
                            token id = par.cut_left(',',1);
                            ssid = id.touint64();
                            while(!par.is_empty())
                            {
                                token t = par.cut_left(',',1);
                                t.skip_char(' ');
                                if(t.begins_with_icase("path")) {
                                    t.cut_left("= ",1);
                                    if( _ssidpathlen > t.len() )
                                        bset=false;
                                    break;
                                }
                            }
                        }
                    }

                    if(bset && ssid!=0)
                        _ssid = ssid;
                }
                */
                if( h1.cmpeqi("Session-Coid") )
                {
                    _ssid = h.touint64();
                }
                else if( h1.cmpeqi("TE")  ||  h1.cmpeqi("Transfer-Encoding") )
                {
                    for(;;)
                    {
                        token k = h.cut_left(", ",-1);
                        if( k.is_empty() )  break;

                        if( k.cmpeqi("deflate") )
                            _te |= TE_DEFLATE;
                        else if( k.cmpeqi("gzip") )
                            _te |= TE_GZIP;
                        else if( k.cmpeqi("chunked") )
                            _te |= TE_CHUNKED;
                        else if( k.cmpeqi("trailers") )
                            _te |= TE_TRAILERS;
                        else if( k.cmpeqi("identity") )
                            _te |= TE_IDENTITY;
                    }
                }
                else if( h1.cmpeqi("Connection") )
                {
                    for(;;)
                    {
                        token k = h.cut_left(", ",-1);
                        if( k.is_empty() )  break;

                        if( k.cmpeqi("close") )
                            _bclose = true;
                        else if( k.cmpeqi("keep-alive") )
                            _bclose = false;
                    }
                }
                else if( h1.cmpeqi("Content-Length") )
                {
                    _content_length = h.toint();
                }
                else if( h1.cmpeqi("If-Modified-Since") )
                {
                    h.todate_gmt(_if_mdf_since);
                }
            }

            return e;
        }
    };

public:
    ///
    class cachestreamex : public cachestream
    {
        uints _hdrpos;

        static const token& header()
        { static token _T = "Content-Length:                     \r\n\r\n";  return _T; }

    public:

        cachestreamex() { _hdrpos = UMAX; }

        opcd set_hdr_pos()
        {
            _hdrpos = len() + 16;   //skip "Content-Length: "
            const token& t = header();
            uints n;
            return write_raw( t.ptr(), n=t.len() );
        }

        opcd on_cache_flush( void* p, uints size, bool final )
        {
            if(!final)
                return ersDENIED;   //no multiple packets supported

            DASSERT( _hdrpos != UMAX );
            //write msg len
            char* pc = (char*) get_raw( _hdrpos, 20 );

            //write content length
            charstr::num<int64>::insert( pc, 20, len() - _hdrpos - 20-4, 10, 0, 20, charstr::ALIGN_NUM_RIGHT );
            _hdrpos = UMAX;

            return 0;
        }
    };



    static const substring& substring_rn()
    {
        static substring _ss( "\r\n" );
        return _ss;
    }

    static const substring& substring_0()
    {
        static substring _ss( "", 1 );
        return _ss;
    }

public:


    virtual opcd on_new_read()          { return 0; }
    virtual opcd on_new_write()         { return 0; }



    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return fATTR_IO_FORMATTING | fATTR_HANDSHAKING | fATTR_READ_UNTIL;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        opcd e = check_write();
        if(e) return e;

        return _cache.write_raw( p, len );
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        opcd e = check_read();
        if(e) return e;

        return _cache.read_raw( p, len );
    }

    virtual void flush()
    {
        _cache.flush();
        _flags &= ~fWSTATUS;

        if( (_flags & fRESPONSE)  &&  _hdr->_bclose )
        {
            _cache.close(true);
        }
    }

    virtual void acknowledge( bool eat=false )
    {
        _cache.acknowledge(eat);
        _flags &= ~fRSTATUS;

        if( !(_flags & fRESPONSE)  &&  _hdr->_bclose )
        {
            _cache.close(true);
        }
    }

    virtual bool is_open() const
    {
        return _cache.is_open();
    }

    virtual void reset()
    {
        _flags &= ~(fWSTATUS | fRSTATUS);
        _cache.reset();
//        _ssid = 0;

        _seqnum = _rnd.rand();
    }


    httpstreamcoid()
    {
        _flags = fSETSESSION;
        _seqnum = _rnd.rand();

        _hdrx = new header;
        _hdr = _hdrx;
        _cache.reserve_buffer_size(1024);
        _tcache.bind(_cache);

        load_opthdr();
    }

    httpstreamcoid( binstream& bin )
    {
//        _ssid = 0;
        _flags = fSETSESSION;
        _seqnum = _rnd.rand();

        _hdrx = new header;
        _hdr = _hdrx;
        _cache.reserve_buffer_size(1024);
        _cache.bind(bin);
        _tcache.bind(_cache);

        load_opthdr();
    }

    httpstreamcoid( header& hdr, cachestream& cache )
    {
//        _ssid = 0;
        _flags = fSKIP_HEADER | fSETSESSION;
        _seqnum = _rnd.rand();

        _hdr = &hdr;
        _cache.bind( *cache.bound() );
        _cache.swap(cache);
        _tcache.bind(_cache);

        load_opthdr();
    }

    void load_opthdr()
    {
        _content_type_qry = "Content-Type: text/plain\r\n";
        _content_type_rsp = "Content-Type: text/plain\r\n";

        bifstream bif(".opthdr");
        if(bif.is_open())
        {
            txtstream txt(bif);
            txt >> _opthdr;
        }
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return _cache.read_until( ss, bout, max_size );
    }

    virtual opcd bind( binstream& bin, int io=0 )
    {
        return _cache.bind( bin, io );
    }

    virtual opcd set_timeout( uint ms )
    {
        return _cache.set_timeout(ms);
    }




    void set_host( const token& tok )
    {
        (_proxyreq = "Host: ") << tok << "\r\n";
        (_urihdr = tok) << "/?.t";
    }

    void set_host( const netAddress& addr )
    {
        charstr a;
		addr.getHost(a, true);
        (_proxyreq = "Host: ") << a << "\r\n";
        (_urihdr = a) << "/?.t";
    }

    void set_response_type( bool resp=true )
    {
        if( resp )
            _flags |= fRESPONSE;
        else
            _flags &= ~fRESPONSE;
    }

    virtual void set_content_type( const char* ct )
    {
        if( _flags & fRESPONSE )
            _content_type_rsp = ct ? ct : "Content-Type: text/plain\r\n";
        else
            _content_type_qry = ct ? ct : "Content-Type: text/plain\r\n";
    }

    void set_skip_header()
    {
        _flags |= fSKIP_HEADER;
        //_len = content_length;
    }

    void set_connection_type( bool bclose )
    {
        if(bclose)
            _flags |= fCLOSE_CONN;
        else
            _flags &=~(int)fCLOSE_CONN;
    }

    bool is_writting()                  { return (_flags & fWSTATUS) != 0; }
    bool is_reading()                   { return (_flags & fRSTATUS) != 0; }


    virtual uint64 get_session_id() const
    {
        return _hdr->_ssid;
    }

    virtual void set_session_id( uint64 sid )
    {
        set_response_type(true);    //setting session id implies that the caller is server
        _flags |= fSETSESSION;

        _hdr->_ssid = sid;
    }

protected:

    txtstream _tcache;
    cachestreamex _cache;

    local<header>  _hdrx;
    header* _hdr;

    charstr _proxyreq;
    charstr _urihdr;
    charstr _opthdr;

    token _content_type_qry;
    token _content_type_rsp;

    rnd_int _rnd;
    uint _seqnum;

    netAddress _addr;
    uint _flags;
    enum {
        fRSTATUS                = 0x01,         ///< closed/transm.open
        fWSTATUS                = 0x02,         ///< reading/all read
        fRESPONSE               = 0x04,         ///< request/response header mode

        fSETSESSION             = 0x10,         ///< write session cookie
        fSKIP_HEADER            = 0x20,
        fCLOSE_CONN             = 0x40,

        REQUEST_NEW             = 0xba,         ///< new connection request
        REQUEST_OLD             = 0xc9,         ///< estabilished conn.req.
    };


protected:

    opcd check_read()
    {
        if( (_flags & fRSTATUS) != 0 )
            return 0;
        //receive header
        return new_read();
    }

    opcd check_write()
    {
        if( (_flags & fWSTATUS) != 0 )
            return 0;
        //write header
        return new_write();
    }

    ///
    opcd new_write()
    {
        if( _flags & fWSTATUS )
            return ersUNAVAILABLE;

        //_cache.set_timeout(0);

        static token _POST( "POST http://" );
        static token _POST1(
            " HTTP/1.1\r\n"
            );
        static token _POST2(
            "Accept: text/plain\r\n"
            "MIME-Version: 1.0\r\n"
            );

        static token _RESP(
            //"X-PLEASE_WAIT: .\r\n"
            "HTTP/1.1 200 OK\r\n"
            "Server: COID/HT\r\n"
            "Cache-Control: no-cache\r\n"
            "MIME-Version: 1.0\r\n"
            );

        _flags |= fWSTATUS;

        if( _flags & fRESPONSE )
            _tcache << _RESP << _content_type_rsp;
        else
            _tcache << _POST << _urihdr << (++_seqnum) << _rnd.rand()
                << _POST1 << _proxyreq << _POST2 << _content_type_qry;

        charstr date;
        time_t t0;
        date.append_date_gmt( time(&t0) );
        _tcache << "Date: " << date << "\r\n";


        static token _SSID_COOKIE( "Set-Cookie: sessioncoid=" );
        static token _SSID( "Session-Coid: " );
        if( (_flags & fSETSESSION) && _hdr->_ssid ) {
            //_tcache << _SSID_COOKIE << _hdr->_ssid;
            //_tcache << "\r\n";
            _tcache << _SSID << _hdr->_ssid;
            _tcache << "\r\n";
            _flags &= ~fSETSESSION;
        }

        //
        static token _CONC( "Connection: Close\r\n" );
        static token _CONKA( "Connection: Keep-Alive\r\n" );
        _tcache << ( _hdr->_bclose ? _CONC : _CONKA );

        _cache.set_hdr_pos();

        return on_new_write();
    }

    ///
    opcd new_read()
    {
        if( _flags & fRSTATUS )
            return ersUNAVAILABLE;

        //_cache.set_timeout(4000);
        _flags |= fRSTATUS;

        opcd e;

        if( (_flags & fSKIP_HEADER) == 0 )
        {
            binstreambuf buf;
            e = _hdr->decode( 0, _cache, &buf );

            if(!e  &&  (_flags & fRESPONSE) )
            {
                if( !_hdr->_fullpath.begins_with("?.t") )
                    e = ersFE_UNRECG_REQUEST;
            }

            if(e)
            {
                 //_cache.set_timeout(10);
                 _cache.read_until( substring_0(), &buf );
                 //_cache.set_timeout(0);

                 bofstream bf("tunnel-http.log.html?wb+");
                 txtstream txt(bf);
                 txt << (token)buf
                     << "-------------------------------------------------------------------------------------\n\n";

                 //_c6.set_all_read();

                 return ersFE_UNRECG_REQUEST;
            }
            else
            {
                 bofstream bf("tunnel-http.log.html?wb+");
                 txtstream txt(bf);
                 txt << (token)buf
                     << "-------------------------------------------------------------------------------------\n\n";
            }
        }

        if(!e)
            e = on_new_read();

        return e;
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_HTTPSTREAM__HEADER_FILE__

