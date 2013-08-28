//
//  wrapper.js
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 7/31/13

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://ndnrtc/log.js");


function NDNWebRTC() {
    info("constructor called");
    this.ndnrtc = Cc["@named-data.net/ndnrtc;1"].createInstance().QueryInterface(Ci.INrtc);
    this.window = null;
    this.renderingCanvas = null;
}

NDNWebRTC.prototype = {
    
classID: Components.ID("{81BAE33D-C6B4-4BAC-A612-996C8BCC26C3}"),
    
QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
    
    
init: function(aWindow) {
    info("initalizing ndnrtc wrapper");
    
    this.window = aWindow;
    let self = this;
    let api = {
        
    StartConference: self.StartConference.bind(self),
    JoinConference: self.JoinConference.bind(self),
    LeaveConference: self.LeaveConference.bind(self),
        
    __exposedProps__: {
        StartConference: "r",
        JoinConference: "r",
        LeaveConference: "r"
    }
    };
    
    return api;
},

makePropertyBag_: function(prop) {
    let sP = [];
    let iP = ["width", "height", "windowWidth", "windowHeight"];
    let bP = [];
    
    let bag = Cc["@mozilla.org/hash-property-bag;1"].
    createInstance(Ci.nsIWritablePropertyBag2);
    
    iP.forEach(function(p) {
               if (p in prop)
               bag.setPropertyAsInt32(p, prop[p]);
//               bag.setPropertyAsAString(p, prop[p]);
               });
    bP.forEach(function(p) {
               if (p in prop)
               bag.setPropertyAsBool(p, prop[p]);
               });
    
    return bag;
},
    
StartConference: function(properties, onStateChanged){
    trace("forwarding StartConference call to ndnrtc");

    try {
        let bag = this.makePropertyBag_(properties);
        return this.ndnrtc.startConference(bag, onStateChanged);
    }
    catch (e)
    {
        error("exception caught: "+e);        
        error("exception caught: "+e.message());
    }
},

JoinConference: function(prefix, onStateChanged){
    trace("publish");
    this.ndnrtc.JoinConference(prefix, onStateChanged);
},
    
LeaveConference: function(prefix, onStateChanged){
    trace("test called");
    return this.ndnrtc.LeaveConference(prefix, onStateChanged);
}
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([NDNWebRTC]);
