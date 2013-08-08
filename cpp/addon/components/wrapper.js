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

function NDNWebRTC() {
    dump("wrapper constructor called\n");
    this.ndnrtc = Cc["@named-data.net/ndnrtc;1"].createInstance().QueryInterface(Ci.INrtc);
}

NDNWebRTC.prototype = {
    
classID: Components.ID("{81BAE33D-C6B4-4BAC-A612-996C8BCC26C3}"),
    
QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
    
    
init: function(aWindow) {
    let self = this;
    let api = {
    Test: self.Test.bind(self),
    ExpressInterest: self.ExpressInterest.bind(self),
		
    __exposedProps__: {
        Test: "r",
        ExpressInterest: "r"
    }
    };
    return api;
},

ExpressInterest: function(interest, onData){
    dump("wrapper express interest");
    return this.ndnrtc.ExpressInterest(interest, onData);
},
    
Test: function(a, b){
    dump("wrapper test called\n");
    return this.ndnrtc.Test(a,b);
}
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([NDNWebRTC]);
