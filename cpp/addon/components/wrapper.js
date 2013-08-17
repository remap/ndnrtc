//
//  wrapper.js
//  ndnrtc
//
//  Created by Peter Gusev on 7/31/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

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
}

NDNWebRTC.prototype = {
    
classID: Components.ID("{81BAE33D-C6B4-4BAC-A612-996C8BCC26C3}"),
    
QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
    
    
init: function(aWindow) {
    info("initalizing ndnrtc wrapper");
    
    this.window = aWindow;
    let self = this;
    let api = {
        
    Test: self.Test.bind(self),
    ExpressInterest: self.ExpressInterest.bind(self),
    PublishData: self.PublishData.bind(self),
    AddUserMedia: self.AddUserMedia.bind(self),
        
    __exposedProps__: {
        Test: "r",
        ExpressInterest: "r",
        PublishData: "r",
        AddUserMedia: "r"
    }
    };
    return api;
},

ExpressInterest: function(interest, onData){
    trace("forwarding expressInterest call to ndnrtc");
    return this.ndnrtc.ExpressInterest(interest, onData);
},

PublishData: function(prefix, data){
    trace("publish");
    this.ndnrtc.PublishData(prefix,data);
},

AddUserMedia: function(stream){
    trace("adding media");
    try {
        this.ndnrtc.AddUserMedia(stream);
    }
    catch(e){
        console.log("exception: "+e);
    }
},

Test: function(a, b){
    trace("test called");
    return this.ndnrtc.Test(a,b);
}
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([NDNWebRTC]);
