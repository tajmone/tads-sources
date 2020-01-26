#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadswebctl.cpp - web control embedding support code
Function

Notes

Modified
  05/12/10 MJRoberts  - Creation
*/

#include <Windows.h>
#include <exdisp.h>
#include <oleauto.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <MsHtmcid.h>

#include "tadshtml.h"
#include "tadscom.h"
#include "tadswebctl.h"

/* ------------------------------------------------------------------------ */
/*
 *   embed a browser object in our window
 */
int CBrowserFrame::embed_browser(int show, HWND handle, int context_menu_id,
                                 IDispatch *ext,
                                 IOleCommandTarget *cmd_target)
{
    RECT rc;

    /* if we already have an embedded browser, remove it */
    unembed_browser();

    /* create our client site */
    client_site_ = new CClientSite(handle, context_menu_id, ext, cmd_target);
    storage_ = new CDummyStorage();

    /* create the WebBrowser control */
    if (!OleCreate(CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW,
                   0, client_site_, storage_, (void**)&browser_)
        && browser_ != 0)
    {
        /* tell the client site about the browser */
        client_site_->set_browser(browser_);

        /* set the host name */
        browser_->SetHostNames(L"TADS Workbench", 0);

        /* get our window client area */
        GetClientRect(handle, &rc);

        /* get the IWebBrowser2 interface for the browser */
        if (!SUCCEEDED(browser_->QueryInterface(
            IID_IWebBrowser2, (void**)&iwb2_)))
            iwb2_ = 0;

        /* get the IOleCommandTarget interface */
        if (!SUCCEEDED(browser_->QueryInterface(
            IID_IOleCommandTarget, (void **)&icmd_)))
            icmd_ = 0;

        /* set up our event handler */
        web_evt_ = new CWebEventSink(this, iwb2_);

        /* set up the OLE container embedding */
        if (!OleSetContainedObject(browser_, TRUE))
        {
            /* show the control */
            if (SUCCEEDED(browser_->DoVerb(OLEIVERB_SHOW, NULL,
                                           client_site_, -1, handle, &rc))
                && iwb2_ != 0)
            {
                /* set the display area to take up our whole client area */
                resize_browser_ctl(0, 0, rc.right, rc.bottom);
            }
        }

        /* success */
        return TRUE;
    }

    /* clean up anything we successfully created */
    unembed_browser();

    /* return failure */
    return FALSE;
}

/*
 *   un-embed a browser object
 */
void CBrowserFrame::unembed_browser()
{
    /* if we have a browser, remvoe it */
    if (browser_ != 0)
    {
        /* close the browser */
        browser_->Close(OLECLOSE_NOSAVE);

        /* detach the browser from our client site */
        browser_->SetClientSite(0);

        /* detach the browser from the site */
        client_site_->set_browser(0);

        /* release and forget the browser object */
        browser_->Release();
        browser_ = 0;
    }

    /* unregister and release our event sink */
    if (web_evt_ != 0)
    {
        web_evt_->register_event_handler(FALSE);
        web_evt_->Release();
        web_evt_ = 0;
    }

    /* delete our host interface implementation objects */
    if (iwb2_ != 0)
    {
        iwb2_->Stop();
        iwb2_->Release();
        iwb2_ = 0;
    }
    if (icmd_ != 0)
    {
        icmd_->Release();
        icmd_ = 0;
    }
    if (storage_ != 0)
    {
        storage_->Release();
        storage_ = 0;
    }
    if (client_site_ != 0)
    {
        client_site_->Release();
        client_site_ = 0;
    }
}

/*
 *   navigate to a URL
 */
void CBrowserFrame::go_to_url(const textchar_t *url)
{
    VARIANT v;

    /* if there's no browser control, we can't proceed */
    if (iwb2_ == 0)
        return;

    /* set up a Variant with the target URL */
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = bstr_from_ansi(url);

    /* navigate to our page */
    iwb2_->Navigate2(&v, 0, 0, 0, 0);

    /* free the BSTR */
    VariantClear(&v);
}

