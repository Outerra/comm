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

#include "net.h"


////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC

#ifndef INCL_WINSOCK_API_PROTOTYPES
#define INCL_WINSOCK_API_PROTOTYPES
#endif

//#include <windows.h>
#include <winsock.h>
#include <stdarg.h>
#include <process.h>

/*
# define htons(A)  ((((uint16)(A) & 0xff00) >> 8) | \
                   (((uint16)(A) & 0x00ff) << 8))
# define htonl(A)  ((((uint32)(A) & 0xff000000) >> 24) | \
                   (((uint32)(A) & 0x00ff0000) >> 8)  | \
                   (((uint32)(A) & 0x0000ff00) << 8)  | \
                   (((uint32)(A) & 0x000000ff) << 24))

# define ntohs(A)  htons(A)
# define ntohl(A)  htonl(A)
*/
////////////////////////////////////////////////////////////////////////////////
//void* LoadLibrary( const char* fname );
//int FreeLibrary( void* hModule );
//void* GetProcAddress( void* hmodule, const char* procname );

//# define HMODULE void*

//void Sleep( unsigned int dwMilliseconds );


# pragma comment(lib, "wsock32.lib")

#else

# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <netinet/tcp.h>
# include <time.h>
# include <sys/time.h>    // Need both for Mandrake 8.0
# include <netdb.h>
# include <fcntl.h>
# include <dlfcn.h>
# include <memory.h>
# include <stdarg.h>

#endif


#if defined(SYSTYPE_MSVC) && !defined(socklen_t)
#define socklen_t int
#endif


namespace coid {

//static token _LOCALHOST_("localhost:");
static token _LOCALHOST("localhost");
static token _BROADCAST("<broadcast>");

////////////////////////////////////////////////////////////////////////////////
void sysSleep( int seconds )
{
    if ( seconds >= 0 )
    {
#ifdef SYSTYPE_MSVC
        Sleep( 1000 * seconds ) ;
        //SleepEx( 1000 * seconds, true ) ;
#else
        timespec ts;
        ts.tv_sec = seconds;
        ts.tv_nsec = 0;
        nanosleep( &ts, 0 );
        //sleep( seconds ) ;
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////
void sysMilliSecondSleep( int milliseconds )
{
    if ( milliseconds >= 0 )
    {
#ifdef SYSTYPE_MSVC
        Sleep( milliseconds ) ;
        //SleepEx( milliseconds, true ) ;
#else
        timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep( &ts, 0 );
        //usleep( milliseconds * 1000 ) ;
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////
uint sysGetPid()
{
#ifdef SYSTYPE_WIN
    return _getpid();
#else
    return getpgrp();
#endif
}

////////////////////////////////////////////////////////////////////////////////
sysDynamicLibrary::sysDynamicLibrary( const char* libname )
{
    handle = 0;
    if( libname )
        open( libname );
}

////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC 

bool sysDynamicLibrary::open( const char* libname )
{
    handle = (ints) LoadLibrary( libname ) ;
    return handle != 0;
}

bool sysDynamicLibrary::close()
{
    bool r = 0!=FreeLibrary( (HMODULE)handle ) ;
    if(r)
        handle = 0;
    return r;
}

const char* sysDynamicLibrary::error() const
{
    return "Unknown error";
}

void *sysDynamicLibrary::getFuncAddress( const char *funcname )
{
    return (void *) GetProcAddress( (HMODULE)handle, funcname ) ; //lint !e611
}

sysDynamicLibrary::~sysDynamicLibrary()
{
    if( handle != 0 )
        FreeLibrary( (HMODULE)handle ) ;
}

#else //////////////////////////////////////////////////////////////////////////

bool sysDynamicLibrary::open( const char* libname )
{
    handle = (void *) dlopen( libname, RTLD_NOW | RTLD_GLOBAL ) ;
    if( ! handle ) {
        const char * err = dlerror();
        if( err ) printf( "%s\n", err );
        return false;
    }
    return handle != 0;
}

bool sysDynamicLibrary::close()
{
    bool r = 0 == dlclose(handle);
    if(r)
        handle = NULL;
    return r;
}

const char* sysDynamicLibrary::error() const
{
    return dlerror();
}

void *sysDynamicLibrary::getFuncAddress ( const char *funcname )
{
    return (handle==NULL) ? NULL : dlsym( handle, funcname ) ;
}

sysDynamicLibrary::~sysDynamicLibrary()
{
    if( handle != NULL )
        dlclose( handle ) ;
}

#endif










////////////////////////////////////////////////////////////////////////////////
void netaddr::set_port( ushort p )
{
    port = htons(p);
}




substring netAddress::protocol = token("://");

////////////////////////////////////////////////////////////////////////////////
netAddress::netAddress( const token& host, int port, bool portoverride )
{
    set( host, port, portoverride ) ;
}

netAddress::netAddress()
{
    sin_family = 2; //AF_INET;
    sin_addr = 0; //INADDR_ANY;
    sin_port = 0;
    ::memset( __pad, 0, sizeof( __pad ) );
}

////////////////////////////////////////////////////////////////////////////////
bool netAddress::isLocalHost () const
{
    return sin_addr == htonl( 0x7f000001 );
}

////////////////////////////////////////////////////////////////////////////////
bool netAddress::isAddrAny () const
{
    return sin_addr == 0;//INADDR_ANY;
}

////////////////////////////////////////////////////////////////////////////////
void netAddress::setAddrAny()
{
    sin_addr = 0;//INADDR_ANY;
}

////////////////////////////////////////////////////////////////////////////////
void netAddress::set( const token& host, int port, bool portoverride )
{
    memset( this, 0, sizeof(netAddress) );

    sin_family = 2;//AF_INET;

    /* Convert a string specifying a host name or one of a few symbolic
    ** names to a numeric IP address.  This usually calls gethostbyname()
    ** to do the work; the names "" and "<broadcast>" are special.
    */

    if( host.is_empty()  ||  host[0] == '\0' )
    {
        sin_addr = 0;   //INADDR_ANY;
        sin_port = htons( port );
        return;
    }

    if( host == _BROADCAST )
    {
        sin_addr = UMAX32;//INADDR_BROADCAST;
    }
    else
    {
        token hostx = host;

        //skip the protocol part if any
        hostx.cut_left( protocol, token::cut_trait_remove_sep_default_empty() );

        token name = hostx.cut_left( ":/", token::cut_trait_keep_sep_with_source() );

        int p = 0;
        if( hostx.first_char() == ':' ) {
            ++hostx;
            p = hostx.toint_and_shift();

            if( !port || !portoverride )
                port = p;
        }

        if( name == _LOCALHOST )
            sin_addr = htonl( 0x7f000001 );
        else
        {
            charstr nameh = name;
            sin_addr = inet_addr(nameh.ptr());
            if( sin_addr == UMAX32 )//INADDR_NONE )
            {
                struct hostent *hp = gethostbyname(nameh.ptr());
                if( hp != NULL )
                {
                    xmemcpy( (char *) &sin_addr, hp->h_addr, hp->h_length );
                }
                else  //failure
                {
                    sin_addr = 0;//INADDR_ANY;
                }
            }
        }
    }

    sin_port = ::htons(port);
}


////////////////////////////////////////////////////////////////////////////////
/* Create a string object representing an IP address.
This is always a string of the form 'dd.dd.dd.dd' (with variable
size numbers). */

charstr& netAddress::getHost( charstr& buf, bool useport ) const
{
    buf.reset();

    long x = ntohl(sin_addr);
    buf << ((x>>24) & 0xff) << '.'
        << ((x>>16) & 0xff) << '.'
        << ((x>> 8) & 0xff) << '.'
        << ((x>> 0) & 0xff);
    if(useport)
        buf << ':' << ntohs(sin_port);
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
charstr& netAddress::getHostName( charstr& buf, bool useport ) const
{
    if (sin_addr == htonl( 0x7f000001 ))
    {
        buf = _LOCALHOST;
        if(useport)
            buf << ':' << ntohs(sin_port);
        return buf;
    }

    //hostent* he = gethostbyaddr( addr, strlen(addr), AF_INET );
    hostent* he = gethostbyaddr( (char*)&sin_addr, sizeof(sin_addr), 2 );//AF_INET );
    if(!he)
    {
        getHost(buf, useport);
        return buf;
    }

    buf = he->h_name;
    if(useport)
        buf << ':' << ntohs(sin_port);
    return buf;
}


////////////////////////////////////////////////////////////////////////////////
int netAddress::getPort() const
{
    return ntohs(sin_port);
}

////////////////////////////////////////////////////////////////////////////////
void netAddress::setPort(int port)
{
    sin_port = htons(port);
}

////////////////////////////////////////////////////////////////////////////////
netAddress* netAddress::getLocalHost( netAddress* adrto )
{
    //gethostbyname(gethostname())

    char buf[256];
    memset(buf, 0, sizeof(buf));
    gethostname( buf, sizeof(buf)-1 );

    adrto->set( buf, 0, false );
    return adrto;
}

////////////////////////////////////////////////////////////////////////////////
const char* netAddress::getLocalHost()
{
    char buf[256];
    memset(buf, 0, sizeof(buf));
    gethostname(buf, sizeof(buf)-1);
    const hostent *hp = gethostbyname(buf);

    if( hp && *hp->h_addr_list )
    {
        in_addr addr = *((in_addr*)*hp->h_addr_list);
        const char* host = inet_ntoa(addr);
        if( host )
            return host ;
    }
    return "127.0.0.1" ;
}

////////////////////////////////////////////////////////////////////////////////
charstr& netAddress::getLocalHostName( charstr& buf )
{
    //gethostbyname(gethostname())

    ::gethostname( buf.get_append_buf(64), 64 );
    buf.correct_size();
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
bool netAddress::getBroadcast() const {
    return sin_addr == UMAX32;//INADDR_BROADCAST;
}

////////////////////////////////////////////////////////////////////////////////
void netAddress::setBroadcast()
{
    getLocalHost(this);
    sin_addr |= htonl( 0x000000ff );
    //sin_addr = INADDR_BROADCAST;
}

////////////////////////////////////////////////////////////////////////////////
netAddress* netSocket::getLocalAddress( netAddress* addrto ) const
{
    socklen_t size = sizeof( sockaddr );
    if (0 == getsockname( handle, (sockaddr*)addrto, &size ))
    {
        if (addrto->sin_addr == 0)
            addrto->sin_addr = htonl( 0x7f000001 );
        return addrto;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
netAddress* netSocket::getRemoteAddress( netAddress* addrto ) const
{
    socklen_t size = sizeof( sockaddr );
    if (0 == getpeername( handle, (sockaddr*)addrto, &size ))
    {
        if (addrto->sin_addr == 0)
            addrto->sin_addr = htonl(0x7f000001);
		if( addrto->sin_addr == htonl(0x7f000001) ) {
			int port = addrto->getPort();
			netAddress::getLocalHost( addrto );
			addrto->setPort( port );
		}
        return addrto;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
netSocket::netSocket ()
{
    handle = UMAXS ;
}

netSocket::~netSocket()
{
    close();
}

void netSocket::setHandle( uints _handle )
{
    close();
    handle = _handle;
}

void netSocket::setHandleInvalid()
{
    handle = UMAXS;
}

////////////////////////////////////////////////////////////////////////////////
bool
netSocket::open ( bool stream )
{
    close();
    handle = ::socket( AF_INET, (stream? SOCK_STREAM: SOCK_DGRAM), 0 ) ;
    return (handle != UMAXS);
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setBlocking( bool blocking )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle

#ifdef SYSTYPE_MSVC

    uint32 nblocking = blocking? 0 : 1;
    ::ioctlsocket( handle, FIONBIO, &nblocking );

#else

    int delay_flag = ::fcntl (handle, F_GETFL, 0);
    if (blocking)
        delay_flag &= (~O_NDELAY);
    else
        delay_flag |= O_NDELAY;
    ::fcntl (handle, F_SETFL, delay_flag);

#endif
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setBroadcast( bool broadcast )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    int result;
    int one = 1;
#ifdef SYSTYPE_WIN
    result = ::setsockopt( handle, SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof(one) );
#else
    result = ::setsockopt( handle, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one) );
#endif
    RASSERTE( result != -1, ersFAILED );
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setBuffers( uint rsize, uint wsize )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    int result;
#ifdef SYSTYPE_WIN
    if(rsize)
        result = ::setsockopt( handle, SOL_SOCKET, SO_RCVBUF, (char*)&rsize, sizeof(rsize) );
    if(wsize)
        result = ::setsockopt( handle, SOL_SOCKET, SO_SNDBUF, (char*)&wsize, sizeof(wsize) );
#else
    if(rsize)
        result = ::setsockopt( handle, SOL_SOCKET, SO_RCVBUF, &rsize, sizeof(rsize) );
    if(wsize)
        result = ::setsockopt( handle, SOL_SOCKET, SO_SNDBUF, &wsize, sizeof(wsize) );
#endif
    RASSERTE( result != -1, ersFAILED );
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setNoDelay( bool nodelay )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    int result;

    int one = nodelay;
#ifdef SYSTYPE_WIN
    result = ::setsockopt( handle, SOL_SOCKET, TCP_NODELAY, (char*)&one, sizeof(one) );
#else
    result = ::setsockopt( handle, SOL_SOCKET, TCP_NODELAY, &one, sizeof(one) );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setReuseAddr( bool reuse )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    int result;

    int one = reuse;
#ifdef SYSTYPE_WIN
    result = ::setsockopt( handle, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one) );
#else
    result = ::setsockopt( handle, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::setLinger( bool blinger, ushort sec )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    int result;

    struct linger lg;
    lg.l_onoff = blinger;
    lg.l_linger = sec;

#ifdef SYSTYPE_WIN
    result = ::setsockopt( handle, SOL_SOCKET, SO_LINGER, (char*)&lg, sizeof(lg) );
#else
    result = ::setsockopt( handle, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg) );
#endif
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::bind( const char* host, int port )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    netAddress addr ( host, port, true ) ;
    return ::bind(handle,(const sockaddr*)&addr,sizeof(netAddress));
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::listen( int backlog )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    return ::listen(handle,backlog);
}

////////////////////////////////////////////////////////////////////////////////
uints netSocket::accept( netAddress* addr )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    socklen_t addr_len = (socklen_t) sizeof(netAddress) ;
    return ::accept(handle,(sockaddr*)addr,&addr_len);
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::connect( const token& host, int port, bool portoverride )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    netAddress addr( host, port, portoverride );

    if( addr.getBroadcast() )
        setBroadcast( true );

    return ::connect( handle, (const sockaddr*)&addr, sizeof(netAddress) );
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::connect( const netAddress& addr )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    if ( addr.getBroadcast() ) {
        setBroadcast( true );
    }
    return ::connect( handle, (const sockaddr*)&addr, sizeof(netAddress) );
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::send( const void * buffer, int size, int flags )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    return ::send (handle, (const char*)buffer, size, flags);
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::sendto( const void * buffer, int size, int flags, const netAddress* to )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    return ::sendto(handle,(const char*)buffer,size,flags,(const sockaddr*)to,sizeof(netAddress));
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::recv( void * buffer, int size, int flags )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    return ::recv (handle, (char*)buffer, size, flags);
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::recvfrom( void * buffer, int size, int flags, netAddress* from )
{
    RASSERTE( handle != UMAXS, ersDISCONNECTED );  //invalid handle
    socklen_t fromlen = (socklen_t) sizeof(netAddress) ;
    return ::recvfrom(handle,(char*)buffer,size,flags,(sockaddr*)from,&fromlen);
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::close()
{
    if ( handle != UMAXS )
    {
#ifdef SYSTYPE_MSVC
        ::closesocket( handle );
#else
        ::close( handle );
#endif
        handle = UMAXS ;
    }
}

////////////////////////////////////////////////////////////////////////////////
void netSocket::lingering_close()
{
    if( handle != UMAXS )
    {
        ::shutdown( handle, 1/*SD_SEND*/ );
        setBlocking(false);
        for(;;)
        {
            char buf[32];
            int n = ::recv( handle, buf, 32, 0 );
            if( n <= 0 )
                break;
        }
        close();
    }
}

////////////////////////////////////////////////////////////////////////////////
bool netSocket::isNonBlockingError()
{
#if defined(SYSTYPE_CYGWIN) || !defined (SYSTYPE_WIN)
    switch (errno) {
    case EWOULDBLOCK: // always == NET_EAGAIN?
    case EALREADY:
    case EINPROGRESS:
        return true;
    }
    return false;
#else
    int wsa_errno = WSAGetLastError();
    if ( wsa_errno != 0 )
    {
        WSASetLastError(0);
        //ulSetError(UL_WARNING,"WSAGetLastError() => %d",wsa_errno);
        switch (wsa_errno) {
        case WSAEWOULDBLOCK: // always == NET_EAGAIN?
        case WSAEALREADY:
        case WSAEINPROGRESS:
            return true;
        }
    }
    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::wait_read( int timeout )
{
    if( handle == UMAXS )
        return 0;

    ::fd_set fdsr;
    FD_ZERO(&fdsr);
    FD_SET( handle, &fdsr );

    struct timeval tv;
    if( timeout>=0 ) {
        tv.tv_sec = timeout/1000;
        tv.tv_usec = (timeout%1000)*1000;
    }

    return ::select( FD_SETSIZE, &fdsr, 0, 0, timeout<0 ? 0 : &tv );
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::wait_write( int timeout )
{
    if( handle == UMAXS )
        return 0;

    ::fd_set fdsr;
    FD_ZERO(&fdsr);
    FD_SET( handle, &fdsr );

    struct timeval tv;
    if( timeout>=0 ) {
        tv.tv_sec = timeout/1000;
        tv.tv_usec = (timeout%1000)*1000;
    }

    return ::select( FD_SETSIZE, 0, &fdsr, 0, timeout<0 ? 0 : &tv );
}

////////////////////////////////////////////////////////////////////////////////
int netSocket::select( netSocket** reads, netSocket** writes, int timeout )
{
    fd_set r,w;

    FD_ZERO (&r);
    FD_ZERO (&w);

    int i, k ;
    int num = 0 ;

    if (reads)
        for ( i=0; reads[i]; i++ )
        {
            ints fd = reads[i]->getHandle();
            DASSERT( fd != -1 );
            FD_SET (fd, &r);
            num++;
        }

    if (writes)
        for ( i=0; writes[i]; i++ )
        {
            ints fd = writes[i]->getHandle();
            DASSERT( fd != -1 );
            FD_SET (fd, &w);
            num++;
        }

    if (!num)
        return num ;

    /* Set up the timeout */
    struct timeval tv ;
    tv.tv_sec = timeout/1000;
    tv.tv_usec = (timeout%1000)*1000;

    // It bothers me that select()'s first argument does not appear to
    // work as advertised... [it hangs like this if called with
    // anything less than FD_SETSIZE, which seems wasteful?]

    // Note: we ignore the 'exception' fd_set - I have never had a
    // need to use it.  The name is somewhat misleading - the only
    // thing I have ever seen it used for is to detect urgent data -
    // which is an unportable feature anyway.

    ::select (FD_SETSIZE, &r, &w, 0, &tv);

    //remove sockets that had no activity

    num = 0 ;

    if (reads)
    {
        for ( k=i=0; reads[i]; i++ )
        {
            ints fd = reads[i]->getHandle();
            if (FD_ISSET (fd, &r)) {
                reads[k++] = reads[i];
                num++;
            }
        }
        reads[k] = NULL ;
    }

    if (writes)
    {
        for ( k=i=0; writes[i]; i++ )
        {
            ints fd = writes[i]->getHandle();
            if (FD_ISSET (fd, &w)) {
                writes[k++] = writes[i];
                num++;
            }
        }
        writes[k] = NULL ;
    }

    return num ;
}

/* Init/Exit functions */
static void netExit ( void )
{
#if defined(SYSTYPE_CYGWIN) || !defined (SYSTYPE_WIN)
#else
    /* Clean up windows networking */
    if ( WSACleanup() == SOCKET_ERROR ) {
        if ( WSAGetLastError() == WSAEINPROGRESS ) {
            WSACancelBlockingCall();
            WSACleanup();
        }
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////
int netInit( int* argc, char** argv )
{
    DASSERTX ( sizeof(sockaddr_in) == sizeof(netAddress), "address struct sizes do not match" ) ;

#ifdef SYSTYPE_MSVC
    /* Start up the windows networking */
    WORD version_wanted = MAKEWORD(1,1);
    WSADATA wsaData;

    if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
        //ulSetError(UL_WARNING,"Couldn't initialize Winsock 1.1");
        return(-1);
    }
#endif

    atexit( netExit ) ;
    return(0);
}
/*
////////////////////////////////////////////////////////////////////////////////
const char* netFormat ( const char* format, ... )
{
    static char buffer[ 256 ];
    va_list argptr;
    va_start(argptr, format);
    vsprintf( buffer, format, argptr );
    va_end(argptr);
    return( buffer );
}
*/
} // namespace coid
