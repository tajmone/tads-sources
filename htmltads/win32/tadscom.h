/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadscom.h - COM helper routines
Function

Notes

Modified
  10/09/06 MJRoberts  - Creation
*/

#ifndef TADSCOM_H
#define TADSCOM_H

#include <Windows.h>
#include <oleauto.h>
#include <wtypes.h>
#include <mshtmhst.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "tadshtml.h"


/* ------------------------------------------------------------------------ */
/*
 *   BSTR and OLESTR helpers
 */

/*
 *   Allocate a BSTR from an ANSI string.
 */
BSTR bstr_from_ansi(const textchar_t *str, size_t len);

/*
 *   Allocate a BSTR from a null-terminated ANSI string.
 */
BSTR bstr_from_ansi(const textchar_t *str);

/*
 *   Convert a BSTR into an ANSI string buffer, copying the ANSI characters
 *   into a buffer.
 */
textchar_t *ansi_from_bstr(
    textchar_t *dst, size_t dstsize, BSTR bstr, int free_bstr);

/*
 *   Allocate an ANSI string buffer from a BSTR
 */
textchar_t *ansi_from_bstr(BSTR bstr, int free_bstr);

/*
 *   Convert an OLESTR string to an ANSI string
 */
textchar_t *ansi_from_olestr(
    textchar_t *dst, size_t dstsize, const OLECHAR *olestr, size_t olecnt);
inline textchar_t *ansi_from_olestr(
    textchar_t *dst, size_t dstsize, const OLECHAR *olestr)
{
    return ansi_from_olestr(dst, dstsize, olestr, wcslen(olestr));
}

/*
 *   Allocate an ANSI string buffer from an OLESTR
 */
inline textchar_t *ansi_from_olestr(const OLECHAR *olestr, size_t olecnt);
inline textchar_t *ansi_from_olestr(const OLECHAR *olestr)
{
    return ansi_from_olestr(olestr, wcslen(olestr));
}

/* ------------------------------------------------------------------------ */
/*
 *   Simplified IDispatch class.
 */
class TadsDispatch;
struct TadsDispatchMap
{
    const wchar_t *name;
    void (TadsDispatch::*func)(DISPPARAMS *params, VARIANT *retp);
};

class TadsDispatch: public IDispatch
{
public:
    /*
     *   Retrieve a boolean argument, if presetn
     */
    static int get_bool_arg(DISPPARAMS *args, unsigned int argn)
    {
        /* if we have the requested argument, retrieve it */
        if (argn < args->cArgs)
        {
            /* convert the parameter index to an array index */
            argn = args->cArgs - argn - 1;

            /* coerce to boolean */
            VARIANTARG dst;
            dst.vt = VT_EMPTY;
            if (SUCCEEDED(VariantChangeType(
                &dst, &args->rgvarg[argn], 0, VT_BOOL)))
                return dst.boolVal;
        }

        /*
         *   the argument is missing or isn't convertible to boolean; to be
         *   consistent with Javascript, treat a missing argument as false
         */
        return FALSE;
    }

    /*
     *   Retrieve a String argument, if present.  This returns a simple
     *   null-terminated C-style string buffer that the caller is responsible
     *   for freeing with th_free().
     */
    static char *get_str_arg(DISPPARAMS *args, unsigned int argn)
    {
        /* if we have the requested argument, retrieve it */
        if (argn < args->cArgs)
        {
            /*
             *   convert the argument index to an array index, and retrieve
             *   the string value
             */
            return get_str_arg(&args->rgvarg[args->cArgs - argn - 1]);
        }
        else
        {
            /* argument index out of bounds - return a null string */
            return 0;
        }
    }

    static char *get_str_arg(VARIANTARG *arg)
    {
        /* check the type of the argument */
        switch (arg->vt)
        {
        case VT_BSTR:
            /* it's a BSTR - convert to ANSI */
            return ansi_from_bstr(arg->bstrVal, FALSE);

        case VT_BSTR | VT_BYREF:
            return ansi_from_bstr(*arg->pbstrVal, FALSE);

        default:
            /* we don't convert other types; just return a null string */
            return 0;
        }
    }

    /*
     *   Retrieve an IDispatch argument, if present
     */
    static IDispatch *get_disp_arg(DISPPARAMS *args, unsigned int argn)
    {
        /* if we have the requested argument, retrieve it */
        if (argn < args->cArgs)
        {
            /*
             *   convert the parameter index to an array index, and get the
             *   argument, coerced to an IDispatch
             */
            return get_disp_arg(&args->rgvarg[args->cArgs - argn - 1]);
        }
        else
        {
            /*
             *   the argument is missing; to be consistent with Javascript,
             *   treat a missing argument as null
             */
            return 0;
        }
    }

    /*
     *   Retrieve an IDispatch argument
     */
    static IDispatch *get_disp_arg(VARIANTARG *arg)
    {
        /* we don't have the IDispatch yet */
        IDispatch *dp = 0;

        /* check the type of the argument */
        switch (arg->vt)
        {
        case VT_DISPATCH:
            /* it's provided directly as an IDispatch */
            dp = arg->pdispVal;

            /* keep a reference */
            if (dp != 0)
                dp->AddRef();

            /* return it */
            return dp;

        case VT_UNKNOWN:
            /* it's an IUnknown, which *could* be an IDispatch as well... */
            {
                /* ask the IUnknown for an IDispatch interface */
                IUnknown *up = arg->punkVal;
                if (up != 0
                    && SUCCEEDED(up->QueryInterface(
                        IID_IDispatch, (void **)&dp)))
                {
                    /* got it */
                    return dp;
                }
            }

            /* it's not an IDispatch */
            return 0;

        case VT_NULL:
        default:
            /* treat this as a null IDispatch */
            return 0;
        }
    }

    /*
     *   get a DWORD argument
     */
    static DWORD get_dword_arg(DISPPARAMS *args, unsigned int argn)
    {
        /* if we have the requested argument, retrieve it */
        if (argn < args->cArgs)
        {
            /*
             *   convert the argument index to an array index, and retrieve
             *   the DWORD value
             */
            return get_dword_arg(&args->rgvarg[args->cArgs - argn - 1]);
        }
        else
        {
            /* argument index out of bounds - return zero as the default */
            return 0;
        }
    }

    static DWORD get_dword_arg(VARIANTARG *arg)
    {
        /* coerce to DWORD */
        VARIANTARG dst;
        dst.vt = VT_EMPTY;
        if (SUCCEEDED(VariantChangeType(&dst, arg, 0, VT_UI4)))
            return dst.ulVal;

        /* not convertible to DWORD - return zero as the default */
        return 0;
    }

    /*
     *   get a Long argument
     */
    static DWORD get_long_arg(DISPPARAMS *args, unsigned int argn)
    {
        /* if we have the requested argument, retrieve it */
        if (argn < args->cArgs)
        {
            /*
             *   convert the argument index to an array index, and retrieve
             *   the Long value
             */
            return get_long_arg(&args->rgvarg[args->cArgs - argn - 1]);
        }
        else
        {
            /* argument index out of bounds - return zero as the default */
            return 0;
        }
    }

    static DWORD get_long_arg(VARIANTARG *arg)
    {
        /* coerce to Long */
        VARIANTARG dst;
        dst.vt = VT_EMPTY;
        if (SUCCEEDED(VariantChangeType(&dst, arg, 0, VT_I4)))
            return dst.lVal;

        /* not convertible to Long - return zero as the default */
        return 0;
    }

    /* call an IDispatch function */
    static int call_disp(IDispatch *disp, wchar_t *funcname,
                         int argc, VARIANT *argv, VARIANT *retp)
    {
        /* if there's no IDispatch, we can't proceed */
        if (disp == 0)
            return FALSE;

        /* set up the locale */
        LCID lcid = MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             SORT_DEFAULT);

        /* look up the function */
        DISPID funcid;
        if (SUCCEEDED(disp->GetIDsOfNames(
            IID_NULL, &funcname, 1, lcid, &funcid)))
        {
            /* set up the DISPPARAMS */
            DISPPARAMS params;
            params.rgvarg = argv;
            params.cArgs = argc;
            params.cNamedArgs = 0;

            /* call the function */
            EXCEPINFO excep;
            unsigned int argErr;
            if (SUCCEEDED(disp->Invoke(funcid, IID_NULL, lcid, DISPATCH_METHOD,
                                       &params, retp, &excep, &argErr)))
                return TRUE;
        }

        /* something went wrong in the invoke */
        return FALSE;
    }

    TadsDispatch()
    {
        /* set up one reference on behalf of the creator */
        refcnt_ = 1;

        /* we haven't figured the map size yet */
        map_cnt_ = 0;
    }

    virtual ~TadsDispatch()
    {
    }

    /* standard OLE reference counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret = --refcnt_;
        if (refcnt_ == 0)
            delete this;
        return ret;
    }

    /* query interfaces: we provide IUknown and IDispatch */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_IDispatch)
            *ifc = (void *)(IDispatch *)this;
        else
            return E_NOINTERFACE;

        AddRef();
        return S_OK;
    }

    /* we have no type library information */
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(unsigned int *pctinfo)
    {
        return E_NOTIMPL;
    }

    /* we have no type library information */
    HRESULT STDMETHODCALLTYPE GetTypeInfo(
        unsigned int idx, LCID lcid, ITypeInfo **info)
    {
        return E_NOTIMPL;
    }

    /* get IDs */
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        REFIID riid, OLECHAR **names, unsigned int cnt,
        LCID lcid, DISPID *dispid)
    {
        /*
         *   we only accept zero-parameter names, since we're talking to
         *   javascript, which doesn't do type signatures for naming
         */
        if (cnt != 1)
            return DISP_E_UNKNOWNNAME;

        /*
         *   look up the name in our table; note that we start at element 1,
         *   since the first is always the "default" value entry, which has
         *   no name
         */
        const TadsDispatchMap *map = get_map();
        int map_cnt = get_map_cnt();
        for (int i = 1 ; i < map_cnt ; ++i)
        {
            /* check for a match to this name */
            if (wcscmp(map[i].name, names[0]) == 0)
            {
                *dispid = i;
                return S_OK;
            }
        }

        /* didn't find it */
        return DISP_E_UNKNOWNNAME;
    }

    /* get the default value (disp interface #0) */
    virtual void default_value(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_I4;
        ret->lVal = 0;
    }

    HRESULT STDMETHODCALLTYPE Invoke(
        DISPID member, REFIID riid, LCID lcid, WORD flags,
        DISPPARAMS *params, VARIANT *retp, EXCEPINFO *excep,
        unsigned int *argErr)
    {
        /* find the member in the table, if it's in range */
        if (member > 0 && member < get_map_cnt())
        {
            /* set up an empty return value */
            VARIANT ret;
            VariantInit(&ret);

            /* invoke the member from the dispatch table */
            (this->*get_map()[member].func)(params, &ret);

            /* if the caller wants the return value, copy it out */
            if (retp != 0)
                VariantCopy(retp, &ret);

            /* success */
            return S_OK;
        }
        else
        {
            /* unknown interface */
            return DISP_E_MEMBERNOTFOUND;
        }
    }

    virtual const TadsDispatchMap *get_map() = 0;
    int get_map_cnt()
    {
        /* cache the map count if we haven't done so already */
        if (map_cnt_ == 0)
        {
            for (const TadsDispatchMap *map = get_map() ;
                 map[map_cnt_].name != 0 ;
                 ++map_cnt_) ;
        }

        /* return the count */
        return map_cnt_;
    }

    int map_cnt_;
    ULONG refcnt_;
};

/* declare the map - do INSIDE the class */
#define TADSDISP_DECL_MAP \
    virtual const TadsDispatchMap *get_map() { return __map; } \
    static const TadsDispatchMap __map[]

/* define the map - do OUTSIDE of the class */
#define TADSDISP_MAP_BEGIN(cls) \
    const TadsDispatchMap cls::__map[] = { \
    { L"<default>", &default_value },
#define TADSDISP_MAP(cls, name) \
    { L#name, (void (TadsDispatch::*)(DISPPARAMS *, VARIANT *))&cls::name },
#define TADSDISP_MAP_END \
    { 0, 0 } \
    }


/* ------------------------------------------------------------------------ */
/*
 *   Base COM object class.  This manages the COM reference counting.
 */
class CBaseOle
{
public:
    CBaseOle()
    {
        /*
         *   construction automatically gives us one initial reference, from
         *   the caller who constructed us
         */
        refcnt_ = 1;
    }

    virtual ~CBaseOle() { }

protected:
    ULONG refcnt_;
};

/*
 *   Define the base IUnknown methods for an object.  This defines the
 *   AddRef/ReleaseRef methods using the reference count from the CBaseOle
 *   base class.
 */
#define BaseIUnknown \
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; } \
ULONG STDMETHODCALLTYPE Release() \
{ \
    ULONG ret = --refcnt_; \
    if (ret == 0) \
        delete this; \
    return ret; \
}


/* ------------------------------------------------------------------------ */
/*
 *   Dummy IStorage interface.  This is useful for certain system classes
 *   that require an IStorage object to be provided, but don't actually use
 *   it for anything.  For example, this is needed for in-place objects, such
 *   as embedded browser objects.  We merely define most of these methods as
 *   'not implemented'.
 */
class CDummyStorage: public CBaseOle, public IStorage
{
public:
    BaseIUnknown;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        *obj = 0;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CreateStream(
        const WCHAR *pwcsName, DWORD grfMode,
        DWORD reserved1, DWORD reserved2, IStream **ppstm)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE OpenStream(
        const WCHAR * pwcsName, void *reserved1, DWORD grfMode,
        DWORD reserved2, IStream **ppstm)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE CreateStorage(
        const WCHAR *pwcsName, DWORD grfMode,
        DWORD reserved1, DWORD reserved2, IStorage **ppstg)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE OpenStorage(
        const WCHAR *pwcsName, IStorage *pstgPriority, DWORD grfMode,
        SNB snbExclude, DWORD reserved, IStorage **ppstg)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE CopyTo(
        DWORD ciidExclude, IID const *rgiidExclude, SNB snbExclude,
        IStorage *pstgDest)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE MoveElementTo(
        const OLECHAR *pwcsName, IStorage *pstgDest,
        const OLECHAR *pwcsNewName, DWORD grfFlags)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE Revert()
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE EnumElements(
        DWORD reserved1, void *reserved2, DWORD reserved3,
        IEnumSTATSTG **ppenum)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE DestroyElement(
        const OLECHAR *pwcsName)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE RenameElement(
        const WCHAR *pwcsOldName, const WCHAR *pwcsNewName)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetElementTimes(
        const WCHAR *pwcsName, FILETIME const *pctime,
        FILETIME const *patime, FILETIME const *pmtime)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetClass(REFCLSID clsid)
        { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetStateBits(DWORD grfStateBits, DWORD grfMask)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE Stat(STATSTG * pstatstg, DWORD grfStatFlag)
        { return E_NOTIMPL; }
};



#endif /* TADSCOM_H */
