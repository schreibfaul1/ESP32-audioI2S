#pragma once

#include "Arduino.h"
#include "silk_defines.h"

typedef struct {
    int8_t  GainsIndices[MAX_NB_SUBFR];
    int8_t  LTPIndex[MAX_NB_SUBFR];
    int8_t  NLSFIndices[MAX_LPC_ORDER + 1];
    int16_t lagIndex;
    int8_t  contourIndex;
    int8_t  signalType;
    int8_t  quantOffsetType;
    int8_t  NLSFInterpCoef_Q2;
    int8_t  PERIndex;
    int8_t  LTP_scaleIndex;
    int8_t  Seed;
} sideInfoIndices_t;

typedef struct {
    int32_t AnaState[2];                  /* Analysis filterbank state: 0-8 kHz                                   */
    int32_t AnaState1[2];                 /* Analysis filterbank state: 0-4 kHz                                   */
    int32_t AnaState2[2];                 /* Analysis filterbank state: 0-2 kHz                                   */
    int32_t XnrgSubfr[VAD_N_BANDS];       /* Subframe energies                                                    */
    int32_t NrgRatioSmth_Q8[VAD_N_BANDS]; /* Smoothed energy level in each band                                   */
    int16_t HPstate;                      /* State of differentiator in the lowest band                           */
    int32_t NL[VAD_N_BANDS];              /* Noise energy level in each band                                      */
    int32_t inv_NL[VAD_N_BANDS];          /* Inverse noise energy level in each band                              */
    int32_t NoiseLevelBias[VAD_N_BANDS];  /* Noise level estimator bias/offset                                    */
    int32_t counter;                      /* Frame counter used in the initial phase                              */
} silk_VAD_state_t;

typedef struct {                 /* Variable cut-off low-pass filter state */
    int32_t In_LP_State[2];      /* Low pass filter state */
    int32_t transition_frame_no; /* Counter which is mapped to a cut-off frequency */
    int32_t mode;                /* Operating mode, <0: switch down, >0: switch up; 0: do nothing           */
    int32_t saved_fs_kHz;        /* If non-zero, holds the last sampling rate before a bandwidth switching reset. */
} silk_LP_state_t;

typedef struct { /* Structure containing NLSF codebook */
    const int16_t  nVectors;
    const int16_t  order;
    const int16_t  quantStepSize_Q16;
    const int16_t  invQuantStepSize_Q6;
    const uint8_t* CB1_NLSF_Q8;
    const int16_t* CB1_Wght_Q9;
    const uint8_t* CB1_iCDF;
    const uint8_t* pred_Q8;
    const uint8_t* ec_sel;
    const uint8_t* ec_iCDF;
    const uint8_t* ec_Rates_Q5;
    const int16_t* deltaMin_Q15;
} silk_NLSF_CB_struct_t;

typedef struct _silk_resampler_state_struct {
    int32_t sIIR[SILK_RESAMPLER_MAX_IIR_ORDER]; /* this must be the first element of this struct */
    union {
        int32_t i32[SILK_RESAMPLER_MAX_FIR_ORDER];
        int16_t i16[SILK_RESAMPLER_MAX_FIR_ORDER];
    } sFIR;
    int16_t        delayBuf[48];
    int32_t        resampler_function;
    int32_t        batchSize;
    int32_t        invRatio_Q16;
    int32_t        FIR_Order;
    int32_t        FIR_Fracs;
    int32_t        Fs_in_kHz;
    int32_t        Fs_out_kHz;
    int32_t        inputDelay;
    const int16_t* Coefs;
} silk_resampler_state_struct_t;

typedef struct {
    int16_t pred_prev_Q13[2];
    int16_t sMid[2];
    int16_t sSide[2];
} stereo_dec_state_t;

/* Struct for Packet Loss Concealment */
typedef struct {
    int32_t pitchL_Q8;              /* Pitch lag to use for voiced concealment                          */
    int16_t LTPCoef_Q14[LTP_ORDER]; /* LTP coeficients to use for voiced concealment                    */
    int16_t prevLPC_Q12[MAX_LPC_ORDER];
    int32_t last_frame_lost; /* Was previous frame lost                                          */
    int32_t rand_seed;       /* Seed for unvoiced signal generation                              */
    int16_t randScale_Q14;   /* Scaling of unvoiced random signal                                */
    int32_t conc_energy;
    int32_t conc_energy_shift;
    int16_t prevLTP_scale_Q14;
    int32_t prevGain_Q16[2];
    int32_t fs_kHz;
    int32_t nb_subfr;
    int32_t subfr_length;
} silk_PLC_struct_t;

/* Struct for CNG */
typedef struct {
    int32_t CNG_exc_buf_Q14[MAX_FRAME_LENGTH];
    int16_t CNG_smth_NLSF_Q15[MAX_LPC_ORDER];
    int32_t CNG_synth_state[MAX_LPC_ORDER];
    int32_t CNG_smth_Gain_Q16;
    int32_t rand_seed;
    int32_t fs_kHz;
} silk_CNG_struct_t;

typedef struct {
    int32_t        prev_gain_Q16;
    int32_t        exc_Q14[MAX_FRAME_LENGTH];
    int32_t        sLPC_Q14_buf[MAX_LPC_ORDER];
    int16_t        outBuf[MAX_FRAME_LENGTH + 2 * MAX_SUB_FRAME_LENGTH]; /* Buffer for output signal                    */
    int32_t        lagPrev;                                             /* Previous Lag                                                     */
    int8_t         LastGainIndex;                                       /* Previous gain index                                              */
    int32_t        fs_kHz;                                              /* Sampling frequency in kHz                                        */
    int32_t        fs_API_hz;                                           /* API sample frequency (Hz)                                        */
    int32_t        nb_subfr;                                            /* Number of 5 ms subframes in a frame                              */
    int32_t        frame_length;                                        /* Frame length (samples)                                           */
    int32_t        subfr_length;                                        /* Subframe length (samples)                                        */
    int32_t        ltp_mem_length;                                      /* Length of LTP memory                                             */
    int32_t        LPC_order;                                           /* LPC order                                                        */
    int16_t        prevNLSF_Q15[MAX_LPC_ORDER];                         /* Used to interpolate LSFs                                         */
    int32_t        first_frame_after_reset;                             /* Flag for deactivating NLSF interpolation                         */
    const uint8_t* pitch_lag_low_bits_iCDF;                             /* Pointer to iCDF table for low bits of pitch lag index            */
    const uint8_t* pitch_contour_iCDF;                                  /* Pointer to iCDF table for pitch contour index                    */
    /* For buffering payload in case of more frames per packet */
    int32_t nFramesDecoded;
    int32_t nFramesPerPacket;
    /* Specifically for entropy coding */
    int32_t                      ec_prevSignalType;
    int16_t                      ec_prevLagIndex;
    int32_t                      VAD_flags[MAX_FRAMES_PER_PACKET];
    int32_t                      LBRR_flag;
    int32_t                      LBRR_flags[MAX_FRAMES_PER_PACKET];
    const silk_NLSF_CB_struct_t* psNLSF_CB; /* Pointer to NLSF codebook                                         */
    sideInfoIndices_t            indices;   /* Quantization indices */
    silk_CNG_struct_t            sCNG;      /* CNG state */
    int32_t                      lossCnt;   /* Stuff used for PLC */
    int32_t                      prevSignalType;
    silk_PLC_struct_t            sPLC;
} silk_decoder_state_t;

typedef struct {
    int32_t pitchL[MAX_NB_SUBFR]; /* Prediction and coding parameters */
    int32_t Gains_Q16[MAX_NB_SUBFR];
    int16_t PredCoef_Q12[2][MAX_LPC_ORDER]; /* Holds interpolated and final coefficients, 4-byte aligned */
    int16_t LTPCoef_Q14[LTP_ORDER * MAX_NB_SUBFR];
    int32_t LTP_scale_Q14;
} silk_decoder_control_t;

/* Decoder Super Struct */
typedef struct {
    stereo_dec_state_t sStereo;
    int32_t            nChannelsAPI;
    int32_t            nChannelsInternal;
    int32_t            prev_decode_only_middle;
} silk_decoder_t;

typedef struct {
    int32_t sLPC_Q14[MAX_SUB_FRAME_LENGTH + NSQ_LPC_BUF_LENGTH];
    int32_t RandState[DECISION_DELAY];
    int32_t Q_Q10[DECISION_DELAY];
    int32_t Xq_Q14[DECISION_DELAY];
    int32_t Pred_Q15[DECISION_DELAY];
    int32_t Shape_Q14[DECISION_DELAY];
    int32_t sAR2_Q14[MAX_SHAPE_LPC_ORDER];
    int32_t LF_AR_Q14;
    int32_t Diff_Q14;
    int32_t Seed;
    int32_t SeedInit;
    int32_t RD_Q10;
} NSQ_del_dec_struct;

typedef struct {
    int32_t Q_Q10;
    int32_t RD_Q10;
    int32_t xq_Q14;
    int32_t LF_AR_Q14;
    int32_t Diff_Q14;
    int32_t sLTP_shp_Q14;
    int32_t LPC_exc_Q14;
} NSQ_sample_struct;

typedef NSQ_sample_struct NSQ_sample_pair[2];

typedef struct {
    int32_t nChannelsAPI;       /* I:   Number of channels; 1/2 */
    int32_t nChannelsInternal;  /* I:   Number of channels; 1/2 */
    int32_t API_sampleRate;     /* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000/32000/44100/48000  */
    int32_t internalSampleRate; /* I:   Internal sampling rate used, in Hertz; 8000/12000/16000                         */
    int32_t payloadSize_ms;     /* I:   Number of samples per packet in milliseconds; 10/20/40/60 */
    int32_t prevPitchLag;       /* O:   Pitch lag of previous frame (0 if unvoiced), measured in samples at 48 kHz */
} silk_DecControlStruct_t;
