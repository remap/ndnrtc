//
//  segmentizer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <iostream>
#include <string.h>
#include "fec.h"

using namespace fec;

//******************************************************************************
#pragma mark - construction/destruction
Rs28Encoder::Rs28Encoder(unsigned int nSourceSymbols,
                         unsigned int nRepairSymbols,
                         unsigned int symbolLength):
Rs28Coder(nSourceSymbols, nRepairSymbols, symbolLength)
{
}

int
Rs28Encoder::encode(unsigned char* data, unsigned char* parityData)
{
    initCoder();
    
    if (!Rs28Coder<OF_ENCODER>::isCoderReady_)
        return -1;
    
    int ret = 0;
    unsigned char** encodingSymbolTable = buildSymbolTable(data, parityData);
    of_parameters_t* params = Rs28Coder<OF_ENCODER>::coderParameters_;
    of_session_t* session = Rs28Coder<OF_ENCODER>::coderSession_;
    
    for (UINT32 esi = params->nb_source_symbols;
         esi < params->nb_source_symbols + params->nb_repair_symbols && ret >= 0;
         esi++)
    {
				memset(encodingSymbolTable[esi], 0, params->encoding_symbol_length);
        
				if (of_build_repair_symbol(session, (void**)encodingSymbolTable, esi) != OF_STATUS_OK)
				{
            ret = -1;
				}
    }
    
    releaseSymbolTable(encodingSymbolTable);
    
    return ret;
}

//******************************************************************************
#pragma mark - construction/destruction
Rs28Decoder::Rs28Decoder(unsigned int nSourceSymbols,
                         unsigned int nRepairSymbols,
                         unsigned int symbolLength):
Rs28Coder(nSourceSymbols, nRepairSymbols, symbolLength)
{
}

int
Rs28Decoder::decode(unsigned char* data, unsigned char* parityData,
                    unsigned char* rList)
{
    initCoder();
    
    if (!Rs28Coder<OF_DECODER>::isCoderReady_)
        return -1;
    
    int ret = 0;
    unsigned char** decodingSymbolTable = buildSymbolTable(data, parityData, rList);
    of_session_t* session = Rs28Coder<OF_DECODER>::coderSession_;
    
    for (unsigned int i = 0; i < nSourceSymbols_+nRepairSymbols_; i++)
    {
        if (rList[i] == FEC_RLIST_SYMREADY)
            of_decode_with_new_symbol(session, decodingSymbolTable[i], i);
        else
            rList[i] = FEC_RLIST_INPROCESS;
    }
    
    if (of_finish_decoding(coderSession_) != OF_STATUS_OK)
        ret = -1;
    else
    {
        if(of_get_source_symbols_tab(session, (void**)decodingSymbolTable) != OF_STATUS_OK)
            ret = -1;
        else
        {
            for (int i = 0; i < nSourceSymbols_+nRepairSymbols_; i++)
                if (rList[i] == FEC_RLIST_INPROCESS)
                {
                    rList[i] = FEC_RLIST_SYMREPAIRED;
                    ret++;
                    if (i < nSourceSymbols_)
                    {
                        memcpy(&data[i*symbolLength_], decodingSymbolTable[i], symbolLength_);
                        free(decodingSymbolTable[i]);
                    }
                }
        }
    }
    
    releaseSymbolTable(decodingSymbolTable);
    
    return ret;
}
