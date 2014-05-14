//
//  segmentizer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Authors:  Peter Gusev, Daisuke Ando
//

#ifndef __ndnrtc__fec__
#define __ndnrtc__fec__

#include <cstdlib>

#define FEC_RLIST_SYMREADY '1'
#define FEC_RLIST_SYMEMPTY '0'
#define FEC_RLIST_SYMREPAIRED '2'

extern "C" {
#include <ndn-fec/lib_common/of_openfec_api.h>
}

typedef struct {
    uint32_t data_segment_num;
    uint32_t parity_segment_num;
    uint32_t one_segment_size;
    unsigned char*    buf;
    unsigned char*    r_list;
} FecInfo;

namespace fec
{
    /**
     * This is the base class for Encoder/Decoder derived classes
     */
    template <of_codec_id_t CoderID, of_codec_type_t CoderType>
    class FecCoder
    {
    public:
        FecCoder(unsigned int nSourceSymbols,
                 unsigned int nRepairSymbols,
                 unsigned int symbolLength):
        nSourceSymbols_(nSourceSymbols),
        nRepairSymbols_(nRepairSymbols),
        symbolLength_(symbolLength),
        coderSession_(nullptr),
        coderParameters_(nullptr),
        isCoderCreated_(false),
        coderId_(CoderID),
        coderType_(CoderType)
        {
        }
        
        virtual
        ~FecCoder()
        {
            if (isCoderCreated_)
                releaseCoder();
        }
        
        int
        getCoderId() { return coderId_; }
        
        int
        getCoderType() { return coderType_; }
        
    protected:
        bool isCoderCreated_, isCoderReady_;
        uint32_t nSourceSymbols_, nRepairSymbols_, symbolLength_;
        of_session_t* coderSession_ = NULL;
        of_parameters_t* coderParameters_ = NULL;
        
        virtual void
        initCoder()
        {
            
            isCoderCreated_ = (of_create_codec_instance(&coderSession_, coderId_,
                                                        coderType_, 0) == OF_STATUS_OK);
            
            if (coderParameters_)
            {
                coderParameters_->nb_source_symbols = nSourceSymbols_;
                coderParameters_->nb_repair_symbols = nRepairSymbols_;
                coderParameters_->encoding_symbol_length = symbolLength_;

                isCoderReady_ = (of_set_fec_parameters(coderSession_, coderParameters_) == OF_STATUS_OK);
            }
        }
        
        virtual void
        releaseCoder()
        {
            if (isCoderCreated_)
            {
                isCoderCreated_ = !(of_release_codec_instance(coderSession_) == OF_STATUS_OK);
            }
            
            if (isCoderReady_)
                free(coderParameters_);
        }
        
        unsigned char**
        buildSymbolTable(unsigned char* data, unsigned char* parityData,
                         unsigned char* rList = nullptr)
        {
            unsigned char** encodingSymbolTable = (unsigned char**)malloc(sizeof(char*)*(nSourceSymbols_+nRepairSymbols_));
            
            for (int i = 0; i < nSourceSymbols_+nRepairSymbols_; i++)
            {
                if (i < nSourceSymbols_)
                    encodingSymbolTable[i] = &data[i*symbolLength_];
                else
                    encodingSymbolTable[i] = &parityData[(i-nSourceSymbols_)*symbolLength_];

                if (rList && rList[i] == FEC_RLIST_SYMEMPTY)
                    encodingSymbolTable[i] = NULL;
            }
            
            return encodingSymbolTable;
        }
        
        void
        releaseSymbolTable(unsigned char** encodingSymbolTable)
        { free(encodingSymbolTable); }
        
    private:
        of_codec_id_t coderId_;
        of_codec_type_t coderType_;
    };
    
    template <of_codec_type_t CoderType>
    class Rs28Coder : public FecCoder<OF_CODEC_REED_SOLOMON_GF_2_8_STABLE, CoderType>
    {
    public:
        Rs28Coder(unsigned int nSourceSymbols,
         unsigned int nRepairSymbols,
                  unsigned int symbolLength):
        FecCoder<OF_CODEC_REED_SOLOMON_GF_2_8_STABLE, CoderType>(nSourceSymbols, nRepairSymbols, symbolLength){}
        
    protected:
        void
        initCoder()
        {
            of_rs_parameters_t	*rsParameters;
            
            rsParameters = (of_rs_parameters_t *)calloc(1, sizeof(*rsParameters));
            FecCoder<OF_CODEC_REED_SOLOMON_GF_2_8_STABLE, CoderType>::coderParameters_ = (of_parameters_t *)rsParameters;
            
            FecCoder<OF_CODEC_REED_SOLOMON_GF_2_8_STABLE, CoderType>::initCoder();
        }
    };
    
    class Rs28Encoder : public Rs28Coder<OF_ENCODER>
    {
    public:
        Rs28Encoder(unsigned int nSourceSymbols,
                    unsigned int nRepairSymbols,
                    unsigned int symbolLength);
        
        int
        encode(unsigned char* data, unsigned char* parityData);
        
    private:
        
    };
    
    class Rs28Decoder : public Rs28Coder<OF_DECODER>
    {
    public:
        Rs28Decoder(unsigned int nSourceSymbols,
                    unsigned int nRepairSymbols,
                    unsigned int symbolLength);
        
        int
        decode(unsigned char* data, unsigned char* parityData,
               unsigned char* rList);
        
    private:
    };
    
}

#endif /* defined(__ndnrtc__fec__) */
