//
//  log.js - Simple log system
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 7/31/13
//

function log(str)
{
    var d = new Date();
    var time = d.getTime();
    
    dump(time/1000+" JS"+" : "+str+"\n");
}

function debug(str)
{
    log("[DEBUG] "+arguments.callee.caller.name+": "+str);
}

function trace(str)
{
    log("[TRACE] "+arguments.callee.caller.name+": "+str);
}

function info(str)
{
    log("[INFO] "+arguments.callee.caller.name+": "+str);
}

function warn(str)
{
    log("[WARN] "+arguments.callee.caller.name+": "+str);
}

function error(str)
{
    log("[ERROR] "+arguments.callee.caller.name+": "+str);
}


var EXPORTED_SYMBOLS = ["error","warn","info","trace","debug"];