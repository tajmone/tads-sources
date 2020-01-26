/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadswebctl.h - web control embedding infrastructure
Function
  Defines the COM/OLE infrastructure for embedding a browser control in
  an application window.
Notes

Modified
  05/12/10 MJRoberts  - Creation
*/

#ifndef TADSWEBCTL_H
#define TADSWEBCTL_H

#include <Windows.h>
#include <exdisp.h>
#include <oleauto.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <MsHtmcid.h>

#include "tadscom.h"
#include "tadsapp.h"

/* ------------------------------------------------------------------------ */
/*
 *   some newer SDK elements we need
 */
#ifndef DISPID_NEWWINDOW3
#define DISPID_NEWWINDOW3  273
#endif
#ifndef DISPID_WINDOWSTATECHANGED
#define DISPID_WINDOWSTATECHANGED 283
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Base class for windows that can embed a browser control
 */
class CBrowserFrame
{
    friend class CWebEventSink;

public:
    CBrowserFrame()
    {
        /* we don't have a browser or an embedding site yet */
        browser_ = 0;
        iwb2_ = 0;
        icmd_ = 0;
        storage_ = 0;
        client_site_ = 0;
        web_evt_ = 0;
        in_resize_ = 0;
    }

    virtual ~CBrowserFrame() { }

    /* embed the browser */
    int embed_browser(int show, HWND handle,
                      int context_menu_id, IDispatch *ext,
                      IOleCommandTarget *cmd_target);

    /* unembed the browser */
    void unembed_browser();

    /* create a new embedded browser control */
    virtual IWebBrowser2 *new_browser_ctl(
        HWND handle, int context_menu_id, IDispatch *ext,
        IOleCommandTarget *cmd_targ)
    {
        /* embed a new browser */
        embed_browser(TRUE, handle, context_menu_id, ext, cmd_targ);

        /* return the new control */
        return iwb2_;
    }

    /* get the browser control */
    IWebBrowser2 *get_browser_ctl()
    {
        if (iwb2_ != 0)
            iwb2_->AddRef();
        return iwb2_;
    }

    /* go to a URL */
    void go_to_url(const textchar_t *url);

    /*
     *   Set the browser control's size and position within the client area.
     *   Pass -1 to leave a current value unchanged.
     */
    void resize_browser_ctl(int left, int top, int width, int height)
    {
        if (iwb2_ != 0)
        {
            /* note that we're in a resize */
            ++in_resize_;

            /* put each changed dimension */
            if (left != -1)
                iwb2_->put_Left(left);
            if (top != -1)
                iwb2_->put_Top(top);
            if (width != -1)
                iwb2_->put_Width(width);
            if (height != -1)
                iwb2_->put_Height(height);
        }

        /* done with the resize */
        --in_resize_;
    }

    /* receive notification of navigating to a new URL */
    virtual void on_url_change(const textchar_t *url) = 0;

    /* receive notification that a document has finished loading */
    virtual void on_doc_done(const textchar_t *url) = 0;

    /* receive notification of the document title */
    virtual void note_new_title(const textchar_t *title) = 0;

    /* note new status line text from the control */
    virtual void note_new_status_text(const textchar_t *txt) = 0;

    /* open a new window, in response to a request from the web page */
    virtual void open_new_window(
        VARIANT_BOOL *cancel, IDispatch **disp,
        const char *url, const char *src_url, enum NWMF flags) = 0;

    /*
     *   Adjust window dimensions for decorations within the client area
     *   (toolbar, statusbar).  This is called by the browser control to
     *   calculate the actual client area size needed to show the browser
     *   control at the given size.  On input, *width and *height give the
     *   desired dimensions for the browser control; on return, these values
     *   should reflect the actual client window size needed such that the
     *   browser control will be shown at the input sizes.
     */
    virtual void adjust_window_dimensions(long *width, long *height) { }

    /* hide/show the window, controlled from the browser */
    virtual void on_visible(int show) { }

    /* receive notification of a size change from the browser */
    virtual void browser_set_width(long width) { }
    virtual void browser_set_height(long height) { }


protected:
    /* our browser object handle */
    IOleObject *browser_;

    /* the browser's IWebBrowser2 control interface */
    IWebBrowser2 *iwb2_;

    /* the browser's IOleCommandTarget interface */
    IOleCommandTarget *icmd_;

    /*
     *   the host interface implementations we provide to the embedded
     *   browser object
     */
    class CDummyStorage *storage_;
    class CClientSite *client_site_;

    /* our web browser event sink */
    class CWebEventSink *web_evt_;

    /*
     *   flag: we're doing a resize, so ignore PropertyChange events related
     *   to resizing
     */
    int in_resize_;
};



/* ------------------------------------------------------------------------ */
/*
 *   Web browser event sink.   This receives events that the html control
 *   sends out to registered DWebBrowserEvents2 objects.
 */
class CWebEventSink: public CBaseOle, IDispatch
{
public:
    CWebEventSink(CBrowserFrame *win, IWebBrowser2 *iwb2)
    {
        /* remember my containing window */
        win_ = win;

        /* remember the web browser control interface */
        iwb2->AddRef();
        iwb2_ = iwb2;

        /* we haven't seen any NewWindow3 events yet */
        got_newwin3_ = FALSE;

        /* we're not yet registered */
        registered_ = FALSE;

        /* register */
        register_event_handler(TRUE);
    }

    ~CWebEventSink()
    {
        /* make sure we're unregistered */
        register_event_handler(FALSE);

        /* release our web browser interface */
        iwb2_->Release();
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* we're an IDispatch, and of course also IUnknown */
        if (iid == IID_IUnknown || iid == IID_IDispatch)
        {
            *obj = (IDispatch *)this;
            AddRef();
            return S_OK;
        }

        /* we don't provide any other interfaces */
        *obj = 0;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(unsigned int FAR *cnt)
    {
        *cnt = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(
        unsigned int itinfo, LCID lcid, ITypeInfo FAR *FAR *info)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        REFIID riid, OLECHAR FAR *FAR *rgszNames, UINT cNames,
        LCID lcid, DISPID FAR *rgDispId)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE Invoke(
        DISPID member, REFIID riid, LCID lcid, WORD flags,
        DISPPARAMS FAR *params, VARIANT FAR *result,
        EXCEPINFO FAR *exc, unsigned int FAR *arg_err)
    {
        /* check which member we've invoking */
        switch(member)
        {
        case DISPID_NEWWINDOW2:
            /*
             *   NewWindow2 is fired by older IE versions, before XP SP2.
             *   The documentation says that newer browsers (>=XP SP2) fire
             *   NewWindow3 "instead of" NewWindow2, but empirically this is
             *   false: they actually fire NewWindow3 *in addition to*
             *   NewWindow2.  If NewWindow3 doesn't do anything, the browser
             *   will follow it by firing NewWindow2; this is presumably a
             *   compatibility measure, but the flaw in MSFT's thinking here
             *   is that they didn't provide a way for NewWindow3 to say
             *   "give me the default" without having NewWindow2 also fire.
             *
             *   The way we'll deal with this is to flag when we see
             *   NewWindow3 fire, then check this flag when NewWindow2 fires.
             *   If we've seen a NewWindow3 event, we'll know this must be a
             *   newer browser, so the NewWindow2 is being fired only as a
             *   fallback and can be ignored.  If we've never seen a
             *   NewWindow3, we'll know this is an older browser, and that we
             *   must execute NewWindow2 instead.  The reason we prefer to do
             *   our work in NewWindow3 is that it gives us more information
             *   about the source of the event, specifically telling us if
             *   the event is a mouse click or a scripting event.  We
             *   sometimes want to handle the two differently.
             *
             *   Note that it's not important to pair *particular* NewWindow3
             *   and NewWindow2 events.  It's pretty clear that if the
             *   browser is a newer version that fires NewWindow3 at all, it
             *   will *always* fire NewWindow3 *first*, and then always fire
             *   NewWindow2 only if NewWindow3 doesn't either provide a
             *   custom window or cancel the event.  Older browsers *never*
             *   fire NewWindow3.  So we don't need to worry about the
             *   correspondence of this NewWindow2 to any particular past
             *   NewWindow3 - this is really just a browser version sensing
             *   flag, so once we've seen a single NewWindow3, we'll know
             *   we're on a newer browser and can ignore all NewWindow2's.
             */
            if (!got_newwin3_)
            {
                IDispatch *disp = 0;
                VARIANT_BOOL cancel = FALSE;

                /* let the frame do the navigation */
                win_->open_new_window(&cancel, &disp, 0, 0, (NWMF)0);

                /* set the 'cancel' flag */
                *params->rgvarg[0].pboolVal = cancel;

                /* return the new window interface */
                if (disp != 0)
                {
                    /* free any old dispatch pointer */
                    if (*params->rgvarg[1].ppdispVal != 0)
                        (*params->rgvarg[1].ppdispVal)->Release();

                    /* set the new value */
                    *params->rgvarg[1].ppdispVal = disp;
                }
            }
            break;

        case DISPID_NEWWINDOW3:
            {
                /*
                 *   flag that we've seen a NewWindow3 event, so that if we
                 *   get a subsequent NewWindow2, we'll know to ignore it
                 */
                got_newwin3_ = TRUE;

                /* presume we won't cancel or supply a custom window */
                IDispatch *disp = 0;
                VARIANT_BOOL cancel = FALSE;

                /* get the destination URL as an ASCII string */
                textchar_t *url = TadsDispatch::get_str_arg(params, 4);

                /* get the source ("context") URL */
                textchar_t *src_url = TadsDispatch::get_str_arg(params, 3);

                /* get the flags */
                NWMF flags = (NWMF)TadsDispatch::get_dword_arg(params, 2);

                /* ask the frame to open the window */
                win_->open_new_window(&cancel, &disp, url, src_url, flags);

                /* done with the url strings */
                th_free(url);
                th_free(src_url);

                /* set the 'cancel' flag */
                *params->rgvarg[3].pboolVal = cancel;

                /* return the new window interface */
                if (disp != 0)
                {
                    /* free any old dispatch value */
                    if (*params->rgvarg[4].ppdispVal != 0)
                        (*params->rgvarg[4].ppdispVal)->Release();

                    /* set the new value */
                    *params->rgvarg[4].ppdispVal = disp;
                }
            }
            break;

        case DISPID_QUIT:
            /* disconnect */
            register_event_handler(FALSE);
            break;

        case DISPID_NAVIGATECOMPLETE2:
            /* navigating to a new URL */
            {
                /* get the frame IDispatch */
                IDispatch *framedisp = TadsDispatch::get_disp_arg(
                    &params->rgvarg[1]);

                /* only pay attention to changes in the main frame */
                IWebBrowser2 *frame;
                if (framedisp != 0
                    && SUCCEEDED(framedisp->QueryInterface(
                        IID_IWebBrowser2, (void **)&frame)))
                {
                    /* if it's our main frame, note the new URL */
                    if (frame == iwb2_)
                    {
                        /* convert to ANSI */
                        textchar_t *urla = TadsDispatch::get_str_arg(
                            &params->rgvarg[0]);

                        /* note the change */
                        win_->on_url_change(urla);

                        /* done with the ANSI version */
                        th_free(urla);
                    }

                    /* done with the frame */
                    frame->Release();
                    framedisp->Release();
                }
            }
            break;

        case DISPID_DOCUMENTCOMPLETE:
            /* loaded a document */
            if (win_ != 0)
            {
                /* get args: documentComplete(url, frame) */
                textchar_t *urla = TadsDispatch::get_str_arg(
                    &params->rgvarg[0]);
                IDispatch *framedisp = TadsDispatch::get_disp_arg(
                    &params->rgvarg[1]);

                /* notify the window */
                win_->on_doc_done(urla);
                th_free(urla);

                /* get the browser frame */
                IWebBrowser2 *frame;
                if (framedisp != 0
                    && SUCCEEDED(framedisp->QueryInterface(
                        IID_IWebBrowser2, (void **)&frame)))
                {
                    /* if it's our frame, proceed */
                    if (frame == iwb2_)
                    {
                        /* get the document */
                        IDispatch *doc;
                        if (SUCCEEDED(frame->get_Document(&doc)))
                        {
                            /* get the doc2 interface */
                            IHTMLDocument2 *doc2;
                            if (SUCCEEDED(doc->QueryInterface(
                                IID_IHTMLDocument2, (void **)&doc2)))
                            {
                                /* get the title */
                                BSTR title;
                                if (SUCCEEDED(doc2->get_title(&title)))
                                {
                                    /* get the local character set version */
                                    textchar_t *atitle =
                                        ansi_from_bstr(title, TRUE);

                                    /* pass it to the frame */
                                    win_->note_new_title(atitle);

                                    /* free the ansi string */
                                    th_free(atitle);
                                }

                                /* done with the doc2 */
                                doc2->Release();
                            }

                            /* done with the document */
                            doc->Release();
                        }
                    }

                    /* done with the frame */
                    frame->Release();
                    framedisp->Release();
                }
            }
            break;

        case DISPID_TITLECHANGE:
            /* title changed */
            if (win_ != 0)
            {
                /* get the title, and convert to ANSI */
                textchar_t *atitle = TadsDispatch::get_str_arg(
                    &params->rgvarg[0]);

                /* notify the window */
                win_->note_new_title(atitle);

                /* free the ansi string */
                th_free(atitle);
            }
            break;

        case DISPID_STATUSTEXTCHANGE:
            /* status line text changed */
            if (win_ != 0)
            {
                /* get the new text, and convert to ANSI */
                textchar_t *atxt = TadsDispatch::get_str_arg(
                    &params->rgvarg[0]);

                /* notify the window */
                win_->note_new_status_text(atxt);

                /* done with the ansi string */
                th_free(atxt);
            }
            break;

        case DISPID_WINDOWSETWIDTH:
            /* set the window width from the browser control */
            win_->browser_set_width(
                (long)TadsDispatch::get_dword_arg(params, 0));
            break;

        case DISPID_WINDOWSETHEIGHT:
            /* set the height from the browser control */
            if (win_->in_resize_ == 0)
                win_->browser_set_height(
                    (long)TadsDispatch::get_dword_arg(params, 0));
            break;

        case DISPID_CLIENTTOHOSTWINDOW:
            /* adjust window dimensions */
            if (win_->in_resize_ == 0)
                win_->adjust_window_dimensions(
                    params->rgvarg[1].plVal, params->rgvarg[0].plVal);
            break;

        case DISPID_ONVISIBLE:
            win_->on_visible(params->rgvarg[0].boolVal);
            break;
        }

        /* handled */
        return S_OK;
    }

    /* register or unregister our web browser event handler */
    void register_event_handler(int reg)
    {
        IConnectionPointContainer *cpcont;

        /* if we're already in the desired state, ignore the request */
        if ((reg && registered_) || (!reg && !registered_))
            return;

        /* get the connection point container */
        if (SUCCEEDED(iwb2_->QueryInterface(
            IID_IConnectionPointContainer, (void **)&cpcont)))
        {
            IConnectionPoint *cp;

            /* get the connection point for web browser events */
            if (SUCCEEDED(cpcont->FindConnectionPoint(
                DIID_DWebBrowserEvents2, &cp)))
            {
                /* register or unregister, as indicated by the caller */
                if (reg)
                    cp->Advise((IDispatch *)this, &cookie_);
                else
                    cp->Unadvise(cookie_);

                /* remember the new registration status */
                registered_ = reg;

                /* release the connection point object */
                cp->Release();
            }

            /* release the connection point container object */
            cpcont->Release();
        }
    }

protected:
    /* my containing window */
    CBrowserFrame *win_;

    /* the browser control interface */
    IWebBrowser2 *iwb2_;

    /* the "cookie" our connection point uses to identify us */
    DWORD cookie_;

    /* are we registered? */
    int registered_;

    /* have we received a NewWindow3 event? */
    int got_newwin3_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Generic in-place frame
 */

/* the frame class */
class CInPlaceFrame: public CBaseOle, public IOleInPlaceFrame
{
public:
    CInPlaceFrame(HWND hwnd)
    {
        /* remember the frame handle */
        hwnd_ = hwnd;

        /* no active object yet */
        actobj_ = 0;
    }

    ~CInPlaceFrame()
    {
        if (actobj_ != 0)
            actobj_->Release();
    }

    /* get the active object's window handle */
    HWND get_obj_hwnd()
    {
        HWND hwnd;

        return (actobj_ != 0 && SUCCEEDED(actobj_->GetWindow(&hwnd))
                ? hwnd : 0);
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* we don't have to expose any interfaces to embed a browser */
        *obj = 0;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetWindow(HWND FAR *lphwnd)
    {
        /* give the browser our window handle */
        *lphwnd = hwnd_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetBorder(LPRECT lprectBorder)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE RequestBorderSpace(LPCBORDERWIDTHS pbw)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetActiveObject(
        IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
    {
        /* remember the active object */
        if (pActiveObject != 0)
            pActiveObject->AddRef();
        if (actobj_ != 0)
            actobj_->Release();
        actobj_ = pActiveObject;

        /* success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE InsertMenus(
        HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetMenu(
        HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
        { return S_OK; }

    HRESULT STDMETHODCALLTYPE RemoveMenus(HMENU hmenuShared)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetStatusText(LPCOLESTR pszStatusText)
        { return S_OK; }

    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL fEnable)
        { return S_OK; }

    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG lpmsg, WORD wID)
        { return E_NOTIMPL; }

protected:
    /* our container window handle */
    HWND hwnd_;

    /* our active object */
    IOleInPlaceActiveObject *actobj_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Our in-place site
 */
class CInPlaceSite: public CBaseOle, public IOleInPlaceSite
{
public:
    CInPlaceSite(HWND hwnd, IOleClientSite *site)
    {
        /* remember our window */
        hwnd_ = hwnd;

        /*
         *   remember our parent client site (don't add a reference to it,
         *   since it owns us - adding a reference would create a circular
         *   reference that would prevent us from ever being deleted)
         */
        client_site_ = site;

        /* create our frame object */
        frame_ = new CInPlaceFrame(hwnd);

        /*
         *   we don't have a browser yet (we can't yet, since we need to
         *   create all of our host interfaces before we can embed the
         *   browser object)
         */
        browser_ = 0;
    }

    ~CInPlaceSite()
    {
        /* release references to the object's we're holding onto */
        if (browser_ != 0)
            browser_->Release();
        if (frame_ != 0)
            frame_->Release();
    }

    /* get the active object's window handle, if available */
    HWND get_active_obj_hwnd()
    {
        return (frame_ != 0 ? frame_->get_obj_hwnd() : 0);
    }

    /* set the browser object */
    void set_browser(IOleObject *browser)
    {
        /* if there's a new object, add a reference to it */
        if (browser != 0)
            browser->AddRef();

        /* if we already have a browser, release the old one */
        if (browser_ != 0)
            browser_->Release();

        /* remember the new one */
        browser_ = browser;
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* let our client site implementation handle it */
        return client_site_->QueryInterface(iid, obj);
    }

    HRESULT STDMETHODCALLTYPE GetWindow(HWND FAR *lphwnd)
    {
        /* return the HWND of our main window */
        *lphwnd = hwnd_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE CanInPlaceActivate()
    {
        /* tell the browser we can activate in place */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceActivate()
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnUIActivate()
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetWindowContext(
        LPOLEINPLACEFRAME FAR *lplpFrame, LPOLEINPLACEUIWINDOW FAR *lplpDoc,
        LPRECT lprcPosRect, LPRECT lprcClipRect,
        LPOLEINPLACEFRAMEINFO lpFrameInfo)
    {
        /*
         *   pass back our in-place frame implementation; add a reference to
         *   it on behalf of the caller
         */
        frame_->AddRef();
        *lplpFrame = frame_;

        /* We have no OLEINPLACEUIWINDOW */
        *lplpDoc = 0;

        /* we're not MDI */
        lpFrameInfo->fMDIApp = FALSE;

        /* return our frame window */
        lpFrameInfo->hwndFrame = hwnd_;

        /* we have no accelerators of our own */
        lpFrameInfo->haccel = 0;
        lpFrameInfo->cAccelEntries = 0;

        /* give the browser the frame window dimensions */
        GetClientRect(hwnd_, lprcPosRect);
        GetClientRect(hwnd_, lprcClipRect);

        /* success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtent)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL fUndoable)
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate()
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DiscardUndoState()
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE DeactivateAndUndo()
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT lprcPosRect)
    {
        IOleInPlaceObject *inplace;

        /* get the browser's in-place object */
        if (browser_ != 0
            && SUCCEEDED(browser_->QueryInterface(
                IID_IOleInPlaceObject, (void**)&inplace)))
        {
            /* give the browser the dimensions of where it can draw */
            inplace->SetObjectRects(lprcPosRect, lprcPosRect);
        }

        /* success */
        return S_OK;
    }

protected:
    /* the window containing the site */
    HWND hwnd_;

    /* our client site implementation */
    IOleClientSite *client_site_;

    /* our in-place frame implementation */
    CInPlaceFrame *frame_;

    /* the browser object we're embedding */
    IOleObject *browser_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Our document host
 */
class CDocHost: public CBaseOle, public IDocHostUIHandler
{
public:
    CDocHost(HWND hwnd, IOleClientSite *site, int context_menu_id,
             IDispatch *ext)
    {
        /* remember the client site - it owns us, so don't add a reference */
        client_site_ = site;

        /* remember the window */
        hwnd_ = hwnd;

        /* remember the context menu */
        context_menu_id_ = context_menu_id;

        /* remember the "external" interface */
        if ((ext_ = ext) != 0)
            ext->AddRef();
    }

    ~CDocHost()
    {
        /* we're done with the "external" interface (if we had one) */
        if (ext_ != 0)
            ext_->Release();
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* let our associated IOleClientSite object handle it */
        return client_site_->QueryInterface(iid, obj);
    }

    HRESULT STDMETHODCALLTYPE ShowContextMenu(
        DWORD dwID, POINT __RPC_FAR *ppt, IUnknown __RPC_FAR *pcmdtReserved,
        IDispatch __RPC_FAR *pdispReserved)
    {
        HMENU m;

        /* if we have a context menu defined, display it */
        if (context_menu_id_ != 0)
        {
            /* show our context menu */
            m = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(context_menu_id_));

            /* run the menu */
            TrackPopupMenu(GetSubMenu(m, 0), TPM_TOPALIGN | TPM_LEFTALIGN,
                           ppt->x, ppt->y, 0, hwnd_, 0);

            /* done with the menu */
            DestroyMenu(m);

            /* indicate success */
            return S_OK;
        }
        else
        {
            /* indicate that we didn't show a menu */
            return S_FALSE;
        }
    }

    HRESULT STDMETHODCALLTYPE GetHostInfo(DOCHOSTUIINFO __RPC_FAR *pInfo)
    {
        /* set the structure size, to indicate interface compatibility */
        pInfo->cbSize = sizeof(DOCHOSTUIINFO);

        /* we don't want a 3D border */
        pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;

        /* use default double-clicking behavior */
        pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;

        /* success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ShowUI(
        DWORD dwID,
        IOleInPlaceActiveObject __RPC_FAR *pActiveObject,
        IOleCommandTarget __RPC_FAR *pCommandTarget,
        IOleInPlaceFrame __RPC_FAR *pFrame,
        IOleInPlaceUIWindow __RPC_FAR *pDoc)
    {
        /*
         *   we contain the whole frame UI - tell the control not to show
         *   toolbars, etc., by returning S_OK
         */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HideUI()
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UpdateUI()
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL fEnable)
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL fActivate)
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL fActivate)
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ResizeBorder(
        LPCRECT prcBorder, IOleInPlaceUIWindow __RPC_FAR *pUIWindow,
        BOOL fRameWindow)
    {
        /* simply indicate success */
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE TranslateAccelerator(
        LPMSG lpMsg, const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID)
    {
        /* we don't translate any accelerators */
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE GetOptionKeyPath(
        LPOLESTR __RPC_FAR *pchKey, DWORD dw)
    {
        /* let the browser use its default registry keys */
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE GetDropTarget(
        IDropTarget __RPC_FAR *pDropTarget,
        IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget)
    {
        /* we don't need a drop target */
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE GetExternal(
        IDispatch __RPC_FAR *__RPC_FAR *ppDispatch)
    {
        /* if we have an ext_ object, return it */
        if ((*ppDispatch = ext_) != 0)
        {
            /* add a reference for the caller and return success */
            ext_->AddRef();
            return S_OK;
        }
        else
        {
            /* no ext_ object */
            return S_FALSE;
        }
    }

    HRESULT STDMETHODCALLTYPE TranslateUrl(
        DWORD dwTranslate, OLECHAR __RPC_FAR *pchURLIn,
        OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut)
    {
        /* we don't need to do any URL translations */
        *ppchURLOut = 0;
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE FilterDataObject(
        IDataObject __RPC_FAR *pDO, IDataObject __RPC_FAR *__RPC_FAR *ppDORet)
    {
        /* we don't provide an IDataObject */
        *ppDORet = 0;
        return(S_FALSE);
    }

protected:
    /* our associated IOleClientSite object */
    IOleClientSite *client_site_;

    /* our window handle */
    HWND hwnd_;

    /* context menu ID */
    int context_menu_id_;

    /* "external" dispatch interface for browser hosting, if applicable */
    IDispatch *ext_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Our client site
 */
class CClientSite: public CBaseOle, public IOleClientSite
{
public:
    CClientSite(HWND hwnd, int context_menu_id, IDispatch *ext,
                IOleCommandTarget *cmd_targ)
    {
        /* create our in-place site */
        in_place_site_ = new CInPlaceSite(hwnd, this);

        /* create our document host */
        doc_host_ = new CDocHost(hwnd, this, context_menu_id, ext);

        /* remember the command target, if provided */
        if ((cmd_targ_ = cmd_targ) != 0)
            cmd_targ_->AddRef();
    }

    ~CClientSite()
    {
        /* release references on our owned objects */
        if (in_place_site_ != 0)
            in_place_site_->Release();
        if (doc_host_ != 0)
            doc_host_->Release();
        if (cmd_targ_ != 0)
            cmd_targ_->Release();
    }

    /* get the active object's window handle, if available */
    HWND get_active_obj_hwnd()
    {
        return (in_place_site_ != 0
                ? in_place_site_->get_active_obj_hwnd() : 0);
    }

    /* set the browser */
    void set_browser(IOleObject *browser)
    {
        /*
         *   we don't need to know about the browser, but our in-place site
         *   does, so pass it through
         */
        in_place_site_->set_browser(browser);
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* check for IUnknown and IOleClientSite */
        if (iid == IID_IUnknown || iid == IID_IOleClientSite)
        {
            /* return 'this', adding a reference on behalf of the caller */
            *obj = (IOleClientSite *)this;
            AddRef();
            return S_OK;
        }

        /* check for IOleInPlaceSite */
        if (iid == IID_IOleInPlaceSite)
        {
            /* return our in-place site object, adding a reference */
            in_place_site_->AddRef();
            *obj = (IOleInPlaceSite *)in_place_site_;
            return S_OK;
        }

        /* check for IDocHostUIHandler */
        if (iid == IID_IDocHostUIHandler)
        {
            /* return our document host object, adding a reference */
            doc_host_->AddRef();
            *obj = (IDocHostUIHandler *)doc_host_;
            return S_OK;
        }

        /* check for IOleCommandTarget */
        if (iid == IID_IOleCommandTarget && cmd_targ_ != 0)
        {
            cmd_targ_->AddRef();
            *obj = (IOleCommandTarget *)cmd_targ_;
            return S_OK;
        }

        /* it's not one we recognize */
        *obj = 0;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE SaveObject()
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetMoniker(
        DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetContainer(LPOLECONTAINER FAR *ppContainer)
    {
        /* report that we don't support a container */
        *ppContainer = 0;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE ShowObject()
        { return NOERROR; }

    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL fShow)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout()
        { return E_NOTIMPL; }

protected:
    /* our in-place site object */
    CInPlaceSite *in_place_site_;

    /* our document host UI handler */
    IDocHostUIHandler *doc_host_;

    /*
     *   our host command target, if any (this allows the embedded MSHTML
     *   control to send commands to the enclosing application frame)
     */
    IOleCommandTarget *cmd_targ_;
};



#endif /* TADSWEBCTL_H */
