/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM /Users/gpeetonn/Documents/code/CCN/ndnrtc/cpp/idl/ndINrtc.idl
 */

#ifndef __gen_ndINrtc_h__
#define __gen_ndINrtc_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

#ifndef __gen_nsIDOMMediaStream_h__
#include "nsIDOMMediaStream.h"
#endif

#ifndef __gen_nsIDOMCanvasRenderingContext2D_h__
#include "nsIDOMCanvasRenderingContext2D.h"
#endif

#ifndef __gen_nsrootidl_h__
#include "nsrootidl.h"
#endif

#ifndef __gen_nsIPropertyBag2_h__
#include "nsIPropertyBag2.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    INrtcObserver */
#define INRTCOBSERVER_IID_STR "03e963ec-423e-474a-b346-00cac8e6d124"

#define INRTCOBSERVER_IID \
  {0x03e963ec, 0x423e, 0x474a, \
    { 0xb3, 0x46, 0x00, 0xca, 0xc8, 0xe6, 0xd1, 0x24 }}

class NS_NO_VTABLE INrtcObserver : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(INRTCOBSERVER_IID)

  /* void onStateChanged (in string state, in string args); */
  NS_IMETHOD OnStateChanged(const char * state, const char * args) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(INrtcObserver, INRTCOBSERVER_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_INRTCOBSERVER \
  NS_IMETHOD OnStateChanged(const char * state, const char * args); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_INRTCOBSERVER(_to) \
  NS_IMETHOD OnStateChanged(const char * state, const char * args) { return _to OnStateChanged(state, args); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_INRTCOBSERVER(_to) \
  NS_IMETHOD OnStateChanged(const char * state, const char * args) { return !_to ? NS_ERROR_NULL_POINTER : _to->OnStateChanged(state, args); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public INrtcObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTCOBSERVER

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, INrtcObserver)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void onStateChanged (in string state, in string args); */
NS_IMETHODIMP _MYCLASS_::OnStateChanged(const char * state, const char * args)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    INrtc */
#define INRTC_IID_STR "86ad66e9-b3f7-4e6a-beee-802b1d9ff442"

#define INRTC_IID \
  {0x86ad66e9, 0xb3f7, 0x4e6a, \
    { 0xbe, 0xee, 0x80, 0x2b, 0x1d, 0x9f, 0xf4, 0x42 }}

class NS_NO_VTABLE INrtc : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(INRTC_IID)

  /* void startConference (in nsIPropertyBag2 prop, in INrtcObserver observer); */
  NS_IMETHOD StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer) = 0;

  /* void joinConference (in string conferencePrefix, in INrtcObserver observer); */
  NS_IMETHOD JoinConference(const char * conferencePrefix, INrtcObserver *observer) = 0;

  /* void leaveConference (in string conferencePrefix, in INrtcObserver observer); */
  NS_IMETHOD LeaveConference(const char * conferencePrefix, INrtcObserver *observer) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(INrtc, INRTC_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_INRTC \
  NS_IMETHOD StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer); \
  NS_IMETHOD JoinConference(const char * conferencePrefix, INrtcObserver *observer); \
  NS_IMETHOD LeaveConference(const char * conferencePrefix, INrtcObserver *observer); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_INRTC(_to) \
  NS_IMETHOD StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer) { return _to StartConference(prop, observer); } \
  NS_IMETHOD JoinConference(const char * conferencePrefix, INrtcObserver *observer) { return _to JoinConference(conferencePrefix, observer); } \
  NS_IMETHOD LeaveConference(const char * conferencePrefix, INrtcObserver *observer) { return _to LeaveConference(conferencePrefix, observer); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_INRTC(_to) \
  NS_IMETHOD StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer) { return !_to ? NS_ERROR_NULL_POINTER : _to->StartConference(prop, observer); } \
  NS_IMETHOD JoinConference(const char * conferencePrefix, INrtcObserver *observer) { return !_to ? NS_ERROR_NULL_POINTER : _to->JoinConference(conferencePrefix, observer); } \
  NS_IMETHOD LeaveConference(const char * conferencePrefix, INrtcObserver *observer) { return !_to ? NS_ERROR_NULL_POINTER : _to->LeaveConference(conferencePrefix, observer); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public INrtc
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTC

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, INrtc)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void startConference (in nsIPropertyBag2 prop, in INrtcObserver observer); */
NS_IMETHODIMP _MYCLASS_::StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void joinConference (in string conferencePrefix, in INrtcObserver observer); */
NS_IMETHODIMP _MYCLASS_::JoinConference(const char * conferencePrefix, INrtcObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void leaveConference (in string conferencePrefix, in INrtcObserver observer); */
NS_IMETHODIMP _MYCLASS_::LeaveConference(const char * conferencePrefix, INrtcObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_ndINrtc_h__ */
