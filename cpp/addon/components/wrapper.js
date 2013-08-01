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

function NDNWebRTC() {}

NDNWebRTC.prototype = {
    
classID: Components.ID("{81BAE33D-C6B4-4BAC-A612-996C8BCC26C3}"),
    
QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
    
init: function(aWindow) {
    let self = this;
    let api = {
    Test: self.Test.bind(self),
		
    __exposedProps__: {
        Test: "r"
        }
    };
    return api;
},
	
Test: function(a, b){
    var ndnrtc = ndnrtc = Cc["@named-data.net/ndnrtc;1"].createInstance();
    
    ndnrtc = ndnrtc.QueryInterface(Ci.INrtc);
    
    return ndnrtc.Test(a,b);
}
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([NDNWebRTC]);
