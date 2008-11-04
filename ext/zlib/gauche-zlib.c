/*
 * Gauche-zlib - zlib module
 *
 *    Copyright (c) 2006 Rui Ueyama, All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the authors nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *    TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *   $Id: zlib.c,v 1.2 2006-01-17 18:20:12 rui Exp $
 */

#include "gauche-zlib.h"
#include <gauche/exception.h>
#include <gauche/class.h>
#define CHUNK 4096

#define DEFAULT_BUFFER_SIZE 4096
#define MINIMUM_BUFFER_SIZE 1024

/*================================================================
 * Class stuff
 */

static ScmClass *port_cpl[] = {
    SCM_CLASS_STATIC_PTR(Scm_PortClass),
    SCM_CLASS_STATIC_PTR(Scm_TopClass),
    NULL
};

SCM_DEFINE_BASE_CLASS(Scm_DeflatePortClass,
                      ScmPort, /* instance type */
                      NULL, NULL, NULL, NULL, port_cpl);

SCM_DEFINE_BASE_CLASS(Scm_InflatePortClass,
                      ScmPort, /* instance type */
                      NULL, NULL, NULL, NULL, port_cpl);

/*================================================================
 * Conditions
 */

static ScmClass *zlib_error_cpl[] = {
    SCM_CLASS_STATIC_PTR(Scm_ZlibErrorClass),
    SCM_CLASS_STATIC_PTR(Scm_ErrorClass),
    SCM_CLASS_STATIC_PTR(Scm_MessageConditionClass),
    SCM_CLASS_STATIC_PTR(Scm_SeriousConditionClass),
    SCM_CLASS_STATIC_PTR(Scm_ConditionClass),
    SCM_CLASS_STATIC_PTR(Scm_TopClass),
    NULL
};

static ScmObj zliberror_allocate(ScmClass *klass, ScmObj initargs);
static void message_print(ScmObj obj, ScmPort *port, ScmWriteContext *ctx);

SCM_DEFINE_BASE_CLASS(Scm_ZlibErrorClass, ScmZlibError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl+1);
SCM_DEFINE_BASE_CLASS(Scm_ZlibNeedDictErrorClass, ScmZlibNeedDictError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl);
SCM_DEFINE_BASE_CLASS(Scm_ZlibStreamErrorClass, ScmZlibStreamError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl);
SCM_DEFINE_BASE_CLASS(Scm_ZlibDataErrorClass, ScmZlibDataError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl);
SCM_DEFINE_BASE_CLASS(Scm_ZlibMemoryErrorClass, ScmZlibMemoryError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl);
SCM_DEFINE_BASE_CLASS(Scm_ZlibVersionErrorClass, ScmZlibVersionError,
                      message_print, NULL, NULL,
                      zliberror_allocate, zlib_error_cpl);

static ScmObj zliberror_allocate(ScmClass *klass, ScmObj initargs)
{
    ScmZlibError *e = SCM_ALLOCATE(ScmZlibError, klass);
    SCM_SET_CLASS(e, klass);
    e->message = SCM_FALSE;
    return SCM_OBJ(e);
}

static void message_print(ScmObj obj, ScmPort *port, ScmWriteContext *ctx)
{
    ScmClass *k = Scm_ClassOf(obj);
    Scm_Printf(port, "#<%A \"%30.1A\">",
               Scm__InternalClassName(k),
               SCM_ERROR_MESSAGE(obj));
}

static ScmClassStaticSlotSpec zliberror_slots[] = {
    { NULL }
};

ScmObj Scm_MakeZlibError(ScmObj message, int error_code)
{
    ScmClass *klass;
    switch (error_code) {
    case Z_NEED_DICT:
        klass = SCM_CLASS_ZLIB_NEED_DICT_ERROR;
        break;
    case Z_DATA_ERROR:
        klass = SCM_CLASS_ZLIB_DATA_ERROR;
        break;
    case Z_STREAM_ERROR:
        klass = SCM_CLASS_ZLIB_STREAM_ERROR;
        break;
    case Z_MEM_ERROR:
        klass = SCM_CLASS_ZLIB_MEMORY_ERROR;
        break;
    case Z_VERSION_ERROR:
        klass = SCM_CLASS_ZLIB_VERSION_ERROR;
        break;
    default:
        fprintf(stderr, "error_code: %d\n", error_code);
        Scm_Error("Scm_MakeZlibError called with unknown error code (%d).  "
                  "Implementation error?",
                  error_code);
    }

    ScmZlibError *e = SCM_ZLIB_ERROR(zliberror_allocate(klass, SCM_NIL));
    e->message = message;
    return SCM_OBJ(e);
}

void Scm_ZlibError(int error_code, const char *msg, ...)
{
    ScmObj e;
    va_list args;
    ScmVM *vm = Scm_VM();

    SCM_UNWIND_PROTECT {
        ScmObj ostr = Scm_MakeOutputStringPort(TRUE);
        va_start(args, msg);
        Scm_Vprintf(SCM_PORT(ostr), msg, args, TRUE);
        va_end(args);
        e = Scm_MakeZlibError(Scm_GetOutputString(SCM_PORT(ostr), 0),
                              error_code);
    }
    SCM_WHEN_ERROR {
        /* TODO: should check continuation */
        e = Scm_MakeError(SCM_MAKE_STR("Error occurred in error handler"));
    }
    SCM_END_PROTECT;
    Scm_VMThrowException(vm, e);
    Scm_Panic("Scm_Error: Scm_VMThrowException returned.  something wrong.");
}

static ScmObj porterror_allocate(ScmClass *klass, ScmObj initargs)
{
    ScmPortError *e = SCM_ALLOCATE(ScmPortError, klass);
    SCM_SET_CLASS(e, klass);
    e->common.message = SCM_FALSE; /* set by initialize */
    e->port = NULL;                /* set by initialize */
    return SCM_OBJ(e);
}

void Scm_ZlibPortError(ScmPort *port, int error_code, const char *msg, ...)
{
    ScmObj pe, e, smsg;
    va_list args;
    ScmVM *vm = Scm_VM();

    SCM_UNWIND_PROTECT {
        ScmObj ostr = Scm_MakeOutputStringPort(TRUE);
        va_start(args, msg);
        Scm_Vprintf(SCM_PORT(ostr), msg, args, TRUE);
        va_end(args);
        smsg = Scm_GetOutputString(SCM_PORT(ostr), 0);
        pe = porterror_allocate(SCM_CLASS_IO_READ_ERROR, SCM_NIL);
        SCM_ERROR(pe)->message = smsg;
        SCM_PORT_ERROR(pe)->port = port;
        e = Scm_MakeCompoundCondition(SCM_LIST2(Scm_MakeZlibError(smsg, error_code),
                                                pe));
    }
    SCM_WHEN_ERROR {
        /* TODO: should check continuation */
        e = Scm_MakeError(SCM_MAKE_STR("Error occurred in error handler"));
    }
    SCM_END_PROTECT;
    Scm_VMThrowException(vm, e);
    Scm_Panic("Scm_Error: Scm_VMThrowException returned.  something wrong.");
}

/*================================================================
 * Common
 */

static int fix_buffer_size(int siz)
{
    if (siz <= 0) return DEFAULT_BUFFER_SIZE;
    if (siz <= MINIMUM_BUFFER_SIZE) return MINIMUM_BUFFER_SIZE;
    return siz;
}

static ScmObj port_name(const char *type, ScmPort *source)
{
    ScmObj out = Scm_MakeOutputStringPort(TRUE);
    Scm_Printf(SCM_PORT(out), "[%s %A]",
               type, Scm_PortName(source));
    return Scm_GetOutputStringUnsafe(SCM_PORT(out), 0);
}

/*================================================================
 * Deflate port
 */
static int deflate_flusher(ScmPort *port, int cnt, int forcep)
{
    ScmZlibInfo *info = SCM_PORT_ZLIB_INFO(port);
    z_streamp strm = SCM_PORT_ZSTREAM(port);
    int ret, nread, nwrite, total = 0;
    unsigned char *inbuf = (unsigned char*)port->src.buf.buffer;
    unsigned char outbuf[CHUNK];

    strm->next_in = inbuf;
    strm->avail_in = SCM_PORT_BUFFER_AVAIL(port);

    if (info->flush == Z_NO_FLUSH && forcep) {
        info->flush == Z_SYNC_FLUSH;
    }

    for (;;) {
        strm->next_out = outbuf;
        strm->avail_out = CHUNK;
        ret = deflate(strm, info->flush);
        SCM_ASSERT(ret == Z_OK);
        if (strm->avail_out != 0) {
            info->flush = Z_NO_FLUSH;
        }
        nread = strm->next_in - inbuf;
        nwrite = strm->next_out - outbuf;
        total += nread;
        if (nwrite > 0) {
            Scm_Putz((char*)outbuf, nwrite, info->remote);
        }
        if (forcep && (total < cnt)) {
            continue;
        }
        return total;
    }
}

static void deflate_closer(ScmPort *port)
{
    ScmZlibInfo *info = SCM_PORT_ZLIB_INFO(port);
    z_streamp strm = SCM_PORT_ZSTREAM(port);
    int r, nwrite;
    unsigned char *inbuf = (unsigned char*)port->src.buf.buffer;
    unsigned char outbuf[CHUNK];

    strm->next_in = inbuf;
    strm->avail_in = SCM_PORT_BUFFER_AVAIL(port);
    strm->next_out = outbuf;
    strm->avail_out = CHUNK;

    do {
        r = deflate(strm, Z_FINISH);
        SCM_ASSERT(r == Z_OK || r == Z_STREAM_END);
        nwrite = strm->next_out - outbuf;
        if (nwrite > 0) {
            Scm_Putz((char*)outbuf, nwrite, info->remote);
            strm->next_out = outbuf;
            strm->avail_out = CHUNK;
        }
    } while (r != Z_STREAM_END);
    r = deflateEnd(strm);
    if (r != Z_OK) {
        Scm_ZlibError(r, "deflateEnd failed: %s", strm->msg);
    }
    Scm_Flush(info->remote);
    if (info->ownerp) {
        Scm_ClosePort(info->remote);
    }
}

static int zlib_fileno(ScmPort *port)
{
    return Scm_PortFileNo(SCM_PORT_ZLIB_INFO(port)->remote);
}

ScmObj Scm_MakeDeflatePort(ScmPort *source, int level,
                           int window_bits, int memlevel,
                           int strategy, ScmObj dict,
                           int bufsiz, int ownerp)
{
    ScmPortBuffer bufrec;
    ScmObj name = port_name("deflate", source);
    ScmZlibInfo *info = SCM_NEW(ScmZlibInfo);
    z_streamp strm = SCM_NEW_ATOMIC2(z_streamp, sizeof(z_stream));
    int r;

    bufsiz = fix_buffer_size(bufsiz);

    strm->zalloc = NULL;
    strm->zfree = NULL;
    strm->opaque = NULL;
    strm->next_in = NULL;
    strm->avail_in = 0;
    r = deflateInit2(strm, level, Z_DEFLATED, window_bits,
                     memlevel, strategy);
    if (r != Z_OK) {
        Scm_ZlibError(r, "deflateInit2 error: %s", strm->msg);
    }

    if (!SCM_FALSEP(dict)) {
        if (!SCM_STRINGP(dict))
            Scm_Error("String required, but got %S", dict);
        r = deflateSetDictionary(strm,
                                 (unsigned char*)SCM_STRING_START(dict),
                                 SCM_STRING_SIZE(dict));
        if (r != Z_OK) {
            Scm_ZlibError(r, "deflateSetDictionary failed: %s", strm->msg);
        }
        info->dict_adler = Scm_MakeIntegerU(strm->adler);
    } else {
        info->dict_adler = SCM_FALSE;
    }

    info->strm = strm;
    info->remote = source;
    info->bufsiz = 0;
    info->buf = NULL;
    info->ptr = NULL;
    info->ownerp = ownerp;
    info->flush = Z_NO_FLUSH;
    info->stream_endp = FALSE;
    info->level = level;
    info->strategy = strategy;

    memset(&bufrec, 0, sizeof(bufrec));
    bufrec.size = bufsiz;
    bufrec.buffer = SCM_NEW_ATOMIC2(char *, bufsiz);
    bufrec.mode = SCM_PORT_BUFFER_FULL;
    bufrec.filler = NULL;
    bufrec.flusher = deflate_flusher;
    bufrec.closer = deflate_closer;
    bufrec.ready = NULL;
    bufrec.filenum = zlib_fileno;
    bufrec.data = (void*)info;

    return Scm_MakeBufferedPort(SCM_CLASS_DEFLATE_PORT, name, SCM_PORT_OUTPUT, TRUE, &bufrec);
}

/*================================================================
 * Inflate port
 */

static int inflate_filler(ScmPort *port, int mincnt)
{
    ScmZlibInfo *info = SCM_PORT_ZLIB_INFO(port);
    z_streamp strm = SCM_PORT_ZSTREAM(port);
    int r, nread;
    unsigned char *outbuf = (unsigned char*)port->src.buf.end;

    if (info->stream_endp) return 0;

    nread = Scm_Getz(info->ptr,
                     info->bufsiz - (info->ptr - info->buf),
                     info->remote);

    if (nread <= 0) {
        /* input reached EOF */
        if (info->ptr == info->buf) {
            info->stream_endp = TRUE;
            return 0;
        }
        strm->avail_in = info->ptr - info->buf;
    } else {
        strm->avail_in = (info->ptr + nread) - info->buf;
    }

    strm->next_in = (unsigned char*)info->buf;
    strm->next_out = outbuf;
    strm->avail_out = SCM_PORT_BUFFER_ROOM(port);

  redo:
    r = inflate(strm, Z_SYNC_FLUSH);
    if (strm->avail_in > 0) {
        memmove(info->buf, strm->next_in, strm->avail_in);
        info->ptr = info->buf + strm->avail_in;
    } else {
        info->ptr = info->buf;
    }
    strm->next_in = (unsigned char*)info->buf;
    SCM_ASSERT(r != Z_STREAM_ERROR);
    switch (r) {
    case Z_NEED_DICT:
        if (info->dict == NULL) {
            Scm_ZlibPortError(info->remote, r, "dictionary required");
        }
        r = inflateSetDictionary(strm, info->dict, info->dictlen);
        if (r != Z_OK) {
            Scm_ZlibError(r, "inflateSetDictionary error: %s", strm->msg);
        }
        info->dict_adler = Scm_MakeIntegerU(strm->adler);
        if (strm->avail_in > 0) {
            goto redo;
        } else {
            goto end;
        }
    case Z_STREAM_END:
        info->stream_endp = TRUE;
        goto end;
    case Z_OK:
        goto end;
    case Z_DATA_ERROR:
        if (strm->next_out - outbuf > 0) {
            goto end;
        }
        /* fallthru */
    default:
        Scm_ZlibPortError(info->remote, r, "inflate error: %s", strm->msg);
    }
  end:
    return strm->next_out - outbuf;
}

static void inflate_closer(ScmPort *port)
{
    ScmZlibInfo *info = SCM_PORT_ZLIB_INFO(port);
    z_streamp strm = SCM_PORT_ZSTREAM(port);
    int r = inflateEnd(strm);
    if (r != Z_OK) {
        Scm_ZlibError(r, "inflateEnd failed: %s", strm->msg);
    }
    if (info->ownerp) {
        Scm_ClosePort(info->remote);
    }
}

static int inflate_ready(ScmPort *port)
{
    return 0;
}

ScmObj Scm_MakeInflatePort(ScmPort *sink, int bufsiz,
                           int window_bits, ScmObj dict,
                           int ownerp)
{
    ScmPortBuffer bufrec;
    ScmObj name = port_name("inflate", sink);
    ScmZlibInfo *info = SCM_NEW(ScmZlibInfo);
    z_streamp strm = SCM_NEW_ATOMIC2(z_streamp, sizeof(z_stream));
    int r;

    bufsiz = fix_buffer_size(bufsiz);

    strm->zalloc = NULL;
    strm->zfree = NULL;
    strm->opaque = NULL;
    strm->next_in = NULL;
    strm->avail_in = 0;
    r = inflateInit2(strm, window_bits);
    if (r != Z_OK)
        Scm_ZlibError(r, "inflateInit2 error: %s", strm->msg);

    if (!SCM_FALSEP(dict)) {
        if (!SCM_STRINGP(dict)) {
            Scm_Error("String required, but got %S", dict);
        }
        info->dict = (unsigned char*)SCM_STRING_START(dict);
        info->dictlen = SCM_STRING_SIZE(dict);
    } else {
        info->dict = NULL;
        info->dictlen = 0;
    }

    info->strm = strm;
    info->remote = sink;
    info->bufsiz = CHUNK;
    info->buf = SCM_NEW_ATOMIC2(char *, CHUNK);
    info->ptr = info->buf;
    info->ownerp = ownerp;
    info->stream_endp = FALSE;
    info->level = 0;
    info->strategy = 0;
    info->dict_adler = SCM_FALSE;

    memset(&bufrec, 0, sizeof(bufrec));
    bufrec.size = info->bufsiz;
    bufrec.buffer = SCM_NEW_ATOMIC2(char *, info->bufsiz);
    bufrec.mode = SCM_PORT_BUFFER_FULL;
    bufrec.filler = inflate_filler;
    bufrec.flusher = NULL;
    bufrec.closer = inflate_closer;
    bufrec.ready = inflate_ready;
    bufrec.filenum = zlib_fileno;
    bufrec.data = (void*)info;

    return Scm_MakeBufferedPort(SCM_CLASS_INFLATE_PORT, name, SCM_PORT_INPUT, TRUE, &bufrec);
}

ScmObj Scm_InflateSync(ScmPort *port)
{
    ScmZlibInfo *info = SCM_PORT_ZLIB_INFO(port);
    z_streamp strm = SCM_PORT_ZSTREAM(port);
    unsigned char *outbuf = (unsigned char*)port->src.buf.end;
    unsigned long curr_in = strm->total_in;
    int r, nread;

    if (info->stream_endp) return SCM_FALSE;

    do {
        nread = Scm_Getz(info->ptr,
                         info->bufsiz - (info->ptr - info->buf),
                         info->remote);
        if (nread <= 0) {
            /* input reached EOF */
            if (info->ptr == info->buf) {
                info->stream_endp = TRUE;
                return SCM_FALSE;
            }
            strm->avail_in = info->ptr - info->buf;
        } else {
            strm->avail_in = (info->ptr + nread) - info->buf;
        }

        strm->next_in = (unsigned char*)info->buf;
        strm->next_out = outbuf;
        strm->avail_out = SCM_PORT_BUFFER_ROOM(port);

        r = inflateSync(strm);
        SCM_ASSERT(r != Z_STREAM_ERROR);
        if (strm->avail_in > 0) {
            memmove(info->buf, strm->next_in, strm->avail_in);
            info->ptr = info->buf + strm->avail_in;
        } else {
            info->ptr = info->buf;
        }
        strm->next_in = (unsigned char*)info->buf;
    } while (r != Z_OK);
    return Scm_MakeIntegerU(strm->total_in - curr_in);
}

/*
 * Module initialization function.
 */
extern void Scm_Init_zliblib(ScmModule*);

ScmObj Scm_Init_zlib(void)
{
    ScmModule *mod;
    ScmClass *cond_meta;

    /* Register this DSO to Gauche */
    SCM_INIT_EXTENSION(zlib);

    /* Create the module if it doesn't exist yet. */
    mod = SCM_MODULE(SCM_FIND_MODULE("rfc.zlib", TRUE));

    Scm_InitStaticClass(&Scm_DeflatePortClass, "<deflate-port>",
                        mod, NULL, 0);
    Scm_InitStaticClass(&Scm_InflatePortClass, "<inflate-port>",
                        mod, NULL, 0);

    cond_meta = Scm_ClassOf(SCM_OBJ(SCM_CLASS_CONDITION));
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_ERROR,
                                "<zlib-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_NEED_DICT_ERROR,
                                "<zlib-need-dict-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_STREAM_ERROR,
                                "<zlib-stream-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_DATA_ERROR,
                                "<zlib-data-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_MEMORY_ERROR,
                                "<zlib-memory-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);
    Scm_InitStaticClassWithMeta(SCM_CLASS_ZLIB_VERSION_ERROR,
                                "<zlib-version-error>",
                                mod, cond_meta, SCM_FALSE,
                                zliberror_slots, 0);

    /* Register stub-generated procedures */
    Scm_Init_zliblib(mod);
}