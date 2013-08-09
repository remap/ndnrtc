//
//  log.js - Simple log system
//  ndnrtc
//
//  Created by Peter Gusev on 7/31/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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