/*
 * net.h - network interface
 *
 *  Copyright(C) 2001 by Shiro Kawai (shiro@acm.org)
 *
 *  Permission to use, copy, modify, distribute this software and
 *  accompanying documentation for any purpose is hereby granted,
 *  provided that existing copyright notices are retained in all
 *  copies and that this notice is included verbatim in all
 *  distributions.
 *  This software is provided as is, without express or implied
 *  warranty.  In no circumstances the author(s) shall be liable
 *  for any damages arising out of the use of this software.
 *
 *  $Id: net.h,v 1.2 2001-05-25 09:07:24 shirok Exp $
 */

#ifndef GAUCHE_NET_H
#define GAUCHE_NET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <gauche.h>
#include "netconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================
 * Socket
 */

/*------------------------------------------------------------------
 * Socket address
 */

typedef struct ScmSockAddrRec {
    SCM_HEADER;
    union {
        struct sockaddr common;
        struct sockaddr_in addr_in;
        struct sockaddr_un addr_un;
    } addr;
} ScmSockAddr;

extern ScmClass Scm_SockAddrClass;
#define SCM_CLASS_SOCKADDR    (&Scm_SockAddrClass)
#define SCM_SOCKADDR(obj)     ((ScmSockAddr*)(obj))
#define SCM_SOCKADDRP(obj)    SCM_XTYPEP(obj, SCM_CLASS_SOCKADDR)

void Scm_StringToSockAddr(ScmString *address, ScmSockAddr *result);

ScmObj Scm_MakeSockAddrInet(ScmString *host, ScmString *port);
ScmObj Scm_MakeSockAddrUnix(ScmString *path);

/*------------------------------------------------------------------
 * Socket
 */

typedef struct ScmSocketRec {
    SCM_HEADER;
    int fd;                     /* -1 if closed */
    int status;
    struct sockaddr *address;
    ScmPort *inPort;
    ScmPort *outPort;
    ScmString *name;
} ScmSocket;

enum {
    SCM_SOCKET_STATUS_NONE,
    SCM_SOCKET_STATUS_BOUND,
    SCM_SOCKET_STATUS_LISTENING,
    SCM_SOCKET_STATUS_CONNECTED,
    SCM_SOCKET_STATUS_SHUTDOWN,
    SCM_SOCKET_STATUS_CLOSED
};

extern ScmClass Scm_SocketClass;
#define SCM_CLASS_SOCKET   (&Scm_SocketClass)
#define SCM_SOCKET(obj)    ((ScmSocket*)obj)
#define SCM_SOCKETP(obj)   SCM_XTYPEP(obj, SCM_CLASS_SOCKET)

extern ScmObj Scm_MakeSocket(int domain, int type, int protocol);
extern ScmObj Scm_SocketShutdown(ScmSocket *s, int how);
extern ScmObj Scm_SocketClose(ScmSocket *s);

extern ScmObj Scm_SocketInputPort(ScmSocket *s);
extern ScmObj Scm_SocketOutputPort(ScmSocket *s);

extern ScmObj Scm_SocketBind(ScmSocket *s, ScmObj address);
extern ScmObj Scm_SocketConnect(ScmSocket *s);
extern ScmObj Scm_SocketListen(ScmSocket *s, int backlog);
extern ScmObj Scm_SocketAccept(ScmSocket *s);

extern ScmObj Scm_MakeClientSocket(ScmString *host, int port);
extern ScmObj Scm_MakeServerSocket(int port);


/*==================================================================
 * Netdb interface
 */

/*
 * Protcol Entry
 */

typedef struct ScmSysProtoEntRec {
    SCM_HEADER;
    struct protoent entry;
} ScmSysProtoEnt;

extern ScmClass Scm_ProtoEntClass;
#define SCM_CLASS_PROTOENT  (&Scm_ProtoEntClass)
#define SCM_PROTOENT(obj)   ((ScmProtoEnt*)obj)
#define SCM_PROTOENTP(obj)  SCM_XTYPEP(obj, SCM_CLASS_PROTOENT)

#ifdef __cplusplus
}
#endif

#endif /*GAUCHE_NET_H */
