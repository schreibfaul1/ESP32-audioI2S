#include "neaacdec.h"
#include <assert.h>
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic warning "-Wunused-function"
#define UNUSED_FUNCTION __attribute__((unused)) // maybe sometimes unused (depends on defines)
// ⏫⏫⏫
// global vars
mp4AudioSpecificConfig_t* m_mp4ASC = NULL;
int32_t*                  m_transf_buf = NULL;
int32_t*                  m_windowed_buf = NULL;
codeword_t*               m_codeword = NULL;
bits_t_t*                 m_segment = NULL;
int32_t**                 m_P_dec = NULL;
int32_t**                 m_G_TransientRatio = NULL;
complex_t**               m_X_hybrid_left = NULL;
complex_t**               m_X_hybrid_right = NULL;
int32_t*                  m_x_est = NULL;
int32_t*                  m_X_est = NULL;
complex_t*                m_Z1_imdct = NULL;
complex_t*                m_Z1_mdct = NULL;
complex_t**               m_X_dcf = NULL;
complex_t**               m_X_dsf = NULL;
complex_t**               m_X_left = NULL;
complex_t**               m_X_right = NULL;
int32_t*                  m_vDk0 = NULL;
int32_t*                  m_vDk1 = NULL;
int32_t*                  m_vk0 = NULL;
int32_t*                  m_vk1 = NULL;
int32_t*                  m_lim_imTable = NULL;
uint8_t*                  m_patchBorders = NULL;
sbr_hfadj_info_t*         m_adj = NULL;
int32_t*                  m_Q_M_lim = NULL;
int32_t*                  m_G_lim = NULL;
int32_t*                  m_S_M = NULL;
element_t*                m_sce = NULL;
int16_t*                  m_spec_data = NULL;
element_t*                m_cpe = NULL;
int16_t*                  m_spec_data1 = NULL;
int16_t*                  m_spec_data2 = NULL;
int32_t*                  m_spec_coef = NULL;
int32_t*                  m_spec_coef1 = NULL;
int32_t*                  m_spec_coef2 = NULL;

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool alloc_mem() {
    // clang-format off
    bool mem1 = true, mem2 = true;
    uint32_t sum = 0;
    m_transf_buf = (int32_t*)faad_malloc(2 * 1024 * sizeof(int32_t));                                                        sum += 2 * 1024 * sizeof(int32_t);
    m_Z1_imdct = (complex_t*)faad_malloc(512 * sizeof(complex_t));                                                           sum += 512 * sizeof(complex_t);
    m_sce = (element_t*)faad_malloc(1 * sizeof(element_t));                                                                  sum += 1 * sizeof(element_t);
    m_spec_data = (int16_t*)faad_malloc(1024 * sizeof(int16_t));                                                             sum += 1024 * sizeof(int16_t);
    m_spec_coef = (int32_t*)faad_malloc(1024 * sizeof(int32_t));                                                             sum += 1024 * sizeof(int32_t);
    m_cpe = (element_t*)faad_malloc(1 * sizeof(element_t));                                                                  sum += 1 * sizeof(element_t);
    m_spec_data1 = (int16_t*)faad_malloc(1024 * sizeof(int16_t));                                                            sum += 1024 * sizeof(int16_t);
    m_spec_data2 = (int16_t*)faad_malloc(1024 * sizeof(int16_t));                                                            sum += 1024 * sizeof(int16_t);
    m_spec_coef1 = (int32_t*)faad_malloc(1024 * sizeof(int32_t));                                                            sum += 1024 * sizeof(int32_t);
    m_spec_coef2 = (int32_t*)faad_malloc(1024 * sizeof(int32_t));                                                            sum += 1024 * sizeof(int32_t);
    m_mp4ASC = (mp4AudioSpecificConfig_t*)faad_malloc(1 * sizeof(mp4AudioSpecificConfig_t));                                 sum += 1 * sizeof(mp4AudioSpecificConfig_t);
    if(!m_transf_buf || !m_Z1_imdct || !m_sce || !m_spec_data || !m_spec_coef || !m_cpe || !m_spec_data1 || !m_spec_data2 || !m_spec_coef1 || !m_spec_coef2 ||!m_mp4ASC){mem1 = false;}
#ifdef SBR_DEC
    m_P_dec = (int32_t**)faad_malloc(32 * sizeof(m_P_dec));                                                                  sum += 32 * sizeof(int32_t*);
    for(uint8_t i = 0; i < 32; i++){m_P_dec[i] = (int32_t*)faad_malloc(34 * sizeof(*(m_P_dec[i])));}                         sum += 32 * 34 * sizeof(int32_t);
    m_G_TransientRatio = (int32_t**)faad_malloc(32 * sizeof(m_G_TransientRatio));                                            sum += 32 * sizeof(int32_t*);
    for(uint8_t i = 0; i < 32; i++){m_G_TransientRatio[i] = (int32_t*)faad_malloc(34 * sizeof(*(m_G_TransientRatio[i])));}   sum += 32 * 34 * sizeof(int32_t);
    m_X_hybrid_left = (complex_t**)faad_malloc(32 * sizeof(m_X_hybrid_left));                                                sum += 32 * sizeof(complex_t*);
    for(uint8_t i = 0; i < 32; i++){m_X_hybrid_left[i] = (complex_t*)faad_malloc(34 * sizeof(*(m_X_hybrid_left[i])));}       sum += 32 * 34 * sizeof(complex_t);
    m_X_hybrid_right = (complex_t**)faad_malloc(32 * sizeof(m_X_hybrid_right));                                              sum += 32 * sizeof(complex_t*);
    for(uint8_t i = 0; i < 32; i++){m_X_hybrid_right[i] = (complex_t*)faad_malloc(34 * sizeof(*(m_X_hybrid_right[i])));}     sum += 32 * 34 * sizeof(complex_t);
    m_X_dsf = (complex_t**)faad_malloc(MAX_NTSRHFG * sizeof(m_X_dsf));                                                          sum += MAX_NTSR * sizeof(complex_t*);
    for(uint8_t i = 0; i < MAX_NTSRHFG; i++){m_X_dsf[i] = (complex_t*)faad_malloc(64 * sizeof(*(m_X_dsf[i])));}                 sum += MAX_NTSR * 64 * sizeof(complex_t);
    m_X_dcf = (complex_t**)faad_malloc(MAX_NTSRHFG * sizeof(m_X_dcf));                                                          sum += MAX_NTSR * sizeof(complex_t*);
    for(uint8_t i = 0; i < MAX_NTSRHFG; i++){m_X_dcf[i] = (complex_t*)faad_malloc(64 * sizeof(*(m_X_dcf[i])));}                 sum += MAX_NTSR * 64 * sizeof(complex_t);
    m_X_left = (complex_t**)faad_malloc(38 * sizeof(m_X_left));                                                              sum += 38 * sizeof(complex_t*);
    for(uint8_t i = 0; i < 38; i++){ m_X_left[i] = (complex_t*)faad_malloc(64 * sizeof(*(m_X_left[i])));}                    sum += 38 * 64 * sizeof(int32_t);
    m_X_right = (complex_t**)faad_malloc(38 * sizeof(m_X_right));                                                            sum += 38 * sizeof(complex_t*);
    for(uint8_t i = 0; i < 38; i++){m_X_right[i] = (complex_t*)faad_malloc(64 * sizeof(*(m_X_right[i])));}                   sum += 38 * 64 * sizeof(int32_t);
    m_vDk0 = (int32_t*)faad_malloc(64 * sizeof(int32_t));                                                                    sum += 64 * sizeof(int32_t);
    m_vDk1 = (int32_t*)faad_malloc(64 * sizeof(int32_t));                                                                    sum += 64 * sizeof(int32_t);
    m_vk0 = (int32_t*)faad_malloc(64 * sizeof(int32_t));                                                                     sum += 64 * sizeof(int32_t);
    m_vk1 = (int32_t*)faad_malloc(64 * sizeof(int32_t));                                                                     sum += 64 * sizeof(int32_t);
    m_adj = (sbr_hfadj_info_t*)faad_malloc(1 * sizeof(sbr_hfadj_info_t));                                                    sum += 1 * sizeof(sbr_hfadj_info_t);
    m_Q_M_lim = (int32_t*)faad_malloc(MAX_M * sizeof(int32_t));                                                              sum += MAX_M * sizeof(int32_t);
    m_G_lim = (int32_t*)faad_malloc(MAX_M * sizeof(int32_t));                                                                sum += MAX_M * sizeof(int32_t);
    m_S_M = (int32_t*)faad_malloc(MAX_M * sizeof(int32_t));                                                                  sum += MAX_M * sizeof(int32_t);

    if(!m_P_dec || !m_G_TransientRatio || !m_X_hybrid_left || !m_X_hybrid_right || !m_X_dsf || !m_X_dcf || !m_X_left || !m_X_right ||!m_vDk0 || !m_vDk1 || !m_adj || !m_Q_M_lim || !m_G_lim  ||
       !m_S_M) {mem2 = false;}

#endif
    if(!mem1 || !mem2) {return false;}
    // printf(ANSI_ESC_ORANGE "libfaad2 INFO: %li bytes allocated" ANSI_ESC_WHITE "\n", sum);
    return true;
    // clang-format off
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void free_mem() {
    // clang-format off
    if(m_mp4ASC)           {free(m_mp4ASC);       m_mp4ASC = NULL;}
    if(m_transf_buf)       {free(m_transf_buf);   m_transf_buf = NULL; }
    if(m_windowed_buf)     {free(m_windowed_buf); m_windowed_buf = NULL;}
    if(m_codeword)         {free(m_codeword);     m_codeword = NULL;}
    if(m_segment)          {free(m_segment);      m_segment = NULL;}
    if(m_P_dec)            {for(uint8_t i = 0; i < 32; i++){free(m_P_dec[i]);            m_P_dec[i] = NULL;}            free(m_P_dec);            m_P_dec = NULL;}
    if(m_G_TransientRatio) {for(uint8_t i = 0; i < 32; i++){free(m_G_TransientRatio[i]); m_G_TransientRatio[i] = NULL;} free(m_G_TransientRatio); m_G_TransientRatio = NULL;}
    if(m_X_hybrid_left)    {for(uint8_t i = 0; i < 32; i++){free(m_X_hybrid_left[i]);    m_X_hybrid_left[i] = NULL;}    free(m_X_hybrid_left);    m_X_hybrid_left = NULL;}
    if(m_X_hybrid_right)   {for(uint8_t i = 0; i < 32; i++){free(m_X_hybrid_right[i]);   m_X_hybrid_right[i] = NULL;}   free(m_X_hybrid_right);   m_X_hybrid_right = NULL;}
    if(m_x_est)            {free(m_x_est);        m_x_est = NULL;}
    if(m_X_est)            {free(m_X_est);        m_X_est = NULL;}
    if(m_Z1_imdct)         {free(m_Z1_imdct);     m_Z1_imdct = NULL;}
    if(m_Z1_mdct)          {free(m_Z1_mdct);      m_Z1_mdct = NULL;}
    if(m_X_dcf)            {for(uint8_t i = 0; i < MAX_NTSR; i++){free(m_X_dcf[i]); m_X_dcf[i] = NULL;} free(m_X_dcf); m_X_dcf = NULL;}
    if(m_X_dsf)            {for(uint8_t i = 0; i < MAX_NTSR; i++){free(m_X_dsf[i]); m_X_dsf[i] = NULL;} free(m_X_dsf); m_X_dsf = NULL;}
    if(m_X_left)           {for(uint8_t i = 0; i < 38; i++){free(m_X_left[i]); m_X_left[i] = NULL;} free(m_X_left);}
    if(m_X_right)          {for(uint8_t i = 0; i < 38; i++){free(m_X_right[i]); m_X_right[i] = NULL;} free(m_X_right);}
    if(m_vk1)              {free(m_vk1); m_vk1 = NULL;}
    if(m_vk0)              {free(m_vk0); m_vk0 = NULL;}
    if(m_vDk1)             {free(m_vDk1); m_vDk1 = NULL;}
    if(m_vDk0)             {free(m_vDk0); m_vDk0 = NULL;}
    if(m_patchBorders)     {free(m_patchBorders); m_patchBorders = NULL;}
    if(m_lim_imTable)      {free(m_lim_imTable); m_lim_imTable = NULL;}
    if(m_adj)              {free(m_adj); m_adj = NULL;}
    if(m_S_M)              {free(m_S_M); m_S_M = NULL;}
    if(m_G_lim)            {free(m_G_lim); m_G_lim = NULL;}
    if(m_Q_M_lim)          {free(m_Q_M_lim); m_Q_M_lim = NULL;}
    if(m_spec_data)        {free(m_spec_data); m_spec_data = NULL;}
    if(m_sce)              {free(m_sce); m_sce = NULL;}
    if(m_cpe)              {free(m_cpe); m_cpe = NULL;}
    if(m_spec_data1)       {free(m_spec_data1); m_spec_data1 = NULL;}
    if(m_spec_data2)       {free(m_spec_data2); m_spec_data2 = NULL;}
    if(m_spec_coef)        {free(m_spec_coef); m_spec_coef = NULL;}
    if(m_spec_coef1)       {free(m_spec_coef1); m_spec_coef1 = NULL;}
    if(m_spec_coef2)       {free(m_spec_coef2); m_spec_coef2 = NULL;}

    // printf(ANSI_ESC_ORANGE "libfaad2 INFO: memory freed" ANSI_ESC_WHITE "\n");
    // clang-format on
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* initialize buffer, call once before first getbits or showbits */
static void faad_initbits(bitfile_t* ld, const void* _buffer, const uint32_t buffer_size) {
    uint32_t tmp;

    if(ld == NULL) return;
    if(buffer_size == 0 || _buffer == NULL) {
        ld->error = 1;
        return;
    }
    ld->buffer = _buffer;
    ld->buffer_size = buffer_size;
    ld->bytes_left = buffer_size;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)ld->buffer);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)ld->buffer + 1);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer + 1, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;

    ld->start = (uint32_t*)ld->buffer;
    ld->tail = ((uint32_t*)ld->buffer + 2);
    ld->bits_left = 32;
    ld->error = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t faad_get_processed_bits(bitfile_t* ld) { return (uint32_t)(8 * (4 * (ld->tail - ld->start) - 4) - (ld->bits_left)); }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t faad_byte_align(bitfile_t* ld) {
    int32_t remainder = (32 - ld->bits_left) & 0x7;
    if(remainder) {
        faad_flushbits(ld, 8 - remainder);
        return (uint8_t)(8 - remainder);
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void faad_flushbits_ex(bitfile_t* ld, uint32_t bits) {
    uint32_t tmp;

    ld->bufa = ld->bufb;
    if(ld->bytes_left >= 4) {
        tmp = getdword(ld->tail);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(ld->tail, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->tail++;
    ld->bits_left += (32 - bits);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* rewind to beginning */
static void faad_rewindbits(bitfile_t* ld) {
    uint32_t tmp;

    ld->bytes_left = ld->buffer_size;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)&ld->start[0]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[0], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)&ld->start[1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->bits_left = 32;
    ld->tail = &ld->start[2];
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* reset to a certain point */
static void faad_resetbits(bitfile_t* ld, int32_t bits) {
    uint32_t tmp;
    uint32_t words = bits >> 5;
    int32_t  remainder = bits & 0x1F;

    if(ld->buffer_size < words * 4) { ld->bytes_left = 0; }
    else { ld->bytes_left = ld->buffer_size - words * 4; }
    if(ld->bytes_left >= 4) {
        tmp = getdword(&ld->start[words]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword(&ld->start[words + 1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words + 1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->bits_left = 32 - remainder;
    ld->tail = &ld->start[words + 2];
    /* recheck for reading too many bytes */
    ld->error = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t* faad_getbitbuffer(bitfile_t* ld, uint32_t bits) {
    int32_t  i;
    uint32_t temp;
    int32_t  bytes = bits >> 3;
    int32_t  remainder = bits & 0x7;

    uint8_t* buffer = (uint8_t*)faad_malloc((bytes + 1) * sizeof(uint8_t));
    for(i = 0; i < bytes; i++) { buffer[i] = (uint8_t)faad_getbits(ld, 8); }
    if(remainder) {
        temp = faad_getbits(ld, remainder) << (8 - remainder);
        buffer[bytes] = (uint8_t)temp;
    }
    return buffer;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* reversed bit reading routines, used for RVLC and HCR */
UNUSED_FUNCTION static void faad_initbits_rev(bitfile_t* ld, void* buffer, uint32_t bits_in_buffer) {
    uint32_t tmp;
    int32_t  index;

    ld->buffer_size = bit2byte(bits_in_buffer);
    index = (bits_in_buffer + 31) / 32 - 1;
    ld->start = (uint32_t*)buffer + index - 2;
    tmp = getdword((uint32_t*)buffer + index);
    ld->bufa = tmp;
    tmp = getdword((uint32_t*)buffer + index - 1);
    ld->bufb = tmp;
    ld->tail = (uint32_t*)buffer + index;
    ld->bits_left = bits_in_buffer % 32;
    if(ld->bits_left == 0) ld->bits_left = 32;
    ld->bytes_left = ld->buffer_size;
    ld->error = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//   passf2, passf3, passf4, passf5. Complex FFT passes fwd and bwd.
static void passf2pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa) {
    uint16_t i, k, ah, ac;

    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            ah = 2 * k;
            ac = 4 * k;
            RE(ch[ah]) = RE(cc[ac]) + RE(cc[ac + 1]);
            RE(ch[ah + l1]) = RE(cc[ac]) - RE(cc[ac + 1]);
            IM(ch[ah]) = IM(cc[ac]) + IM(cc[ac + 1]);
            IM(ch[ah + l1]) = IM(cc[ac]) - IM(cc[ac + 1]);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ah = k * ido;
            ac = 2 * k * ido;
            for(i = 0; i < ido; i++) {
                complex_t t2;
                RE(ch[ah + i]) = RE(cc[ac + i]) + RE(cc[ac + i + ido]);
                RE(t2) = RE(cc[ac + i]) - RE(cc[ac + i + ido]);
                IM(ch[ah + i]) = IM(cc[ac + i]) + IM(cc[ac + i + ido]);
                IM(t2) = IM(cc[ac + i]) - IM(cc[ac + i + ido]);
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(t2), RE(t2), RE(wa[i]), IM(wa[i]));
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void passf2neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa) {
    uint16_t i, k, ah, ac;

    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            ah = 2 * k;
            ac = 4 * k;
            RE(ch[ah]) = RE(cc[ac]) + RE(cc[ac + 1]);
            RE(ch[ah + l1]) = RE(cc[ac]) - RE(cc[ac + 1]);
            IM(ch[ah]) = IM(cc[ac]) + IM(cc[ac + 1]);
            IM(ch[ah + l1]) = IM(cc[ac]) - IM(cc[ac + 1]);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ah = k * ido;
            ac = 2 * k * ido;
            for(i = 0; i < ido; i++) {
                complex_t t2;
                RE(ch[ah + i]) = RE(cc[ac + i]) + RE(cc[ac + i + ido]);
                RE(t2) = RE(cc[ac + i]) - RE(cc[ac + i + ido]);
                IM(ch[ah + i]) = IM(cc[ac + i]) + IM(cc[ac + i + ido]);
                IM(t2) = IM(cc[ac + i]) - IM(cc[ac + i + ido]);
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(t2), IM(t2), RE(wa[i]), IM(wa[i]));
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void passf3(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const int8_t isign) {
    const int32_t taur = FRAC_CONST(-0.5);
    const int32_t taui = FRAC_CONST(0.866025403784439);
    uint16_t      i, k, ac, ah;
    complex_t     c2, c3, d2, d3, t2;

    if(ido == 1) {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                ac = 3 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 1]);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), taur);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), taur);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2);
                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + 1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + 1])), taui);
                RE(ch[ah + l1]) = RE(c2) - IM(c3);
                IM(ch[ah + l1]) = IM(c2) + RE(c3);
                RE(ch[ah + 2 * l1]) = RE(c2) + IM(c3);
                IM(ch[ah + 2 * l1]) = IM(c2) - RE(c3);
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                ac = 3 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 1]);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), taur);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), taur);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2);
                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + 1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + 1])), taui);
                RE(ch[ah + l1]) = RE(c2) + IM(c3);
                IM(ch[ah + l1]) = IM(c2) - RE(c3);
                RE(ch[ah + 2 * l1]) = RE(c2) - IM(c3);
                IM(ch[ah + 2 * l1]) = IM(c2) + RE(c3);
            }
        }
    }
    else {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (3 * k + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + ido]);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + ido]);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), taur);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2);
                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + ido])), taui);
                    RE(d2) = RE(c2) - IM(c3);
                    IM(d3) = IM(c2) - RE(c3);
                    RE(d3) = RE(c2) + IM(c3);
                    IM(d2) = IM(c2) + RE(c3);
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                }
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (3 * k + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + ido]);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + ido]);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), taur);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2);
                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + ido])), taui);
                    RE(d2) = RE(c2) + IM(c3);
                    IM(d3) = IM(c2) + RE(c3);
                    RE(d3) = RE(c2) - IM(c3);
                    IM(d2) = IM(c2) - RE(c3);
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void passf4pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3) {
    uint16_t i, k, ac, ah;

    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            complex_t t1, t2, t3, t4;
            ac = 4 * k;
            ah = k;
            RE(t2) = RE(cc[ac]) + RE(cc[ac + 2]);
            RE(t1) = RE(cc[ac]) - RE(cc[ac + 2]);
            IM(t2) = IM(cc[ac]) + IM(cc[ac + 2]);
            IM(t1) = IM(cc[ac]) - IM(cc[ac + 2]);
            RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 3]);
            IM(t4) = RE(cc[ac + 1]) - RE(cc[ac + 3]);
            IM(t3) = IM(cc[ac + 3]) + IM(cc[ac + 1]);
            RE(t4) = IM(cc[ac + 3]) - IM(cc[ac + 1]);
            RE(ch[ah]) = RE(t2) + RE(t3);
            RE(ch[ah + 2 * l1]) = RE(t2) - RE(t3);
            IM(ch[ah]) = IM(t2) + IM(t3);
            IM(ch[ah + 2 * l1]) = IM(t2) - IM(t3);
            RE(ch[ah + l1]) = RE(t1) + RE(t4);
            RE(ch[ah + 3 * l1]) = RE(t1) - RE(t4);
            IM(ch[ah + l1]) = IM(t1) + IM(t4);
            IM(ch[ah + 3 * l1]) = IM(t1) - IM(t4);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ac = 4 * k * ido;
            ah = k * ido;
            for(i = 0; i < ido; i++) {
                complex_t c2, c3, c4, t1, t2, t3, t4;
                RE(t2) = RE(cc[ac + i]) + RE(cc[ac + i + 2 * ido]);
                RE(t1) = RE(cc[ac + i]) - RE(cc[ac + i + 2 * ido]);
                IM(t2) = IM(cc[ac + i]) + IM(cc[ac + i + 2 * ido]);
                IM(t1) = IM(cc[ac + i]) - IM(cc[ac + i + 2 * ido]);
                RE(t3) = RE(cc[ac + i + ido]) + RE(cc[ac + i + 3 * ido]);
                IM(t4) = RE(cc[ac + i + ido]) - RE(cc[ac + i + 3 * ido]);
                IM(t3) = IM(cc[ac + i + 3 * ido]) + IM(cc[ac + i + ido]);
                RE(t4) = IM(cc[ac + i + 3 * ido]) - IM(cc[ac + i + ido]);
                RE(c2) = RE(t1) + RE(t4);
                RE(c4) = RE(t1) - RE(t4);
                IM(c2) = IM(t1) + IM(t4);
                IM(c4) = IM(t1) - IM(t4);
                RE(ch[ah + i]) = RE(t2) + RE(t3);
                RE(c3) = RE(t2) - RE(t3);
                IM(ch[ah + i]) = IM(t2) + IM(t3);
                IM(c3) = IM(t2) - IM(t3);
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(c2), RE(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&IM(ch[ah + i + 2 * l1 * ido]), &RE(ch[ah + i + 2 * l1 * ido]), IM(c3), RE(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&IM(ch[ah + i + 3 * l1 * ido]), &RE(ch[ah + i + 3 * l1 * ido]), IM(c4), RE(c4), RE(wa3[i]), IM(wa3[i]));
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void passf4neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3) {
    uint16_t i, k, ac, ah;

    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            complex_t t1, t2, t3, t4;
            ac = 4 * k;
            ah = k;
            RE(t2) = RE(cc[ac]) + RE(cc[ac + 2]);
            RE(t1) = RE(cc[ac]) - RE(cc[ac + 2]);
            IM(t2) = IM(cc[ac]) + IM(cc[ac + 2]);
            IM(t1) = IM(cc[ac]) - IM(cc[ac + 2]);
            RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 3]);
            IM(t4) = RE(cc[ac + 1]) - RE(cc[ac + 3]);
            IM(t3) = IM(cc[ac + 3]) + IM(cc[ac + 1]);
            RE(t4) = IM(cc[ac + 3]) - IM(cc[ac + 1]);
            RE(ch[ah]) = RE(t2) + RE(t3);
            RE(ch[ah + 2 * l1]) = RE(t2) - RE(t3);
            IM(ch[ah]) = IM(t2) + IM(t3);
            IM(ch[ah + 2 * l1]) = IM(t2) - IM(t3);
            RE(ch[ah + l1]) = RE(t1) - RE(t4);
            RE(ch[ah + 3 * l1]) = RE(t1) + RE(t4);
            IM(ch[ah + l1]) = IM(t1) - IM(t4);
            IM(ch[ah + 3 * l1]) = IM(t1) + IM(t4);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ac = 4 * k * ido;
            ah = k * ido;
            for(i = 0; i < ido; i++) {
                complex_t c2, c3, c4, t1, t2, t3, t4;
                RE(t2) = RE(cc[ac + i]) + RE(cc[ac + i + 2 * ido]);
                RE(t1) = RE(cc[ac + i]) - RE(cc[ac + i + 2 * ido]);
                IM(t2) = IM(cc[ac + i]) + IM(cc[ac + i + 2 * ido]);
                IM(t1) = IM(cc[ac + i]) - IM(cc[ac + i + 2 * ido]);
                RE(t3) = RE(cc[ac + i + ido]) + RE(cc[ac + i + 3 * ido]);
                IM(t4) = RE(cc[ac + i + ido]) - RE(cc[ac + i + 3 * ido]);
                IM(t3) = IM(cc[ac + i + 3 * ido]) + IM(cc[ac + i + ido]);
                RE(t4) = IM(cc[ac + i + 3 * ido]) - IM(cc[ac + i + ido]);
                RE(c2) = RE(t1) - RE(t4);
                RE(c4) = RE(t1) + RE(t4);
                IM(c2) = IM(t1) - IM(t4);
                IM(c4) = IM(t1) + IM(t4);
                RE(ch[ah + i]) = RE(t2) + RE(t3);
                RE(c3) = RE(t2) - RE(t3);
                IM(ch[ah + i]) = IM(t2) + IM(t3);
                IM(c3) = IM(t2) - IM(t3);
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(c2), IM(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&RE(ch[ah + i + 2 * l1 * ido]), &IM(ch[ah + i + 2 * l1 * ido]), RE(c3), IM(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&RE(ch[ah + i + 3 * l1 * ido]), &IM(ch[ah + i + 3 * l1 * ido]), RE(c4), IM(c4), RE(wa3[i]), IM(wa3[i]));
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void passf5(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3, const complex_t* wa4,
                   const int8_t isign) {
    int32_t   tr11 = FRAC_CONST(0.309016994374947);
    int32_t   ti11 = FRAC_CONST(0.951056516295154);
    int32_t   tr12 = FRAC_CONST(-0.809016994374947);
    int32_t   ti12 = FRAC_CONST(0.587785252292473);
    uint16_t  i, k, ac, ah;
    complex_t c2, c3, c4, c5, d3, d4, d5, d2, t2, t3, t4, t5;

    if(ido == 1) {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                ac = 5 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 3]);
                RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 2]);
                IM(t3) = IM(cc[ac + 1]) + IM(cc[ac + 2]);
                RE(t4) = RE(cc[ac + 1]) - RE(cc[ac + 2]);
                IM(t4) = IM(cc[ac + 1]) - IM(cc[ac + 2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac + 3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac + 3]);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2) + IM(t3);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                RE(c3) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                IM(c3) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                ComplexMult(&RE(c5), &RE(c4), ti11, ti12, RE(t5), RE(t4));
                ComplexMult(&IM(c5), &IM(c4), ti11, ti12, IM(t5), IM(t4));
                RE(ch[ah + l1]) = RE(c2) - IM(c5);
                IM(ch[ah + l1]) = IM(c2) + RE(c5);
                RE(ch[ah + 2 * l1]) = RE(c3) - IM(c4);
                IM(ch[ah + 2 * l1]) = IM(c3) + RE(c4);
                RE(ch[ah + 3 * l1]) = RE(c3) + IM(c4);
                IM(ch[ah + 3 * l1]) = IM(c3) - RE(c4);
                RE(ch[ah + 4 * l1]) = RE(c2) + IM(c5);
                IM(ch[ah + 4 * l1]) = IM(c2) - RE(c5);
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                ac = 5 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 3]);
                RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 2]);
                IM(t3) = IM(cc[ac + 1]) + IM(cc[ac + 2]);
                RE(t4) = RE(cc[ac + 1]) - RE(cc[ac + 2]);
                IM(t4) = IM(cc[ac + 1]) - IM(cc[ac + 2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac + 3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac + 3]);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2) + IM(t3);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                RE(c3) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                IM(c3) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                ComplexMult(&RE(c4), &RE(c5), ti12, ti11, RE(t5), RE(t4));
                ComplexMult(&IM(c4), &IM(c5), ti12, ti11, IM(t5), IM(t4));
                RE(ch[ah + l1]) = RE(c2) + IM(c5);
                IM(ch[ah + l1]) = IM(c2) - RE(c5);
                RE(ch[ah + 2 * l1]) = RE(c3) + IM(c4);
                IM(ch[ah + 2 * l1]) = IM(c3) - RE(c4);
                RE(ch[ah + 3 * l1]) = RE(c3) - IM(c4);
                IM(ch[ah + 3 * l1]) = IM(c3) + RE(c4);
                RE(ch[ah + 4 * l1]) = RE(c2) - IM(c5);
                IM(ch[ah + 4 * l1]) = IM(c2) + RE(c5);
            }
        }
    }
    else {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (k * 5 + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + 3 * ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + 3 * ido]);
                    RE(t3) = RE(cc[ac + ido]) + RE(cc[ac + 2 * ido]);
                    IM(t3) = IM(cc[ac + ido]) + IM(cc[ac + 2 * ido]);
                    RE(t4) = RE(cc[ac + ido]) - RE(cc[ac + 2 * ido]);
                    IM(t4) = IM(cc[ac + ido]) - IM(cc[ac + 2 * ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac + 3 * ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac + 3 * ido]);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2) + IM(t3);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                    RE(c3) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                    IM(c3) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                    ComplexMult(&RE(c5), &RE(c4), ti11, ti12, RE(t5), RE(t4));
                    ComplexMult(&IM(c5), &IM(c4), ti11, ti12, IM(t5), IM(t4));
                    IM(d2) = IM(c2) + RE(c5);
                    IM(d3) = IM(c3) + RE(c4);
                    RE(d4) = RE(c3) + IM(c4);
                    RE(d5) = RE(c2) + IM(c5);
                    RE(d2) = RE(c2) - IM(c5);
                    IM(d5) = IM(c2) - RE(c5);
                    RE(d3) = RE(c3) - IM(c4);
                    IM(d4) = IM(c3) - RE(c4);
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&IM(ch[ah + 3 * l1 * ido]), &RE(ch[ah + 3 * l1 * ido]), IM(d4), RE(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&IM(ch[ah + 4 * l1 * ido]), &RE(ch[ah + 4 * l1 * ido]), IM(d5), RE(d5), RE(wa4[i]), IM(wa4[i]));
                }
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (k * 5 + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + 3 * ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + 3 * ido]);
                    RE(t3) = RE(cc[ac + ido]) + RE(cc[ac + 2 * ido]);
                    IM(t3) = IM(cc[ac + ido]) + IM(cc[ac + 2 * ido]);
                    RE(t4) = RE(cc[ac + ido]) - RE(cc[ac + 2 * ido]);
                    IM(t4) = IM(cc[ac + ido]) - IM(cc[ac + 2 * ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac + 3 * ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac + 3 * ido]);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2) + IM(t3);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                    RE(c3) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                    IM(c3) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                    ComplexMult(&RE(c4), &RE(c5), ti12, ti11, RE(t5), RE(t4));
                    ComplexMult(&IM(c4), &IM(c5), ti12, ti11, IM(t5), IM(t4));
                    IM(d2) = IM(c2) - RE(c5);
                    IM(d3) = IM(c3) - RE(c4);
                    RE(d4) = RE(c3) - IM(c4);
                    RE(d5) = RE(c2) - IM(c5);
                    RE(d2) = RE(c2) + IM(c5);
                    IM(d5) = IM(c2) + RE(c5);
                    RE(d3) = RE(c3) + IM(c4);
                    IM(d4) = IM(c3) + RE(c4);
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&RE(ch[ah + 3 * l1 * ido]), &IM(ch[ah + 3 * l1 * ido]), RE(d4), IM(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&RE(ch[ah + 4 * l1 * ido]), &IM(ch[ah + 4 * l1 * ido]), RE(d5), IM(d5), RE(wa4[i]), IM(wa4[i]));
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//   cfftf1, cfftf, cfftb, cffti1, cffti. Complex FFTs.
static void cfftf1pos(uint16_t n, complex_t* c, complex_t* ch, const uint16_t* ifac, const complex_t* wa, const int8_t isign) {
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;
    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;
    for(k1 = 2; k1 <= nf + 1; k1++) {
        ip = ifac[k1];
        l2 = ip * l1;
        ido = n / l2;
        idl1 = ido * l1;
        (void)idl1;
        switch(ip) {
        case 4:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            if(na == 0) passf4pos(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            else
                passf4pos(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            na = 1 - na;
            break;
        case 2:
            if(na == 0) passf2pos(ido, l1, (const complex_t*)c, ch, &wa[iw]);
            else
                passf2pos(ido, l1, (const complex_t*)ch, c, &wa[iw]);
            na = 1 - na;
            break;
        case 3:
            ix2 = iw + ido;
            if(na == 0) passf3(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            else
                passf3(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
            na = 1 - na;
            break;
        case 5:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            ix4 = ix3 + ido;
            if(na == 0) passf5(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            else
                passf5(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            na = 1 - na;
            break;
        }
        l1 = l2;
        iw += (ip - 1) * ido;
    }
    if(na == 0) return;
    for(i = 0; i < n; i++) {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void cfftf1neg(uint16_t n, complex_t* c, complex_t* ch, const uint16_t* ifac, const complex_t* wa, const int8_t isign) {
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;
    for(k1 = 2; k1 <= nf + 1; k1++) {
        ip = ifac[k1];
        l2 = ip * l1;
        ido = n / l2;
        idl1 = ido * l1;
        (void)idl1;
        switch(ip) {
        case 4:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            if(na == 0) passf4neg(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            else
                passf4neg(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            na = 1 - na;
            break;
        case 2:
            if(na == 0) passf2neg(ido, l1, (const complex_t*)c, ch, &wa[iw]);
            else
                passf2neg(ido, l1, (const complex_t*)ch, c, &wa[iw]);
            na = 1 - na;
            break;
        case 3:
            ix2 = iw + ido;
            if(na == 0) passf3(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            else
                passf3(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
            na = 1 - na;
            break;
        case 5:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            ix4 = ix3 + ido;
            if(na == 0) passf5(ido, l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            else
                passf5(ido, l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            na = 1 - na;
            break;
        }
        l1 = l2;
        iw += (ip - 1) * ido;
    }
    if(na == 0) return;
    for(i = 0; i < n; i++) {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cfftf(cfft_info_t* cfft, complex_t* c) { cfftf1neg(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, -1); }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cfftb(cfft_info_t* cfft, complex_t* c) { cfftf1pos(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, +1); }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void cffti1(uint16_t n, complex_t* wa, uint16_t* ifac) {
    const uint16_t ntryh[4] = {3, 4, 2, 5};
    uint16_t       ntry = 0, i, j;
    uint16_t       ib;
    uint16_t       nf, nl, nq, nr;

    nl = n;
    nf = 0;
    j = 0;
    (void)wa;

startloop:
    j++;
    if(j <= 4) ntry = ntryh[j - 1];
    else
        ntry += 2;
    do {
        nq = nl / ntry;
        nr = nl - ntry * nq;
        if(nr != 0) goto startloop;
        nf++;
        ifac[nf + 1] = ntry;
        nl = nq;
        if(ntry == 2 && nf != 1) {
            for(i = 2; i <= nf; i++) {
                ib = nf - i + 2;
                ifac[ib + 1] = ifac[ib];
            }
            ifac[2] = 2;
        }
    } while(nl != 1);
    ifac[0] = n;
    ifac[1] = nf;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
cfft_info_t* cffti(uint16_t n) {
    cfft_info_t* cfft = (cfft_info_t*)faad_calloc(1, sizeof(cfft_info_t));

    cfft->n = n;
    cfft->work = (complex_t*)faad_calloc(n, sizeof(complex_t));
    cffti1(n, NULL, cfft->ifac);
    switch(n) {
    case 64: cfft->tab = (complex_t*)cfft_tab_64; break;
    case 512: cfft->tab = (complex_t*)cfft_tab_512; break;
#ifdef LD_DEC
    case 256: cfft->tab = (complex_t*)cfft_tab_256; break;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    case 60: cfft->tab = (complex_t*)cfft_tab_60; break;
    case 480: cfft->tab = (complex_t*)cfft_tab_480; break;
    #ifdef LD_DEC
    case 240: cfft->tab = (complex_t*)cfft_tab_240; break;
    #endif
#endif
    case 128: cfft->tab = (complex_t*)cfft_tab_128; break;
    }
    return cfft;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cfftu(cfft_info_t* cfft) {
    if(cfft->work) faad_free(cfft->work);
    if(cfft) faad_free(cfft);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t get_sr_index(const uint32_t samplerate) { /* Returns the sample rate index based on the samplerate */
    if(92017 <= samplerate) return 0;
    if(75132 <= samplerate) return 1;
    if(55426 <= samplerate) return 2;
    if(46009 <= samplerate) return 3;
    if(37566 <= samplerate) return 4;
    if(27713 <= samplerate) return 5;
    if(23004 <= samplerate) return 6;
    if(18783 <= samplerate) return 7;
    if(13856 <= samplerate) return 8;
    if(11502 <= samplerate) return 9;
    if(9391 <= samplerate) return 10;
    if(16428320 <= samplerate) return 11;
    return 11;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t get_sample_rate(const uint8_t sr_index) { /* Returns the sample rate based on the sample rate index */
    const uint32_t sample_rates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
    if(sr_index < 12) return sample_rates[sr_index];
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t max_pred_sfb(const uint8_t sr_index) {
    const uint8_t pred_sfb_max[] = {33, 33, 38, 40, 40, 40, 41, 41, 37, 37, 37, 34};
    if(sr_index < 12) return pred_sfb_max[sr_index];
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t max_tns_sfb(const uint8_t sr_index, const uint8_t object_type, const uint8_t is_short) {
    /* entry for each sampling rate
     * 1    Main/LC long window
     * 2    Main/LC short window
     * 3    SSR long window
     * 4    SSR short window
     */
    const uint8_t tns_sbf_max[][4] = {{31, 9, 28, 7},  /* 96000 */
                                      {31, 9, 28, 7},  /* 88200 */
                                      {34, 10, 27, 7}, /* 64000 */
                                      {40, 14, 26, 6}, /* 48000 */
                                      {42, 14, 26, 6}, /* 44100 */
                                      {51, 14, 26, 6}, /* 32000 */
                                      {46, 14, 29, 7}, /* 24000 */
                                      {46, 14, 29, 7}, /* 22050 */
                                      {42, 14, 23, 8}, /* 16000 */
                                      {42, 14, 23, 8}, /* 12000 */
                                      {42, 14, 23, 8}, /* 11025 */
                                      {39, 14, 19, 7}, /*  8000 */
                                      {39, 14, 19, 7}, /*  7350 */
                                      {0, 0, 0, 0},    {0, 0, 0, 0}, {0, 0, 0, 0}};
    uint8_t       i = 0;
    if(is_short) i++;
    if(object_type == SSR) i += 2;
    return tns_sbf_max[sr_index][i];
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int8_t can_decode_ot(const uint8_t object_type) { /* Returns 0 if an object type is decodable, otherwise returns -1 */
    switch(object_type) {
    case LC: return 0;
    case MAIN:
#ifdef MAIN_DEC
        return 0;
#else
        return -1;
#endif
    case SSR: return -1;

    case LTP:
#ifdef LTP_DEC
        return 0;
#else
        return -1;
#endif
#ifdef ERROR_RESILIENCE /* ER object types */
    case ER_LC: return 0;
    case ER_LTP:
    #ifdef LTP_DEC
        return 0;
    #else
        return -1;
    #endif
    case LD:
    #ifdef LD_DEC
        return 0;
    #else
        return -1;
    #endif
#endif
    }
    return -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef ESP32_
void* faad_malloc(size_t size) { return ps_malloc(size); }
void* faad_calloc(size_t a, size_t s) { return ps_calloc(a, s); }
#else
void* faad_malloc(size_t size) {return ps_malloc(size); }
void* faad_calloc(size_t a, size_t s) { return ps_calloc(a, s); }
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const uint8_t Parity[256] = { // parity
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

// uint32_t __r1 = 1;
// uint32_t __r2 = 1;

/*
 *  This is a simple random number generator with good quality for audio purposes.
 *  It consists of two polycounters with opposite rotation direction and different
 *  periods. The periods are coprime, so the total period is the product of both.
 *
 *     -------------------------------------------------------------------------------------------------
 * +-> |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0|
 * |   -------------------------------------------------------------------------------------------------
 * |                                                                          |  |  |  |     |        |
 * |                                                                          +--+--+--+-XOR-+--------+
 * |                                                                                      |
 * +--------------------------------------------------------------------------------------+
 *
 *     -------------------------------------------------------------------------------------------------
 *     |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0| <-+
 *     -------------------------------------------------------------------------------------------------   |
 *       |  |           |  |                                                                               |
 *       +--+----XOR----+--+                                                                               |
 *                |                                                                                        |
 *                +----------------------------------------------------------------------------------------+
 *
 *
 *  The first has an period of 3*5*17*257*65537, the second of 7*47*73*178481,
 *  which gives a period of 18.410.713.077.675.721.215. The result is the
 *  XORed values of both generators.
 */
uint32_t ne_rng(uint32_t* __r1, uint32_t* __r2) {
    uint32_t t1, t2, t3, t4;

    t3 = t1 = *__r1;
    t4 = t2 = *__r2; // Parity calculation is done via table lookup, this is also available
    t1 &= 0xF5;
    t2 >>= 25; // on CPUs without parity, can be implemented in C and avoid unpredictable
    t1 = Parity[t1];
    t2 &= 0x63; // jumps and slow rotate through the carry flag operations.
    t1 <<= 31;
    t2 = Parity[t2];
    return (*__r1 = (t3 >> 1) | t1) ^ (*__r2 = (t4 + t4) | t2);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t ones32(uint32_t x) {
    x -= ((x >> 1) & 0x55555555);
    x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
    x = (((x >> 4) + x) & 0x0f0f0f0f);
    x += (x >> 8);
    x += (x >> 16);
    return (x & 0x0000003f);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t ones64(uint64_t x){
    return ones32((uint32_t)x) + ones32(x >> 32);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t floor_log2(uint64_t x) {
#if 1
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);

    return (ones64(x) - 1);
#else
    uint32_t count = 0;

    while (x >>= 1)
        count++;

    return count;
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* returns position of first bit that is not 0 from msb, starting count at lsb */
uint32_t wl_min_lzc(uint32_t x) {
#if 1
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);

    return (ones32(x));
#else
    uint32_t count = 0;

    while (x >>= 1)
        count++;

    return (count + 1);
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t pow2_fix(int32_t val) {
    uint32_t x1, x2;
    uint32_t errcorr;
    uint32_t index_frac;
    int32_t  retval;
    int32_t  whole = (val >> REAL_BITS);
    int32_t  rest = val - (whole << REAL_BITS);        /* rest = [0..1] */
    int32_t  index = rest >> (REAL_BITS - TABLE_BITS); /* index into pow2_tab */

    if(val == 0) return (1 << REAL_BITS);
    /* leave INTERP_BITS bits */
    index_frac = rest >> (REAL_BITS - TABLE_BITS - INTERP_BITS);
    index_frac = index_frac & ((1 << INTERP_BITS) - 1);
    if(whole > 0) { retval = 1 << whole; }
    else { retval = REAL_CONST(1) >> -whole; }
    x1 = pow2_tab[index & ((1 << TABLE_BITS) - 1)];
    x2 = pow2_tab[(index & ((1 << TABLE_BITS) - 1)) + 1];
    errcorr = ((index_frac * (x2 - x1))) >> INTERP_BITS;
    if(whole > 0) { retval = retval * (errcorr + x1); }
    else { retval = MUL_R(retval, (errcorr + x1)); }
    return retval;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t pow2_int(int32_t val) {

    uint32_t x1, x2;
    uint32_t errcorr;
    uint32_t index_frac;
    uint64_t retval;
    int32_t whole = (val >> REAL_BITS);
    int32_t exp = 0;

    /* rest = [0..1] */
    int32_t rest = val & ((1 << REAL_BITS) - 1);

    /* index into pow2_tab */
    int32_t index = rest >> (REAL_BITS-TABLE_BITS);

    if (val < 0)
        return 0;
    if (val == 0)
        return 1;
    if (whole > COEF_BITS) {
        exp = whole - COEF_BITS;
        whole = COEF_BITS;
    }

    /* leave INTERP_BITS bits */
    index_frac = rest >> (REAL_BITS-TABLE_BITS-INTERP_BITS);
    index_frac = index_frac & ((1<<INTERP_BITS)-1);

    if (whole >= 0)
        retval = (uint32_t)(1 << whole);
    else
        retval = 0;

    x1 = pow2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = pow2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];
    errcorr = ((index_frac*(x2-x1))) >> INTERP_BITS;

    retval = MUL_R(retval, (errcorr + x1));

    return retval << exp;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t log2_int(uint32_t val) {
    uint32_t frac;
    int32_t exp = 0;
    uint32_t index;
    uint32_t index_frac;
    uint32_t x1, x2;
    uint32_t errcorr;

    exp = floor_log2(val);
    exp -= REAL_BITS;

    /* frac = [1..2] */
    if (exp >= 0)
        frac = (uint32_t)(val >> exp);
    else
        frac = (uint32_t)(val << -exp);

    /* index in the log2 table */
    index = frac >> (REAL_BITS-TABLE_BITS);

    /* leftover part for linear interpolation */
    index_frac = frac & ((1<<(REAL_BITS-TABLE_BITS))-1);

    /* leave INTERP_BITS bits */
    index_frac = index_frac >> (REAL_BITS-TABLE_BITS-INTERP_BITS);

    x1 = log2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = log2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];

    /* linear interpolation */
    /* retval = exp + ((index_frac)*x2 + (1-index_frac)*x1) */

    errcorr = (index_frac * (x2-x1)) >> INTERP_BITS;
    return ((exp+REAL_BITS) << REAL_BITS) + errcorr + x1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t log2_fix(uint32_t val) {
    uint32_t frac;
    uint32_t whole = (val >> REAL_BITS);
    int8_t   exp = 0;
    uint32_t index;
    uint32_t index_frac;
    uint32_t x1, x2;
    uint32_t errcorr;

    (void)whole;
    if(val == 0) return -100000; /* error */
    exp = floor_log2(val);
    exp -= REAL_BITS;
    if(exp >= 0) frac = val >> exp; /* frac = [1..2] */
    else { frac = val << -exp; }
    index = frac >> (REAL_BITS - TABLE_BITS);                          /* index in the log2 table */
    index_frac = frac & ((1 << (REAL_BITS - TABLE_BITS)) - 1);         /* leftover part for linear interpolation */
    index_frac = index_frac >> (REAL_BITS - TABLE_BITS - INTERP_BITS); /* leave INTERP_BITS bits */
    x1 = log2_tab[index & ((1 << TABLE_BITS) - 1)];
    x2 = log2_tab[(index & ((1 << TABLE_BITS) - 1)) + 1];
    errcorr = (index_frac * (x2 - x1)) >> INTERP_BITS; /* linear interpolation */
    return (exp << REAL_BITS) + errcorr + x1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* TNS decoding for one channel and frame */
void tns_decode_frame(ic_stream_t* ics, tns_info_t* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len) {
    uint8_t  w, f, tns_order;
    int8_t   inc;
    int16_t  size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len / 8;
    int32_t  lpc[TNS_MAX_ORDER + 1];
    uint8_t  exp;

    if(!ics->tns_data_present) return;
    for(w = 0; w < ics->num_windows; w++) {
        bottom = ics->num_swb;
        for(f = 0; f < tns->n_filt[w]; f++) {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if(!tns_order) continue;
            exp = tns_decode_coef(tns_order, tns->coef_res[w] + 3, tns->coef_compress[w][f], tns->coef[w][f], lpc);
            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = min(ics->swb_offset[start], ics->swb_offset_max);
            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = min(ics->swb_offset[end], ics->swb_offset_max);
            size = end - start;
            if(size <= 0) continue;
            if(tns->direction[w][f]) {
                inc = -1;
                start = end - 1;
            }
            else { inc = 1; }
            tns_ar_filter(&spec[(w * nshort) + start], size, inc, lpc, tns_order, exp);
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* TNS encoding for one channel and frame */
void tns_encode_frame(ic_stream_t* ics, tns_info_t* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len) {
    uint8_t  w, f, tns_order;
    int8_t   inc;
    int16_t  size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len / 8;
    int32_t  lpc[TNS_MAX_ORDER + 1];
    uint8_t exp;

    if(!ics->tns_data_present) return;
    for(w = 0; w < ics->num_windows; w++) {
        bottom = ics->num_swb;
        for(f = 0; f < tns->n_filt[w]; f++) {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if(!tns_order) continue;
            exp = tns_decode_coef(tns_order, tns->coef_res[w] + 3, tns->coef_compress[w][f], tns->coef[w][f], lpc);
            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = min(ics->swb_offset[start], ics->swb_offset_max);
            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = min(ics->swb_offset[end], ics->swb_offset_max);
            size = end - start;
            if(size <= 0) continue;
            if(tns->direction[w][f]) {
                inc = -1;
                start = end - 1;
            }
            else { inc = 1; }
            tns_ma_filter(&spec[(w * nshort) + start], size, inc, lpc, tns_order, exp);
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t* all_tns_coefs[] = {tns_coef_0_3, tns_coef_0_4, tns_coef_1_3, tns_coef_1_4};
/* Decoder transmitted coefficients for one TNS filter */
static uint8_t tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress, uint8_t* coef, int32_t* a) {

    uint8_t i, m;
    int32_t  tmp2[TNS_MAX_ORDER + 1], b[TNS_MAX_ORDER + 1];
    uint8_t table_index = 2 * (coef_compress != 0) + (coef_res_bits != 3);
    int32_t* tns_coef = all_tns_coefs[table_index];
    uint8_t exp = 0;

    /* Conversion to signed integer */
    for(i = 0; i < order; i++) tmp2[i] = tns_coef[coef[i]];

    /* Conversion to LPC coefficients */
    a[0] = COEF_CONST(1.0);
    for(m = 1; m <= order; m++) {
        a[m] = tmp2[m - 1];    /* changed */
        for(i = 1; i < m; i++) /* loop only while i<m */
            b[i] = a[i] + MUL_C(a[m], a[m - i]);

        for(i = 1; i < m; i++) /* loop only while i<m */
            a[i] = b[i];

#ifdef FIXED_POINT
        a[m] >>= exp;

        /* OK not to check after the last iteration. */
        if(m < order) {
            int32_t sum_abs = COEF_CONST(0.0);
            for(i = 1; i <= m; ++i) { sum_abs += (a[i] >= 0) ? a[i] : -a[i]; }
            /* Next iteration would turn sum to 2*sum + 1; maximal "coef" is 7.999 */
            if(sum_abs >= COEF_CONST(3.5)) {
                exp++;
                for(i = 1; i <= m; ++i) { a[i] >>= 1; }
            }
        }
#endif
    }
    return exp;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void tns_ar_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order, uint8_t exp) {
    /*
     - Simple all-pole filter of order "order" defined by y(n) = x(n) - lpc[1]*y(n-1) - ... - lpc[order]*y(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment to the next data sample is given by "inc"
    */

    uint8_t j;
    uint16_t i;
    /* state is stored as a double ringbuffer */
    int32_t state[2*TNS_MAX_ORDER] = {0};
    int8_t state_index = 0;
    int32_t mul = 1;

#ifdef FIXED_POINT
    if (exp >= 4)
        return;
    mul = 1 << exp;
#else
    (void)exp;
#endif

    for (i = 0; i < size; i++)
    {
        int32_t y = REAL_CONST(0.0);
        for (j = 0; j < order; j++)
            y += MUL_C(state[state_index+j], lpc[j+1]);
        y = *spectrum - (y * mul);

        /* double ringbuffer state */
        state_index--;
        if (state_index < 0)
            state_index = order-1;
        state[state_index] = state[state_index + order] = y;

        *spectrum = y;
        spectrum += inc;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void tns_ma_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order, uint8_t exp) {
    /*
     - Simple all-zero filter of order "order" defined by y(n) =  x(n) + a(2)*x(n-1) + ... + a(order+1)*x(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment to the next data sample is given by "inc"
    */

    uint8_t  j;
    uint16_t i;
    /* state is stored as a double ringbuffer */
    int32_t  state[2 * TNS_MAX_ORDER] = {0};
    int8_t  state_index = 0;
    int32_t mul = 1;

#ifdef FIXED_POINT
    if(exp >= 4) return;
    mul = 1 << exp;
#else
    (void)exp;
#endif

    for(i = 0; i < size; i++) {
        int32_t y = REAL_CONST(0.0);
        for(j = 0; j < order; j++) y += MUL_C(state[state_index + j], lpc[j + 1]);

        y = *spectrum + (y * mul);

        /* double ringbuffer state */
        state_index--;
        if(state_index < 0) state_index = order - 1;
        state[state_index] = state[state_index + order] = *spectrum;

        *spectrum = y;
        spectrum += inc;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t NeAACDecGetVersion(const char** faad_id_string, const char** faad_copyright_string) {
    static const char* libfaadName = "2.20.1";
    static const char* libCopyright = " Copyright 2002-2004: Ahead Software AG\n"
                                      " http://www.audiocoding.com\n"
                                      " bug tracking: https://sourceforge.net/p/faac/bugs/\n";

    if(faad_id_string) *faad_id_string = libfaadName;
    if(faad_copyright_string) *faad_copyright_string = libCopyright;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char* err_msg[] = {"No error",
                         "Gain control not yet implemented",
                         "Pulse coding not allowed in short blocks",
                         "Invalid huffman codebook",
                         "Scalefactor out of range",
                         "Unable to find ADTS syncword",
                         "Channel coupling not yet implemented",
                         "Channel configuration not allowed in error resilient frame",
                         "Bit error in error resilient scalefactor decoding",
                         "Error decoding huffman scalefactor (bitstream error)",
                         "Error decoding huffman codeword (bitstream error)",
                         "Non existent huffman codebook number found",
                         "Invalid number of channels",
                         "Maximum number of bitstream elements exceeded",
                         "Input data buffer too small",
                         "Array index out of range",
                         "Maximum number of scalefactor bands exceeded",
                         "Quantised value out of range",
                         "LTP lag out of range",
                         "Invalid SBR parameter decoded",
                         "SBR called without being initialised",
                         "Unexpected channel configuration change",
                         "Error in program_config_t_element",
                         "First SBR frame is not the same as first AAC frame",
                         "Unexpected fill element with SBR data",
                         "Not all elements were provided with SBR data",
                         "LTP decoding not available",
                         "Output data buffer too small",
                         "CRC error in DRM data",
                         "PNS not allowed in DRM data stream",
                         "No standard extension payload allowed in DRM",
                         "PCE shall be the first element in a frame",
                         "Bitstream value not allowed by specification",
                         "MAIN prediction not initialised"};

const char* NeAACDecGetErrorMessage(uint8_t errcode) {
    if(errcode >= NUM_ERROR_MESSAGES) return "unknown aac error";
    return err_msg[errcode];
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t NeAACDecGetCapabilities(void) {
    uint32_t cap = 0;
    cap += LC_DEC_CAP; /* can't do without it */
#ifdef MAIN_DEC
    cap += MAIN_DEC_CAP;
#endif
#ifdef LTP_DEC
    cap += LTP_DEC_CAP;
#endif
#ifdef LD_DEC
    cap += LD_DEC_CAP;
#endif
#ifdef ERROR_RESILIENCE
    cap += ERROR_RESILIENCE_CAP;
#endif
    cap += FIXED_POINT_CAP;
    return cap;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
NeAACDecHandle NeAACDecOpen(void) {
    uint8_t           i;
    NeAACDecStruct_t* hDecoder = NULL;
    if (!alloc_mem()) { printf(ANSI_ESC_RED "libfaad2 ERROR: out of memory" ANSI_ESC_WHITE "\n"); return NULL;}
    if((hDecoder = (NeAACDecStruct_t*)faad_malloc(sizeof(NeAACDecStruct_t))) == NULL) return NULL;
    memset(hDecoder, 0, sizeof(NeAACDecStruct_t));
    hDecoder->cmes = mes;
    hDecoder->config.outputFormat = FAAD_FMT_16BIT;
    hDecoder->config.defObjectType = MAIN;
    hDecoder->config.defSampleRate = 44100; /* Default: 44.1kHz */
    hDecoder->config.downMatrix = 0;
    hDecoder->adts_header_t_present = 0;
    hDecoder->adif_header_t_present = 0;
    hDecoder->latm_header_t_present = 0;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = 0;
    hDecoder->aacScalefactorDataResilienceFlag = 0;
    hDecoder->aacSpectralDataResilienceFlag = 0;
#endif
    hDecoder->frameLength = 1024;
    hDecoder->frame = 0;
    hDecoder->sample_buffer = NULL;
    hDecoder->__r1 = 1;
    hDecoder->__r2 = 1;
    for(i = 0; i < MAX_CHANNELS; i++) {
        hDecoder->element_id[i] = INVALID_ELEMENT_ID;
        hDecoder->window_shape_prev[i] = 0;
        hDecoder->time_out[i] = NULL;
        hDecoder->fb_intermed[i] = NULL;
#ifdef MAIN_DEC
        hDecoder->pred_stat[i] = NULL;
#endif
#ifdef LTP_DEC
        hDecoder->ltp_lag[i] = 0;
        hDecoder->lt_pred_stat[i] = NULL;
#endif
    }
#ifdef SBR_DEC
    for(i = 0; i < MAX_SYNTAX_ELEMENTS; i++) { hDecoder->sbr[i] = NULL; }
#endif
    hDecoder->drc = drc_init(REAL_CONST(1.0), REAL_CONST(1.0));
    return hDecoder;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
NeAACDecConfigurationPtr_t NeAACDecGetCurrentConfiguration(NeAACDecHandle hpDecoder) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;
    if(hDecoder) {
        NeAACDecConfigurationPtr_t config = &(hDecoder->config);
        return config;
    }
    return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr_t config) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;

    if(hDecoder && config) {
        if(can_decode_ot(config->defObjectType) < 0) { return 0; } /* check if we can decode this object type */
        hDecoder->config.defObjectType = config->defObjectType;
        if(config->defSampleRate == 0) { return 0; } /* samplerate: anything but 0 should be possible */
        hDecoder->config.defSampleRate = config->defSampleRate;
        if((config->outputFormat < 1) || (config->outputFormat > 4)) { return 0; } /* check output format */
        hDecoder->config.outputFormat = config->outputFormat;
        if(config->downMatrix > 1) { return 0; }
        hDecoder->config.downMatrix = config->downMatrix;
        return 1; /* OK */
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
long NeAACDecInit(NeAACDecHandle hpDecoder, uint8_t* buffer, uint32_t buffer_size, uint32_t* samplerate, uint8_t* channels) {
    uint32_t          bits = 0;
    int32_t           ret = 0;
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;

    adif_header_t* adif = (adif_header_t*)faad_malloc(1 * sizeof(adif_header_t));
    adts_header_t* adts = (adts_header_t*)faad_malloc(1 * sizeof(adts_header_t));
    bitfile_t*     ld = (bitfile_t*)faad_calloc(1, sizeof(bitfile_t));

    if((hDecoder == NULL) || (samplerate == NULL) || (channels == NULL) || (buffer_size == 0)) {
        ret = -1;
        goto exit;
    }
    adts->old_format = hDecoder->config.useOldADTSFormat;
    hDecoder->sf_index = get_sr_index(hDecoder->config.defSampleRate);
    hDecoder->object_type = hDecoder->config.defObjectType;
    *samplerate = get_sample_rate(hDecoder->sf_index);
    *channels = 1;
    if(buffer != NULL) {
        faad_initbits(ld, buffer, buffer_size);
        /* Check if an ADIF header is present */
        if((buffer[0] == 'A') && (buffer[1] == 'D') && (buffer[2] == 'I') && (buffer[3] == 'F')) {
            hDecoder->adif_header_t_present = 1;
            get_adif_header_t(adif, ld);
            faad_byte_align(ld);

            hDecoder->sf_index = adif->pce[0].sf_index;
            hDecoder->object_type = adif->pce[0].object_type + 1;
            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = adif->pce[0].channels;
            memcpy(&(hDecoder->pce), &(adif->pce[0]), sizeof(program_config_t));
            hDecoder->pce_set = 1;
            bits = bit2byte(faad_get_processed_bits(ld));
            /* Check if an ADTS header is present */
        }
        else if(adts_frame(adts, ld) == 0) {
            hDecoder->adts_header_t_present = 1;
            // adts->old_format = hDecoder->config.useOldADTSFormat;
            // adts_frame(adts, ld);
            hDecoder->sf_index = adts->sf_index;
            hDecoder->object_type = adts->profile + 1;
            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = (adts->channel_configuration > 6) ? 2 : adts->channel_configuration;
        }
        if(ld->error) {
            ret = -1;
            goto exit;
        }
    }
    if(!*samplerate) {
        ret = -1;
        goto exit;
    }
#if(defined(PS_DEC))
    if(*channels == 1) { /* check if we have a mono file */
        *channels = 2;   /* upMatrix to 2 channels for implicit signalling of PS */
    }
#endif
    hDecoder->channelConfiguration = *channels;
#ifdef SBR_DEC
    if(*samplerate <= 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0)) { /* implicit signalling */
        *samplerate *= 2;
        hDecoder->forceUpSampling = 1;
    }
    else if(*samplerate > 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0)) { hDecoder->downSampledSBR = 1; }
#endif
    hDecoder->fb = filter_bank_init(hDecoder->frameLength); /* must be done before frameLength is divided by 2 for LD */
#ifdef LD_DEC
    if(hDecoder->object_type == LD) hDecoder->frameLength >>= 1;
#endif
    if(can_decode_ot(hDecoder->object_type) < 0) {
        ret = -1;
        goto exit;
    }
    ret = bits;
    goto exit;

exit:
    if(ld) {
        free(ld);
        ld = NULL;
    }
    if(adts) {
        free(adts);
        adts = NULL;
    }
    if(adif) {
        free(adif);
        adif = NULL;
    }
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Init the library using a DecoderSpecificInfo */
int8_t NeAACDecInit2(NeAACDecHandle hpDecoder, uint8_t* pBuffer, uint32_t SizeOfDecoderSpecificInfo, uint32_t* samplerate, uint8_t* channels) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;
    int8_t            rc;
    int8_t            ret = 0;

    if((hDecoder == NULL) || (pBuffer == NULL) || (SizeOfDecoderSpecificInfo < 2) || (samplerate == NULL) || (channels == NULL)) {
        ret = -1;
        goto exit;
    }
    hDecoder->adif_header_t_present = 0;
    hDecoder->adts_header_t_present = 0;
    rc = AudioSpecificConfig2(pBuffer, SizeOfDecoderSpecificInfo, &(hDecoder->pce), hDecoder->latm_header_t_present); /* decode the audio specific config */
    *samplerate = m_mp4ASC->samplingFrequency;                                                                                  /* copy the relevant info to the decoder handle */
    if(m_mp4ASC->channelsConfiguration) { *channels = m_mp4ASC->channelsConfiguration; }
    else {
        *channels = hDecoder->pce.channels;
        hDecoder->pce_set = 1;
    }
#if(defined(PS_DEC))
    if(*channels == 1) { /* check if we have a mono file */
        *channels = 2;   /* upMatrix to 2 channels for implicit signalling of PS */
    }
#endif
    hDecoder->sf_index = m_mp4ASC->samplingFrequencyIndex;
    hDecoder->object_type = m_mp4ASC->objectTypeIndex;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = m_mp4ASC->aacSectionDataResilienceFlag;
    hDecoder->aacScalefactorDataResilienceFlag = m_mp4ASC->aacScalefactorDataResilienceFlag;
    hDecoder->aacSpectralDataResilienceFlag = m_mp4ASC->aacSpectralDataResilienceFlag;
#endif
#ifdef SBR_DEC
    hDecoder->sbr_present_flag = m_mp4ASC->sbr_present_flag;
    hDecoder->downSampledSBR = m_mp4ASC->downSampledSBR;
    if(hDecoder->config.dontUpSampleImplicitSBR == 0) { hDecoder->forceUpSampling = m_mp4ASC->forceUpSampling; }
    else { hDecoder->forceUpSampling = 0; }
    /* AAC core decoder samplerate is 2 times as low */
    if(((hDecoder->sbr_present_flag == 1) && (!hDecoder->downSampledSBR)) || hDecoder->forceUpSampling == 1) { hDecoder->sf_index = get_sr_index(m_mp4ASC->samplingFrequency / 2); }
#endif
    if(rc != 0) {
        ret = rc;
        goto exit;
    }
    hDecoder->channelConfiguration = m_mp4ASC->channelsConfiguration;
    if(m_mp4ASC->frameLengthFlag)
#ifdef ALLOW_SMALL_FRAMELENGTH
        hDecoder->frameLength = 960;
#else
        ret = -1;
    goto exit;
#endif
    /* must be done before frameLength is divided by 2 for LD */
    hDecoder->fb = filter_bank_init(hDecoder->frameLength);
#ifdef LD_DEC
    if(hDecoder->object_type == LD) hDecoder->frameLength >>= 1;
#endif
    ret = 0;
    goto exit;

exit:
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void NeAACDecClose(NeAACDecHandle hpDecoder) {
    uint8_t           i;
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;

    if(hDecoder == NULL) return;
    for(i = 0; i < MAX_CHANNELS; i++) {
        if(hDecoder->time_out[i]) faad_free(hDecoder->time_out[i]);
        if(hDecoder->fb_intermed[i]) faad_free(hDecoder->fb_intermed[i]);
#ifdef MAIN_DEC
        if (hDecoder->pred_stat[i]) faad_free(hDecoder->pred_stat[i]);
#endif
#ifdef LTP_DEC
        if(hDecoder->lt_pred_stat[i]) faad_free(hDecoder->lt_pred_stat[i]);
#endif
    }
    filter_bank_end(hDecoder->fb);
    drc_end(hDecoder->drc);
    if(hDecoder->sample_buffer) faad_free(hDecoder->sample_buffer);
#ifdef SBR_DEC
    for(i = 0; i < MAX_SYNTAX_ELEMENTS; i++) {
        if(hDecoder->sbr[i]) sbrDecodeEnd(hDecoder->sbr[i]);
    }
#endif
    if(hDecoder) faad_free(hDecoder);
    free_mem();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void NeAACDecPostSeekReset(NeAACDecHandle hpDecoder, long frame) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;
    if(hDecoder) {
        hDecoder->postSeekResetFlag = 1;
        if(frame != -1) hDecoder->frame = frame;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void create_channel_config(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo) {
    hInfo->num_front_channels = 0;
    hInfo->num_side_channels = 0;
    hInfo->num_back_channels = 0;
    hInfo->num_lfe_channels = 0;
    memset(hInfo->channel_position, 0, MAX_CHANNELS * sizeof(uint8_t));

    if(hDecoder->downMatrix) {
        hInfo->num_front_channels = 2;
        hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
        hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
        return;
    }
    /* check if there is a PCE */
    if(hDecoder->pce_set) {
        uint8_t i, chpos = 0;
        uint8_t chdir, back_center = 0, total = 0;
        hInfo->num_front_channels = hDecoder->pce.num_front_channels;
        total += hInfo->num_front_channels;
        hInfo->num_side_channels = hDecoder->pce.num_side_channels;
        total += hInfo->num_side_channels;
        hInfo->num_back_channels = hDecoder->pce.num_back_channels;
        total += hInfo->num_back_channels;
        hInfo->num_lfe_channels = hDecoder->pce.num_lfe_channels;
        total += hInfo->num_lfe_channels;
        chdir = hInfo->num_front_channels;
        if(chdir & 1) {
#if(defined(PS_DEC))
            if(total == 1) {
                /* When PS is enabled output is always stereo */
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
            }
            else
#endif
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_CENTER;
            chdir--;
        }
        for(i = 0; i < chdir; i += 2) {
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
        }
        for(i = 0; i < hInfo->num_side_channels; i += 2) {
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_RIGHT;
        }
        chdir = hInfo->num_back_channels;
        if(chdir & 1) {
            back_center = 1;
            chdir--;
        }
        for(i = 0; i < chdir; i += 2) {
            hInfo->channel_position[chpos++] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = BACK_CHANNEL_RIGHT;
        }
        if(back_center) { hInfo->channel_position[chpos++] = BACK_CHANNEL_CENTER; }

        for(i = 0; i < hInfo->num_lfe_channels; i++) { hInfo->channel_position[chpos++] = LFE_CHANNEL; }
    }
    else {
        switch(hDecoder->channelConfiguration) {
        case 1:
#if(defined(PS_DEC))
            /* When PS is enabled output is always stereo */
            hInfo->num_front_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
#else
            hInfo->num_front_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
#endif
            break;
        case 2:
            hInfo->num_front_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
            break;
        case 3:
            hInfo->num_front_channels = 3;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            break;
        case 4:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_CENTER;
            break;
        case 5:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
            break;
        case 6:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 2;
            hInfo->num_lfe_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
            hInfo->channel_position[5] = LFE_CHANNEL;
            break;
        case 7:
            hInfo->num_front_channels = 3;
            hInfo->num_side_channels = 2;
            hInfo->num_back_channels = 2;
            hInfo->num_lfe_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = SIDE_CHANNEL_LEFT;
            hInfo->channel_position[4] = SIDE_CHANNEL_RIGHT;
            hInfo->channel_position[5] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[6] = BACK_CHANNEL_RIGHT;
            hInfo->channel_position[7] = LFE_CHANNEL;
            break;
        default: // channelConfiguration == 0 || channelConfiguration > 7
            uint8_t i;
            uint8_t ch = hDecoder->fr_channels - hDecoder->has_lfe;
            if(ch & 1) /* there's either a center front or a center back channel */
            {
                uint8_t ch1 = (ch - 1) / 2;
                if(hDecoder->first_syn_ele == ID_SCE) {
                    hInfo->num_front_channels = ch1 + 1;
                    hInfo->num_back_channels = ch1;
                    hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                    for(i = 1; i <= ch1; i += 2) {
                        hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                    }
                    for(i = ch1 + 1; i < ch; i += 2) {
                        hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                    }
                }
                else {
                    hInfo->num_front_channels = ch1;
                    hInfo->num_back_channels = ch1 + 1;
                    for(i = 0; i < ch1; i += 2) {
                        hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                    }
                    for(i = ch1; i < ch - 1; i += 2) {
                        hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                    }
                    hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
                }
            }
            else {
                uint8_t ch1 = (ch) / 2;
                hInfo->num_front_channels = ch1;
                hInfo->num_back_channels = ch1;
                if(ch1 & 1) {
                    hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                    for(i = 1; i <= ch1; i += 2) {
                        hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                    }
                    for(i = ch1 + 1; i < ch - 1; i += 2) {
                        hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                    }
                    hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
                }
                else {
                    for(i = 0; i < ch1; i += 2) {
                        hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                    }
                    for(i = ch1; i < ch; i += 2) {
                        hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                        hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                    }
                }
            }
            hInfo->num_lfe_channels = hDecoder->has_lfe;
            for(i = ch; i < hDecoder->fr_channels; i++) { hInfo->channel_position[i] = LFE_CHANNEL; }
            break;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void* NeAACDecDecode(NeAACDecHandle hpDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t* buffer, uint32_t buffer_size) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;
    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size, NULL, 0);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void* NeAACDecDecode2(NeAACDecHandle hpDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t* buffer, uint32_t buffer_size, void** sample_buffer, uint32_t sample_buffer_size) {
    NeAACDecStruct_t* hDecoder = (NeAACDecStruct_t*)hpDecoder;
    if((sample_buffer == NULL) || (sample_buffer_size == 0)) {
        hInfo->error = 27;
        return NULL;
    }
    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size, sample_buffer, sample_buffer_size);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void* aac_frame_decode(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t* buffer, uint32_t buffer_size, void** sample_buffer2, uint32_t sample_buffer_size) {
    uint16_t  i;
    uint8_t   channels = 0;
    uint8_t   output_channels = 0;
    bitfile_t ld = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // ⏫⏫⏫ size only 48
    uint32_t  bitsconsumed;
    uint16_t  frame_len;
    void*     sample_buffer;
    uint32_t  startbit = 0, endbit = 0, payload_bits = 0;

    (void)startbit;
    (void)endbit;
    (void)payload_bits;
    if((hDecoder == NULL) || (hInfo == NULL) || (buffer == NULL)) { return NULL; } /* safety checks */
    frame_len = hDecoder->frameLength;
    memset(hInfo, 0, sizeof(NeAACDecFrameInfo_t));
    memset(hDecoder->internal_channel, 0, MAX_CHANNELS * sizeof(hDecoder->internal_channel[0]));
    /* check for some common metadata tag types in the bitstream. No need to return an error */
    /* ID3 */
    if(buffer_size >= 128) {
        if(memcmp(buffer, "TAG", 3) == 0) {
            /* found it */
            hInfo->bytesconsumed = 128; /* 128 bytes fixed size */
            return NULL;                /* no error, but no output either */
        }
    }
    faad_initbits(&ld, buffer, buffer_size); /* initialize the bitstream */
    if(hDecoder->adts_header_t_present) {
        adts_header_t adts;
        adts.old_format = hDecoder->config.useOldADTSFormat;
        if((hInfo->error = adts_frame(&adts, &ld)) > 0) goto error;
        /* MPEG2 does byte_alignment() here, but ADTS header is always multiple of 8 bits in MPEG2 so not needed to actually do it. */
    }
    raw_data_block(hDecoder, hInfo, &ld, &hDecoder->pce, hDecoder->drc); /* decode the complete bitstream */
    channels = hDecoder->fr_channels;
    if(hInfo->error > 0) goto error;
    if(channels == 0 || channels > MAX_CHANNELS) { /* safety check */
        hInfo->error = 12;                         /* invalid number of channels */
        goto error;
    }
    bitsconsumed = faad_get_processed_bits(&ld); /* no more bit reading after this */
    hInfo->bytesconsumed = bit2byte(bitsconsumed);
    if(ld.error) {
        hInfo->error = 14;
        goto error;
    }
    if(!hDecoder->adts_header_t_present && !hDecoder->adif_header_t_present) {
        if(hDecoder->channelConfiguration == 0) hDecoder->channelConfiguration = channels;
        if(channels == 8) { hDecoder->channelConfiguration = 7; } /* 7.1 */
        if(channels == 7) { hDecoder->channelConfiguration = 0; } /* not a standard channelConfiguration */
    }
    if((channels == 5 || channels == 6) && hDecoder->config.downMatrix) {
        hDecoder->downMatrix = 1;
        output_channels = 2;
    }
    else { output_channels = channels; }
#if(defined(PS_DEC))
    hDecoder->upMatrix = 0;
    if(output_channels == 1) {  /* check if we have a mono file */
        hDecoder->upMatrix = 1; /* upMatrix to 2 channels for implicit signalling of PS */
        output_channels = 2;
    }
#endif
    create_channel_config(hDecoder, hInfo);                  /* Make a channel configuration based on either a PCE or a channelConfiguration */
    hInfo->samples = frame_len * output_channels;            /* number of samples in this frame */
    hInfo->channels = output_channels;                       /* number of channels in this frame */
    hInfo->samplerate = get_sample_rate(hDecoder->sf_index); /* samplerate */
    hInfo->object_type = hDecoder->object_type;              /* object type */
    hInfo->sbr = NO_SBR;                                     /* sbr */
    hInfo->header_type = RAW;                                /* header type */
    if(hDecoder->adif_header_t_present) hInfo->header_type = ADIF;
    if(hDecoder->adts_header_t_present) hInfo->header_type = ADTS;
#if(defined(PS_DEC))
    hInfo->ps = hDecoder->ps_used_global;
    hInfo->isPS = hDecoder->isPS;
#endif
    if(channels == 0) { /* check if frame has channel elements */
        hDecoder->frame++;
        return NULL;
    }
    /* allocate the buffer for the final samples */
    if((hDecoder->sample_buffer == NULL) || (hDecoder->alloced_channels != output_channels)) {
        static const uint8_t str[] = {sizeof(int16_t), sizeof(int32_t), sizeof(int32_t), sizeof(float), sizeof(double), sizeof(int16_t), sizeof(int16_t), sizeof(int16_t), sizeof(int16_t), 0, 0, 0};
        uint8_t              stride = str[hDecoder->config.outputFormat - 1];
#ifdef SBR_DEC
        if(((hDecoder->sbr_present_flag == 1) && (!hDecoder->downSampledSBR)) || (hDecoder->forceUpSampling == 1)) { stride = 2 * stride; }
#endif
        if(sample_buffer_size == 0) { /* check if we want to use internal sample_buffer */
            if(hDecoder->sample_buffer) faad_free(hDecoder->sample_buffer);
            hDecoder->sample_buffer = NULL;
            hDecoder->sample_buffer = faad_malloc(frame_len * output_channels * stride);
        }
        else if(sample_buffer_size < frame_len * output_channels * stride) {
            hInfo->error = 27; /* provided sample buffer is not big enough */
            return NULL;
        }
        hDecoder->alloced_channels = output_channels;
    }
    if(sample_buffer_size == 0) { sample_buffer = hDecoder->sample_buffer; }
    else { sample_buffer = *sample_buffer2; }
#ifdef SBR_DEC
    if((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
        uint8_t ele;
        if(!hDecoder->downSampledSBR) { /* this data is different when SBR is used or when the data is upsampled */
            frame_len *= 2;
            hInfo->samples *= 2;
            hInfo->samplerate *= 2;
        }
        for(ele = 0; ele < hDecoder->fr_ch_ele; ele++) { /* check if every element was provided with SBR data */
            if(hDecoder->sbr[ele] == NULL) {
                hInfo->error = 25;
                goto error;
            }
        }
        if(hDecoder->sbr_present_flag == 1) { /* sbr */
            hInfo->object_type = HE_AAC;
            hInfo->sbr = SBR_UPSAMPLED;
        }
        else { hInfo->sbr = NO_SBR_UPSAMPLED; }
        if(hDecoder->downSampledSBR) { hInfo->sbr = SBR_DOWNSAMPLED; }
    }
#endif
    sample_buffer = output_to_PCM(hDecoder, hDecoder->time_out, sample_buffer, output_channels, frame_len, hDecoder->config.outputFormat);
    hDecoder->postSeekResetFlag = 0;
    hDecoder->frame++;
#ifdef LD_DEC
    if(hDecoder->object_type != LD) {
#endif
        if(hDecoder->frame <= 1) hInfo->samples = 0;
#ifdef LD_DEC
    }
    else {
        /* LD encoders will give lower delay */
        if(hDecoder->frame <= 0) hInfo->samples = 0;
    }
#endif
    return sample_buffer;
error:
    /* reset filterbank state */
    for(i = 0; i < MAX_CHANNELS; i++) {
        if(hDecoder->fb_intermed[i] != NULL) { memset(hDecoder->fb_intermed[i], 0, hDecoder->frameLength * sizeof(int32_t)); }
    }
#ifdef SBR_DEC
    for(i = 0; i < MAX_SYNTAX_ELEMENTS; i++) {
        if(hDecoder->sbr[i] != NULL) { sbrReset(hDecoder->sbr[i]); }
    }
#endif
    return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
drc_info_t* drc_init(int32_t cut, int32_t boost) {
    drc_info_t* drc = (drc_info_t*)faad_malloc(sizeof(drc_info_t));
    memset(drc, 0, sizeof(drc_info_t));

    drc->ctrl1 = cut;
    drc->ctrl2 = boost;
    drc->num_bands = 1;
    drc->band_top[0] = 1024 / 4 - 1;
    drc->dyn_rng_sgn[0] = 1;
    drc->dyn_rng_ctl[0] = 0;
    return drc;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drc_end(drc_info_t* drc) {
    if(drc) faad_free(drc);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drc_decode(drc_info_t* drc, int32_t* spec) {
    uint16_t i, bd, top;
    int32_t  exp, frac;
    uint16_t bottom = 0;

    if(drc->num_bands == 1) drc->band_top[0] = 1024 / 4 - 1;
    for(bd = 0; bd < drc->num_bands; bd++) {
        top = 4 * (drc->band_top[bd] + 1);
        /* Decode DRC gain factor */
        if(drc->dyn_rng_sgn[bd]) /* compress */
        {
            exp = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24;
            frac = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        }
        else { /* boost */
            exp = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24;
            frac = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        }
        /* Apply gain factor */
        if(exp < 0) {
            for(i = bottom; i < top; i++) {
                spec[i] >>= -exp;
                if(frac) spec[i] = MUL_R(spec[i], drc_pow2_table[frac + 23]);
            }
        }
        else {
            for(i = bottom; i < top; i++) {
                spec[i] <<= exp;
                if(frac) spec[i] = MUL_R(spec[i], drc_pow2_table[frac + 23]);
            }
        }
        bottom = top;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
fb_info_t* filter_bank_init(uint16_t frame_len) {
    uint16_t nshort = frame_len / 8;
#ifdef LD_DEC
    uint16_t frame_len_ld = frame_len / 2;
#endif
    fb_info_t* fb = (fb_info_t*)faad_malloc(sizeof(fb_info_t));
    memset(fb, 0, sizeof(fb_info_t));

    /* normal */
    fb->mdct256 = faad_mdct_init(2 * nshort);
    fb->mdct2048 = faad_mdct_init(2 * frame_len);
#ifdef LD_DEC
    /* LD */
    fb->mdct1024 = faad_mdct_init(2 * frame_len_ld);
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    if(frame_len == 1024) {
#endif
        fb->long_window[0] = sine_long_1024;
        fb->short_window[0] = sine_short_128;
        fb->long_window[1] = kbd_long_1024;
        fb->short_window[1] = kbd_short_128;
#ifdef LD_DEC
        fb->ld_window[0] = sine_mid_512;
        fb->ld_window[1] = ld_mid_512;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    }
    else /* (frame_len == 960) */ {
        fb->long_window[0] = sine_long_960;
        fb->short_window[0] = sine_short_120;
        fb->long_window[1] = kbd_long_960;
        fb->short_window[1] = kbd_short_120;
    #ifdef LD_DEC
        fb->ld_window[0] = sine_mid_480;
        fb->ld_window[1] = ld_mid_480;
    #endif
    }
#endif
    return fb;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void filter_bank_end(fb_info_t* fb) {
    if(fb != NULL) {
        faad_mdct_end(fb->mdct256);
        faad_mdct_end(fb->mdct2048);
#ifdef LD_DEC
        faad_mdct_end(fb->mdct1024);
#endif
        faad_free(fb);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline void imdct_long(fb_info_t* fb, int32_t* in_data, int32_t* out_data, uint16_t len) {
    (void)len;
#ifdef LD_DEC
    mdct_info_t* mdct = NULL;

    switch(len) {
    case 2048:
    case 1920: mdct = fb->mdct2048; break;
    case 1024:
    case 960: mdct = fb->mdct1024; break;
    }
    faad_imdct(mdct, in_data, out_data);
#else
    faad_imdct(fb->mdct2048, in_data, out_data);
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline void mdct(fb_info_t* fb, int32_t* in_data, int32_t* out_data, uint16_t len) {
    mdct_info_t* mdct = NULL;

    switch(len) {
    case 2048:
    case 1920: mdct = fb->mdct2048; break;
    case 256:
    case 240: mdct = fb->mdct256; break;
#ifdef LD_DEC
    case 1024:
    case 960: mdct = fb->mdct1024; break;
#endif
    }
    faad_mdct(mdct, in_data, out_data);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ifilter_bank(fb_info_t* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, int32_t* overlap, uint8_t object_type,
                  uint16_t frame_len) {
    (void)object_type;
    int16_t        i;
    const int32_t* window_long = NULL;
    const int32_t* window_long_prev = NULL;
    const int32_t* window_short = NULL;
    const int32_t* window_short_prev = NULL;
    uint16_t       nlong = frame_len;
    uint16_t       nshort = frame_len / 8;
    uint16_t       trans = nshort / 2;
    uint16_t       nflat_ls = (nlong - nshort) / 2;

    memset(m_transf_buf, 0, 2 * 1024 * sizeof(int32_t));
#ifdef LD_DEC /* select windows of current frame and previous frame (Sine or KBD) */
    if(object_type == LD) {
        window_long = fb->ld_window[window_shape];
        window_long_prev = fb->ld_window[window_shape_prev];
    }
    else {
#endif
        window_long = fb->long_window[window_shape];
        window_long_prev = fb->long_window[window_shape_prev];
        window_short = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif
    switch(window_sequence) {
    case ONLY_LONG_SEQUENCE:
        /* perform iMDCT */
        imdct_long(fb, freq_in, m_transf_buf, 2 * nlong);
        /* add second half output of previous frame to windowed output of current frame */
        for(i = 0; i < nlong; i += 4) {
            time_out[i] = overlap[i] + MUL_F(m_transf_buf[i], window_long_prev[i]);
            time_out[i + 1] = overlap[i + 1] + MUL_F(m_transf_buf[i + 1], window_long_prev[i + 1]);
            time_out[i + 2] = overlap[i + 2] + MUL_F(m_transf_buf[i + 2], window_long_prev[i + 2]);
            time_out[i + 3] = overlap[i + 3] + MUL_F(m_transf_buf[i + 3], window_long_prev[i + 3]);
        }
        /* window the second half and save as overlap for next frame */
        for(i = 0; i < nlong; i += 4) {
            overlap[i] = MUL_F(m_transf_buf[nlong + i], window_long[nlong - 1 - i]);
            overlap[i + 1] = MUL_F(m_transf_buf[nlong + i + 1], window_long[nlong - 2 - i]);
            overlap[i + 2] = MUL_F(m_transf_buf[nlong + i + 2], window_long[nlong - 3 - i]);
            overlap[i + 3] = MUL_F(m_transf_buf[nlong + i + 3], window_long[nlong - 4 - i]);
        }
        break;
    case LONG_START_SEQUENCE:
        /* perform iMDCT */
        imdct_long(fb, freq_in, m_transf_buf, 2 * nlong);
        /* add second half output of previous frame to windowed output of current frame */
        for(i = 0; i < nlong; i += 4) {
            time_out[i] = overlap[i] + MUL_F(m_transf_buf[i], window_long_prev[i]);
            time_out[i + 1] = overlap[i + 1] + MUL_F(m_transf_buf[i + 1], window_long_prev[i + 1]);
            time_out[i + 2] = overlap[i + 2] + MUL_F(m_transf_buf[i + 2], window_long_prev[i + 2]);
            time_out[i + 3] = overlap[i + 3] + MUL_F(m_transf_buf[i + 3], window_long_prev[i + 3]);
        }
        /* window the second half and save as overlap for next frame */
        /* construct second half window using padding with 1's and 0's */
        for(i = 0; i < nflat_ls; i++) overlap[i] = m_transf_buf[nlong + i];
        for(i = 0; i < nshort; i++) overlap[nflat_ls + i] = MUL_F(m_transf_buf[nlong + nflat_ls + i], window_short[nshort - i - 1]);
        for(i = 0; i < nflat_ls; i++) overlap[nflat_ls + nshort + i] = 0;
        break;
    case EIGHT_SHORT_SEQUENCE:
        /* perform iMDCT for each short block */
        faad_imdct(fb->mdct256, freq_in + 0 * nshort, m_transf_buf + 2 * nshort * 0);
        faad_imdct(fb->mdct256, freq_in + 1 * nshort, m_transf_buf + 2 * nshort * 1);
        faad_imdct(fb->mdct256, freq_in + 2 * nshort, m_transf_buf + 2 * nshort * 2);
        faad_imdct(fb->mdct256, freq_in + 3 * nshort, m_transf_buf + 2 * nshort * 3);
        faad_imdct(fb->mdct256, freq_in + 4 * nshort, m_transf_buf + 2 * nshort * 4);
        faad_imdct(fb->mdct256, freq_in + 5 * nshort, m_transf_buf + 2 * nshort * 5);
        faad_imdct(fb->mdct256, freq_in + 6 * nshort, m_transf_buf + 2 * nshort * 6);
        faad_imdct(fb->mdct256, freq_in + 7 * nshort, m_transf_buf + 2 * nshort * 7);
        /* add second half output of previous frame to windowed output of current frame */
        for(i = 0; i < nflat_ls; i++) time_out[i] = overlap[i];
        for(i = 0; i < nshort; i++) {
            time_out[nflat_ls + i] = overlap[nflat_ls + i] + MUL_F(m_transf_buf[nshort * 0 + i], window_short_prev[i]);
            time_out[nflat_ls + 1 * nshort + i] =
                overlap[nflat_ls + nshort * 1 + i] + MUL_F(m_transf_buf[nshort * 1 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 2 + i], window_short[i]);
            time_out[nflat_ls + 2 * nshort + i] =
                overlap[nflat_ls + nshort * 2 + i] + MUL_F(m_transf_buf[nshort * 3 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 4 + i], window_short[i]);
            time_out[nflat_ls + 3 * nshort + i] =
                overlap[nflat_ls + nshort * 3 + i] + MUL_F(m_transf_buf[nshort * 5 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 6 + i], window_short[i]);
            if(i < trans)
                time_out[nflat_ls + 4 * nshort + i] =
                    overlap[nflat_ls + nshort * 4 + i] + MUL_F(m_transf_buf[nshort * 7 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 8 + i], window_short[i]);
        }
        /* window the second half and save as overlap for next frame */
        for(i = 0; i < nshort; i++) {
            if(i >= trans) overlap[nflat_ls + 4 * nshort + i - nlong] = MUL_F(m_transf_buf[nshort * 7 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 8 + i], window_short[i]);
            overlap[nflat_ls + 5 * nshort + i - nlong] = MUL_F(m_transf_buf[nshort * 9 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 10 + i], window_short[i]);
            overlap[nflat_ls + 6 * nshort + i - nlong] = MUL_F(m_transf_buf[nshort * 11 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 12 + i], window_short[i]);
            overlap[nflat_ls + 7 * nshort + i - nlong] = MUL_F(m_transf_buf[nshort * 13 + i], window_short[nshort - 1 - i]) + MUL_F(m_transf_buf[nshort * 14 + i], window_short[i]);
            overlap[nflat_ls + 8 * nshort + i - nlong] = MUL_F(m_transf_buf[nshort * 15 + i], window_short[nshort - 1 - i]);
        }
        for(i = 0; i < nflat_ls; i++) overlap[nflat_ls + nshort + i] = 0;
        break;
    case LONG_STOP_SEQUENCE:
        /* perform iMDCT */
        imdct_long(fb, freq_in, m_transf_buf, 2 * nlong);
        /* add second half output of previous frame to windowed output of current frame */
        /* construct first half window using padding with 1's and 0's */
        for(i = 0; i < nflat_ls; i++) time_out[i] = overlap[i];
        for(i = 0; i < nshort; i++) time_out[nflat_ls + i] = overlap[nflat_ls + i] + MUL_F(m_transf_buf[nflat_ls + i], window_short_prev[i]);
        for(i = 0; i < nflat_ls; i++) time_out[nflat_ls + nshort + i] = overlap[nflat_ls + nshort + i] + m_transf_buf[nflat_ls + nshort + i];
        /* window the second half and save as overlap for next frame */
        for(i = 0; i < nlong; i++) overlap[i] = MUL_F(m_transf_buf[nlong + i], window_long[nlong - 1 - i]);
        break;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* only works for LTP -> no overlapping, no short blocks */
void filter_bank_ltp(fb_info_t* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* in_data, int32_t* out_mdct, uint8_t object_type, uint16_t frame_len) {
    (void)object_type;
    int16_t i;
    memset(m_windowed_buf, 0, 2 * 1024 * sizeof(int32_t));

    const int32_t* window_long = NULL;
    const int32_t* window_long_prev = NULL;
    const int32_t* window_short = NULL;
    const int32_t* window_short_prev = NULL;
    uint16_t       nlong = frame_len;
    uint16_t       nshort = frame_len / 8;
    uint16_t       nflat_ls = (nlong - nshort) / 2;
    assert(window_sequence != EIGHT_SHORT_SEQUENCE);

#ifdef LD_DEC
    if(object_type == LD) {
        window_long = fb->ld_window[window_shape];
        window_long_prev = fb->ld_window[window_shape_prev];
    }
    else {
#endif
        window_long = fb->long_window[window_shape];
        window_long_prev = fb->long_window[window_shape_prev];
        window_short = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif
    switch(window_sequence) {
    case ONLY_LONG_SEQUENCE:
        for(i = nlong - 1; i >= 0; i--) {
            m_windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
            m_windowed_buf[i + nlong] = MUL_F(in_data[i + nlong], window_long[nlong - 1 - i]);
        }
        mdct(fb, m_windowed_buf, out_mdct, 2 * nlong);
        break;
    case LONG_START_SEQUENCE:
        for(i = 0; i < nlong; i++) m_windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
        for(i = 0; i < nflat_ls; i++) m_windowed_buf[i + nlong] = in_data[i + nlong];
        for(i = 0; i < nshort; i++) m_windowed_buf[i + nlong + nflat_ls] = MUL_F(in_data[i + nlong + nflat_ls], window_short[nshort - 1 - i]);
        for(i = 0; i < nflat_ls; i++) m_windowed_buf[i + nlong + nflat_ls + nshort] = 0;
        mdct(fb, m_windowed_buf, out_mdct, 2 * nlong);
        break;
    case LONG_STOP_SEQUENCE:
        for(i = 0; i < nflat_ls; i++) m_windowed_buf[i] = 0;
        for(i = 0; i < nshort; i++) m_windowed_buf[i + nflat_ls] = MUL_F(in_data[i + nflat_ls], window_short_prev[i]);
        for(i = 0; i < nflat_ls; i++) m_windowed_buf[i + nflat_ls + nshort] = in_data[i + nflat_ls + nshort];
        for(i = 0; i < nlong; i++) m_windowed_buf[i + nlong] = MUL_F(in_data[i + nlong], window_long[nlong - 1 - i]);
        mdct(fb, m_windowed_buf, out_mdct, 2 * nlong);
        break;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int8_t huffman_scale_factor(bitfile_t* ld) {
    uint16_t offset = 0;

    while(hcb_sf[offset][1]) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb_sf[offset][b];
        if(offset > 240) {
            // printf(ANSI_ESC_RED"libfaad2 ERROR: offset into hcb_sf = %d >240!" ANSI_ESC_WHITE "\n", offset);
            return -1;
        }
    }
    return hcb_sf[offset][0];
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void huffman_sign_bits(bitfile_t* ld, int16_t* sp, uint8_t len) {
    uint8_t i;

    for(i = 0; i < len; i++) {
        if(sp[i]) {
            if(faad_get1bit(ld) & 1) { sp[i] = -sp[i]; }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_getescape(bitfile_t* ld, int16_t* sp) {
    uint8_t neg, i;
    int16_t j;
    int16_t off;
    int16_t x = *sp;

    if(x < 0) {
        if(x != -16) return 0;
        neg = 1;
    }
    else {
        if(x != 16) return 0;
        neg = 0;
    }
    for(i = 4; i < 16; i++) {
        if(faad_get1bit(ld) == 0) { break; }
    }
    if(i >= 16) return 10;
    off = (int16_t)faad_getbits(ld, i);
    j = off | (1 << i);
    if(neg) j = -j;
    *sp = j;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_2step_quad(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;

    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;
    if(extra_bits) {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb]);
    }
    else { faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits); }
    if(offset > hcb_2_quad_table_size[cb]) {
        // printf(ANSI_ESC_RED "libfaad2 ERROR: offset into hcb_2_quad_table = %d > %ld!" ANSI_ESC_WHITE "\n", offset, hcb_2_quad_table_size[cb]);
        return 10;
    }
    sp[0] = hcb_2_quad_table[cb][offset].x;
    sp[1] = hcb_2_quad_table[cb][offset].y;
    sp[2] = hcb_2_quad_table[cb][offset].v;
    sp[3] = hcb_2_quad_table[cb][offset].w;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint8_t err = huffman_2step_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);
    return err;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_2step_pair(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;

    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;
    if(extra_bits) {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb]);
    }
    else { faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits); }
    if(offset > hcb_2_pair_table_size[cb]) {
        // printf(ANSI_ESC_RED "libfaad2 ERROR: offset into hcb_2_pair_table = %d > %li!" ANSI_ESC_WHITE "\n", offset, hcb_2_pair_table_size[cb]);
        return 10;
    }
    sp[0] = hcb_2_pair_table[cb][offset].x;
    sp[1] = hcb_2_pair_table[cb][offset].y;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_2step_pair_sign(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint8_t err = huffman_2step_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);
    return err;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_binary_quad(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint16_t offset = 0;

    while(!hcb3[offset].is_leaf) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb3[offset].data[b];
    }
    if(offset > hcb_bin_table_size[cb]) {
        // printf(ANSI_ESC_RED "libfaad2 ERROR: offset into hcb_bin_table = %d > %li!" ANSI_ESC_WHITE "\n", offset, hcb_bin_table_size[cb]);
        return 10;
    }
    sp[0] = hcb3[offset].data[0];
    sp[1] = hcb3[offset].data[1];
    sp[2] = hcb3[offset].data[2];
    sp[3] = hcb3[offset].data[3];
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint8_t err = huffman_binary_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);
    return err;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_binary_pair(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint16_t offset = 0;

    while(!hcb_bin_table[cb][offset].is_leaf) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb_bin_table[cb][offset].data[b];
    }
    if(offset > hcb_bin_table_size[cb]) {
        // printf(ANSI_ESC_RED "libfaad2 ERROR: offset into hcb_bin_table = %d > %li!" ANSI_ESC_WHITE "\n", offset, hcb_bin_table_size[cb]);
        return 10;
    }
    sp[0] = hcb_bin_table[cb][offset].data[0];
    sp[1] = hcb_bin_table[cb][offset].data[1];
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    uint8_t err = huffman_binary_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);
    return err;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int16_t huffman_codebook(uint8_t i) {
    static const uint32_t data = 16428320;
    if(i == 0) return (int16_t)(data >> 16) & 0xFFFF;
    else
        return (int16_t)data & 0xFFFF;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void vcb11_check_LAV(uint8_t cb, int16_t* sp) {
    (void)vcb11_check_LAV; // -Wunused-function
    const uint16_t vcb11_LAV_tab[] = {16, 31, 47, 63, 95, 127, 159, 191, 223, 255, 319, 383, 511, 767, 1023, 2047};
    uint16_t       max = 0;

    if(cb < 16 || cb > 31) return;
    max = vcb11_LAV_tab[cb - 16];
    if((abs(sp[0]) > max) || (abs(sp[1]) > max)) {
        sp[0] = 0;
        sp[1] = 0;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t huffman_spectral_data(uint8_t cb, bitfile_t* ld, int16_t* sp) {
    switch(cb) {
    case 1: /* 2-step method for data quadruples */
    case 2: return huffman_2step_quad(cb, ld, sp);
    case 3: /* binary search for data quadruples */ return huffman_binary_quad_sign(cb, ld, sp);
    case 4: /* 2-step method for data quadruples */ return huffman_2step_quad_sign(cb, ld, sp);
    case 5: /* binary search for data pairs */ return huffman_binary_pair(cb, ld, sp);
    case 6: /* 2-step method for data pairs */ return huffman_2step_pair(cb, ld, sp);
    case 7: /* binary search for data pairs */
    case 9: return huffman_binary_pair_sign(cb, ld, sp);
    case 8: /* 2-step method for data pairs */
    case 10: return huffman_2step_pair_sign(cb, ld, sp);
    case 12: {
        uint8_t err = huffman_2step_pair(11, ld, sp);
        sp[0] = huffman_codebook(0);
        sp[1] = huffman_codebook(1);
        return err;
    }
    case 11: {
        uint8_t err = huffman_2step_pair_sign(11, ld, sp);
        if(!err) err = huffman_getescape(ld, &sp[0]);
        if(!err) err = huffman_getescape(ld, &sp[1]);
        return err;
    }
#ifdef ERROR_RESILIENCE
    /* VCB11 uses codebook 11 */
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31: {
        uint8_t err = huffman_2step_pair_sign(11, ld, sp);
        if(!err) err = huffman_getescape(ld, &sp[0]);
        if(!err) err = huffman_getescape(ld, &sp[1]);

        /* check LAV (Largest Absolute Value) */
        /* this finds errors in the ESCAPE signal */
        vcb11_check_LAV(cb, sp);

        return err;
    }
#endif
    default:
        /* Non existent codebook number, something went wrong */
        return 11;
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Special version of huffman_spectral_data. Will not read from a bitfile_t but a bits_t_t structure. Will keep track of the bits decoded and return
   the number of bits remaining. Do not read more than ld->len, return -1 if codeword would be longer */

int8_t huffman_spectral_data_2(uint8_t cb, bits_t_t* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;
    uint8_t  i, vcb11 = 0;

    switch(cb) {
    case 1: /* 2-step method for data quadruples */
    case 2:
    case 4:

        cw = showbits_hcr(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;

        if(extra_bits) {
            /* we know for sure it's more than hcbN[cb] bits long */
            if(flushbits_hcr(ld, hcbN[cb])) return -1;
            offset += (uint16_t)showbits_hcr(ld, extra_bits);
            if(flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb])) return -1;
        }
        else {
            if(flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits)) return -1;
        }
        sp[0] = hcb_2_quad_table[cb][offset].x;
        sp[1] = hcb_2_quad_table[cb][offset].y;
        sp[2] = hcb_2_quad_table[cb][offset].v;
        sp[3] = hcb_2_quad_table[cb][offset].w;
        break;
    case 6: /* 2-step method for data pairs */
    case 8:
    case 10:
    case 11:
    /* VCB11 uses codebook 11 */
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:

        if(cb >= 16) {
            /* store the virtual codebook */
            vcb11 = cb;
            cb = 11;
        }
        cw = showbits_hcr(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;
        if(extra_bits) {
            /* we know for sure it's more than hcbN[cb] bits long */
            if(flushbits_hcr(ld, hcbN[cb])) return -1;
            offset += (uint16_t)showbits_hcr(ld, extra_bits);
            if(flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb])) return -1;
        }
        else {
            if(flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits)) return -1;
        }
        sp[0] = hcb_2_pair_table[cb][offset].x;
        sp[1] = hcb_2_pair_table[cb][offset].y;
        break;
    case 3: /* binary search for data quadruples */
        while(!hcb3[offset].is_leaf) {
            uint8_t b;
            if(get1bit_hcr(ld, &b)) return -1;
            offset += hcb3[offset].data[b];
        }
        sp[0] = hcb3[offset].data[0];
        sp[1] = hcb3[offset].data[1];
        sp[2] = hcb3[offset].data[2];
        sp[3] = hcb3[offset].data[3];
        break;
    case 5: /* binary search for data pairs */
    case 7:
    case 9:
        while(!hcb_bin_table[cb][offset].is_leaf) {
            uint8_t b;
            if(get1bit_hcr(ld, &b)) return -1;
            offset += hcb_bin_table[cb][offset].data[b];
        }
        sp[0] = hcb_bin_table[cb][offset].data[0];
        sp[1] = hcb_bin_table[cb][offset].data[1];
        break;
    }
    /* decode sign bits */
    if(unsigned_cb[cb]) {
        for(i = 0; i < ((cb < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN); i++) {
            if(sp[i]) {
                uint8_t b;
                if(get1bit_hcr(ld, &b)) return -1;
                if(b != 0) { sp[i] = -sp[i]; }
            }
        }
    }
    /* decode huffman escape bits */
    if((cb == ESC_HCB) || (cb >= 16)) {
        uint8_t k;
        for(k = 0; k < 2; k++) {
            if((sp[k] == 16) || (sp[k] == -16)) {
                uint8_t  neg, i;
                int32_t  j;
                uint32_t off;
                neg = (sp[k] < 0) ? 1 : 0;
                for(i = 4;; i++) {
                    uint8_t b;
                    if(get1bit_hcr(ld, &b)) return -1;
                    if(b == 0) break;
                }
                if(getbits_hcr(ld, i, &off)) return -1;
                j = off + (1 << i);
                sp[k] = (int16_t)((neg) ? -j : j);
            }
        }
        if(vcb11 != 0) {
            /* check LAV (Largest Absolute Value) */
            /* this finds errors in the ESCAPE signal */
            vcb11_check_LAV(vcb11, sp);
        }
    }
    return ld->len;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void is_decode(ic_stream_t* ics, ic_stream_t* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len) {

    uint8_t g, sfb, b;
    uint16_t i;
    int32_t scale;
#ifdef FIXED_POINT
    int32_t exp, frac;
#endif
    uint16_t nshort = frame_len/8;
    uint8_t group = 0;

    for (g = 0; g < icsr->num_window_groups; g++) {
        /* Do intensity stereo decoding */
        for (b = 0; b < icsr->window_group_length[g]; b++) {
            for (sfb = 0; sfb < icsr->max_sfb; sfb++) {
                if (is_intensity(icsr, g, sfb)) {
                    int16_t scale_factor = icsr->scale_factors[g][sfb];
#ifdef MAIN_DEC
                    /* For scalefactor bands coded in intensity stereo the
                       corresponding predictors in the right channel are
                       switched to "off".
                     */
                    ics->pred.prediction_used[sfb] = 0;
                    icsr->pred.prediction_used[sfb] = 0;
#endif
#ifndef FIXED_POINT
                    scale_factor = min(max(scale_factor, -120), 120);
                    scale = (int32_t)pow(0.5, (0.25*scale_factor));
#else
                    scale_factor = min(max(scale_factor, -60), 60);
                    exp = scale_factor >> 2; /* exp is -15..15 */
                    frac = scale_factor & 3;
                    scale = pow05_table[frac];
                    exp += COEF_BITS - REAL_BITS; /* exp is -1..29 */
                    if (exp < 0)
                        scale <<= -exp;
                    else
                        scale >>= exp;
#endif
                    /* Scale from left to right channel,
                       do not touch left channel */
                    for (i = icsr->swb_offset[sfb]; i < min(icsr->swb_offset[sfb+1], ics->swb_offset_max); i++)
                    {
                        r_spec[(group*nshort)+i] = MUL_R(l_spec[(group*nshort)+i], scale);
                        if (is_intensity(icsr, g, sfb) != invert_intensity(ics, g, sfb))
                            r_spec[(group*nshort)+i] = -r_spec[(group*nshort)+i];
                    }
                }
            }
            group++;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* rewind and reverse - 32 bit version */
// static uint32_t rewrev_word(uint32_t v, const uint8_t len) {
//     /* 32 bit reverse */
//     v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]);
//     v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]);
//     v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]);
//     v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
//     v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);
//     /* shift off low bits */
//     v >>= (32 - len);
//     return v;
// }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* 64 bit version */
// static void rewrev_lword(uint32_t* hi, uint32_t* lo, const uint8_t len) {
//     if(len <= 32) {
//         *hi = 0;
//         *lo = rewrev_word(*lo, len);
//     }
//     else {
//         uint32_t t = *hi, v = *lo;
//         /* double 32 bit reverse */
//         v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]);
//         t = ((t >> S[0]) & B[0]) | ((t << S[0]) & ~B[0]);
//         v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]);
//         t = ((t >> S[1]) & B[1]) | ((t << S[1]) & ~B[1]);
//         v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]);
//         t = ((t >> S[2]) & B[2]) | ((t << S[2]) & ~B[2]);
//         v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
//         t = ((t >> S[3]) & B[3]) | ((t << S[3]) & ~B[3]);
//         v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);
//         t = ((t >> S[4]) & B[4]) | ((t << S[4]) & ~B[4]);
//         /* last 32<>32 bit swap is implicit below shift off low bits (this is really only one 64 bit shift) */
//         *lo = (t >> (64 - len)) | (v << (len - 32));
//         *hi = v >> (64 - len);
//     }
// }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t reverse_word(uint32_t v){
    v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]);
    v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]);
    v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]);
    v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
    v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);
    return v;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* bits_t_t version */
static void rewrev_bits(bits_t_t* bits) {
    // if(bits->len == 0) return;
    // rewrev_lword(&bits->bufb, &bits->bufa, bits->len);
    if (bits->len == 0) return;
    if (bits->len <= 32) {
        bits->bufb = 0;
        bits->bufa = reverse_word(bits->bufa) >> (32 - bits->len);
    } else {
        /* last 32<>32 bit swap via rename */
        uint32_t lo = reverse_word(bits->bufb);
        uint32_t hi = reverse_word(bits->bufa);

        if (bits->len == 64) {
            bits->bufb = hi;
            bits->bufa = lo;
        } else {
            /* shift off low bits (this is really only one 64 bit shift) */
            bits->bufb = hi >> (64 - bits->len);
            bits->bufa = (lo >> (64 - bits->len)) | (hi << (bits->len - 32));
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* merge bits of a to b */
static void concat_bits(bits_t_t* b, bits_t_t* a) {
    uint32_t bl, bh, al, ah;

    if(a->len == 0) return;
    /* addend becomes result */
    if (b->len == 0)
    {
        *b = *a;
        return;
    }
    al = a->bufa;
    ah = a->bufb;
    if(b->len > 32) {
        /* maskoff superfluous high b bits */
        bl = b->bufa;
        bh = b->bufb & ((1u << (b->len - 32)) - 1);
        /* left shift a b->len bits */
        ah = al << (b->len - 32);
        al = 0;
    }
    else if (b->len == 32) {
        bl = b->bufa;
        bh = 0;
        ah = al;
        al = 0;
    }
    else {
        bl = b->bufa & ((1u << (b->len)) - 1);
        bh = 0;
        ah = (ah << (b->len)) | (al >> (32 - b->len));
        al = al << b->len;
    }
    /* merge */
    b->bufa = bl | al;
    b->bufb = bh | ah;
    b->len += a->len;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t is_good_cb(uint8_t this_CB, uint8_t this_sec_CB) {
    /* only want spectral data CB's */
    if((this_sec_CB > ZERO_HCB && this_sec_CB <= ESC_HCB) || (this_sec_CB >= VCB11_FIRST && this_sec_CB <= VCB11_LAST)) {
        if(this_CB < ESC_HCB) {
            /* normal codebook pairs */
            return ((this_sec_CB == this_CB) || (this_sec_CB == this_CB + 1));
        }
        else {
            /* escape codebook */
            return (this_sec_CB == this_CB);
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void read_segment(bits_t_t* segment, uint8_t segwidth, bitfile_t* ld) {
    segment->len = segwidth;

    if(segwidth > 32) {
        segment->bufb = faad_getbits(ld, segwidth - 32);
        segment->bufa = faad_getbits(ld, 32);
    }
    else {
        segment->bufa = faad_getbits(ld, segwidth);
        segment->bufb = 0;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void fill_in_codeword(codeword_t* codeword, uint16_t index, uint16_t sp, uint8_t cb) {
    codeword[index].sp_offset = sp;
    codeword[index].cb = cb;
    codeword[index].decoded = 0;
    codeword[index].bits.len = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t reordered_spectral_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, int16_t* spectral_data) {
    uint16_t       PCWs_done;
    uint16_t       numberOfSegments, numberOfSets, numberOfCodewords;
    uint16_t       sp_offset[8];
    uint16_t       g, i, sortloop, set, bitsread;
    uint8_t        w_idx, sfb, this_CB, last_CB, this_sec_CB;
    const uint16_t nshort = hDecoder->frameLength / 8;
    const uint16_t sp_data_len = ics->length_of_reordered_spectral_data;
    const uint8_t* PreSortCb;

    /* no data (e.g. silence) */
    if(sp_data_len == 0) return 0;
    /* since there is spectral data, at least one codeword has nonzero length */
    if(ics->length_of_longest_codeword == 0) return 10;
    if(sp_data_len < ics->length_of_longest_codeword) return 10;
    codeword_t* m_codeword = (codeword_t*)faad_malloc(512 * sizeof(codeword_t));
    bits_t_t*   m_segment = (bits_t_t*)faad_malloc(512 * sizeof(bits_t_t));
    sp_offset[0] = 0;
    for(g = 1; g < ics->num_window_groups; g++) { sp_offset[g] = sp_offset[g - 1] + nshort * ics->window_group_length[g - 1]; }
    PCWs_done = 0;
    numberOfSegments = 0;
    numberOfCodewords = 0;
    bitsread = 0;
    if(hDecoder->aacSectionDataResilienceFlag) { /* VCB11 code books in use */
        PreSortCb = PreSortCB_ER;
        last_CB = NUM_CB_ER;
    }
    else {
        PreSortCb = PreSortCB_STD;
        last_CB = NUM_CB;
    }
    /* step 1: decode PCW's (set 0), and stuff data in easier-to-use format */
    for(sortloop = 0; sortloop < last_CB; sortloop++) {
        /* select codebook to process this pass */
        this_CB = PreSortCb[sortloop];
        /* loop over sfbs */
        for(sfb = 0; sfb < ics->max_sfb; sfb++) {
            /* loop over all in this sfb, 4 lines per loop */
            for(w_idx = 0; 4 * w_idx < (min(ics->swb_offset[sfb + 1], ics->swb_offset_max) - ics->swb_offset[sfb]); w_idx++) {
                for(g = 0; g < ics->num_window_groups; g++) {
                    for(i = 0; i < ics->num_sec[g]; i++) {
                        /* check whether sfb used here is the one we want to process */
                        if((ics->sect_start[g][i] <= sfb) && (ics->sect_end[g][i] > sfb)) {
                            /* check whether codebook used here is the one we want to process */
                            this_sec_CB = ics->sect_cb[g][i];
                            if(is_good_cb(this_CB, this_sec_CB)) {
                                /* precalculate some stuff */
                                uint16_t sect_sfb_size = ics->sect_sfb_offset[g][sfb + 1] - ics->sect_sfb_offset[g][sfb];
                                uint8_t  inc = (this_sec_CB < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN;
                                uint16_t group_cws_count = (4 * ics->window_group_length[g]) / inc;
                                uint8_t  segwidth = segmentWidth(this_sec_CB);
                                uint16_t cws;
                                /* read codewords until end of sfb or end of window group (shouldn't only 1 trigger?) */
                                for(cws = 0; (cws < group_cws_count) && ((cws + w_idx * group_cws_count) < sect_sfb_size); cws++) {
                                    uint16_t sp = sp_offset[g] + ics->sect_sfb_offset[g][sfb] + inc * (cws + w_idx * group_cws_count);
                                    /* read and decode PCW */
                                    if(!PCWs_done) {
                                        /* read in normal segments */
                                        if(bitsread + segwidth <= sp_data_len) {
                                            read_segment(&m_segment[numberOfSegments], segwidth, ld);
                                            bitsread += segwidth;
                                            huffman_spectral_data_2(this_sec_CB, &m_segment[numberOfSegments], &spectral_data[sp]);
                                            /* keep leftover bits */
                                            rewrev_bits(&m_segment[numberOfSegments]);
                                            numberOfSegments++;
                                        }
                                        else {
                                            /* remaining stuff after last m_segment, we unfortunately couldn't read
                                               this in earlier because it might not fit in 64 bits. since we already
                                               decoded (and removed) the PCW it is now should fit */
                                            if(bitsread < sp_data_len) {
                                                const uint8_t additional_bits = sp_data_len - bitsread;
                                                read_segment(&m_segment[numberOfSegments], additional_bits, ld);
                                                m_segment[numberOfSegments].len += m_segment[numberOfSegments - 1].len;
                                                if (m_segment[numberOfSegments].len > 64) return 10;
                                                rewrev_bits(&m_segment[numberOfSegments]);

                                                if(m_segment[numberOfSegments - 1].len > 32) {
                                                    m_segment[numberOfSegments - 1].bufb =
                                                        m_segment[numberOfSegments].bufb + showbits_hcr(&m_segment[numberOfSegments - 1], m_segment[numberOfSegments - 1].len - 32);
                                                    m_segment[numberOfSegments - 1].bufa = m_segment[numberOfSegments].bufa + showbits_hcr(&m_segment[numberOfSegments - 1], 32);
                                                }
                                                else {
                                                    m_segment[numberOfSegments - 1].bufa =
                                                        m_segment[numberOfSegments].bufa + showbits_hcr(&m_segment[numberOfSegments - 1], m_segment[numberOfSegments - 1].len);
                                                    m_segment[numberOfSegments - 1].bufb = m_segment[numberOfSegments].bufb;
                                                }
                                                m_segment[numberOfSegments - 1].len += additional_bits;
                                            }
                                            bitsread = sp_data_len;
                                            PCWs_done = 1;
                                            fill_in_codeword(m_codeword, 0, sp, this_sec_CB);
                                        }
                                    }
                                    else { fill_in_codeword(m_codeword, numberOfCodewords - numberOfSegments, sp, this_sec_CB); }
                                    numberOfCodewords++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if(numberOfSegments == 0) { return 10; }
    numberOfSets = numberOfCodewords / numberOfSegments;
    /* step 2: decode nonPCWs */
    for(set = 1; set <= numberOfSets; set++) {
        uint16_t trial;
        for(trial = 0; trial < numberOfSegments; trial++) {
            uint16_t codewordBase;
            for(codewordBase = 0; codewordBase < numberOfSegments; codewordBase++) {
                const uint16_t segment_idx = (trial + codewordBase) % numberOfSegments;
                const uint16_t codeword_idx = codewordBase + set * numberOfSegments - numberOfSegments;
                /* data up */
                if(codeword_idx >= numberOfCodewords - numberOfSegments) break;
                if(!m_codeword[codeword_idx].decoded && m_segment[segment_idx].len > 0) {
                    uint8_t tmplen;
                    if(m_codeword[codeword_idx].bits.len != 0) concat_bits(&m_segment[segment_idx], &m_codeword[codeword_idx].bits);
                    tmplen = m_segment[segment_idx].len;
                    if(huffman_spectral_data_2(m_codeword[codeword_idx].cb, &m_segment[segment_idx], &spectral_data[m_codeword[codeword_idx].sp_offset]) >= 0) { m_codeword[codeword_idx].decoded = 1; }
                    else {
                        m_codeword[codeword_idx].bits = m_segment[segment_idx];
                        m_codeword[codeword_idx].bits.len = tmplen;
                    }
                }
            }
        }
        for(i = 0; i < numberOfSegments; i++) rewrev_bits(&m_segment[i]);
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ms_decode(ic_stream_t* ics, ic_stream_t* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len) {
    uint8_t  g, b, sfb;
    uint8_t  group = 0;
    uint16_t nshort = frame_len / 8;
    uint16_t i, k;
    int32_t  tmp;

    if(ics->ms_mask_present >= 1) {
        for(g = 0; g < ics->num_window_groups; g++) {
            for(b = 0; b < ics->window_group_length[g]; b++) {
                for(sfb = 0; sfb < ics->max_sfb; sfb++) {
                    /* If intensity stereo coding or noise substitution is on for a particular scalefactor band, no M/S stereo decoding is carried out. */
                    if((ics->ms_used[g][sfb] || ics->ms_mask_present == 2) && !is_intensity(icsr, g, sfb) && !is_noise(ics, g, sfb)) {
                        for(i = ics->swb_offset[sfb]; i < min(ics->swb_offset[sfb + 1], ics->swb_offset_max); i++) {
                            k = (group * nshort) + i;
                            tmp = l_spec[k] - r_spec[k];
                            l_spec[k] = l_spec[k] + r_spec[k];
                            r_spec[k] = tmp;
                        }
                    }
                }
                group++;
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int32_t get_sample(int32_t** input, uint8_t channel, uint16_t sample, uint8_t down_matrix, uint8_t up_matrix, uint8_t* internal_channel) {
    if(up_matrix == 1) return input[internal_channel[0]][sample];

    if(!down_matrix) return input[internal_channel[channel]][sample];
    if(channel == 0) {
        int32_t C = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        int32_t L_S = MUL_F(input[internal_channel[3]][sample], RSQRT2);
        int32_t cum = input[internal_channel[1]][sample] + C + L_S;
        return MUL_F(cum, DM_MUL);
    }
    else {
        int32_t C = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        int32_t R_S = MUL_F(input[internal_channel[4]][sample], RSQRT2);
        int32_t cum = input[internal_channel[2]][sample] + C + R_S;
        return MUL_F(cum, DM_MUL);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void* output_to_PCM(NeAACDecStruct_t* hDecoder, int32_t** input, void* sample_buffer, uint8_t channels, uint16_t frame_len, uint8_t format) {
    uint8_t  ch;
    uint16_t i;
    int16_t* short_sample_buffer = (int16_t*)sample_buffer;
    int32_t* int_sample_buffer = (int32_t*)sample_buffer;

    /* Copy output to a standard PCM buffer */
    for(ch = 0; ch < channels; ch++) {
        switch(format) {
        case FAAD_FMT_16BIT:
            for(i = 0; i < frame_len; i++) {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                if(tmp >= 0) {
                    tmp += (1 << (REAL_BITS - 1));
                    if(tmp >= REAL_CONST(32767)) { tmp = REAL_CONST(32767); }
                }
                else {
                    tmp += -(1 << (REAL_BITS - 1));
                    if(tmp <= REAL_CONST(-32768)) { tmp = REAL_CONST(-32768); }
                }
                tmp >>= REAL_BITS;
                short_sample_buffer[(i * channels) + ch] = (int16_t)tmp;
            }
            break;
        case FAAD_FMT_24BIT:
            for(i = 0; i < frame_len; i++) {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                if(tmp >= 0) {
                    tmp += (1 << (REAL_BITS - 9));
                    tmp >>= (REAL_BITS - 8);
                    if(tmp >= 8388607) { tmp = 8388607; }
                }
                else {
                    tmp += -(1 << (REAL_BITS - 9));
                    tmp >>= (REAL_BITS - 8);
                    if(tmp <= -8388608) { tmp = -8388608; }
                }
                int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
            }
            break;
        case FAAD_FMT_32BIT:
            for(i = 0; i < frame_len; i++) {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                if(tmp >= 0) {
                    tmp += (1 << (16 - REAL_BITS - 1));
                    tmp <<= (16 - REAL_BITS);
                }
                else {
                    tmp += -(1 << (16 - REAL_BITS - 1));
                    tmp <<= (16 - REAL_BITS);
                }
                int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
            }
            break;
        case FAAD_FMT_FIXED:
            for(i = 0; i < frame_len; i++) {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
            }
            break;
        }
    }
    return sample_buffer;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* fixed point square root approximation */
/* !!!! ONLY WORKS FOR EVEN %REAL_BITS% !!!! */
int32_t fp_sqrt(int32_t value) {
    int32_t root = 0;

    // clang-format off
    if((0x40000000l >> 0)  + root <= value) { value -= (0x40000000l >> 0)  + root; root = (root >> 1) | (0x40000000l >> 0); }  else { root = root >> 1; }
    if((0x40000000l >> 2)  + root <= value) { value -= (0x40000000l >> 2)  + root; root = (root >> 1) | (0x40000000l >> 2); }  else { root = root >> 1; }
    if((0x40000000l >> 4)  + root <= value) { value -= (0x40000000l >> 4)  + root; root = (root >> 1) | (0x40000000l >> 4); }  else { root = root >> 1; }
    if((0x40000000l >> 6)  + root <= value) { value -= (0x40000000l >> 6)  + root; root = (root >> 1) | (0x40000000l >> 6); }  else { root = root >> 1; }
    if((0x40000000l >> 8)  + root <= value) { value -= (0x40000000l >> 8)  + root; root = (root >> 1) | (0x40000000l >> 8); }  else { root = root >> 1; }
    if((0x40000000l >> 10) + root <= value) { value -= (0x40000000l >> 10) + root; root = (root >> 1) | (0x40000000l >> 10); } else { root = root >> 1; }
    if((0x40000000l >> 12) + root <= value) { value -= (0x40000000l >> 12) + root; root = (root >> 1) | (0x40000000l >> 12); } else { root = root >> 1; }
    if((0x40000000l >> 14) + root <= value) { value -= (0x40000000l >> 14) + root; root = (root >> 1) | (0x40000000l >> 14); } else { root = root >> 1; }
    if((0x40000000l >> 16) + root <= value) { value -= (0x40000000l >> 16) + root; root = (root >> 1) | (0x40000000l >> 16); } else { root = root >> 1; }
    if((0x40000000l >> 18) + root <= value) { value -= (0x40000000l >> 18) + root; root = (root >> 1) | (0x40000000l >> 18); } else { root = root >> 1; }
    if((0x40000000l >> 20) + root <= value) { value -= (0x40000000l >> 20) + root; root = (root >> 1) | (0x40000000l >> 20); } else { root = root >> 1; }
    if((0x40000000l >> 22) + root <= value) { value -= (0x40000000l >> 22) + root; root = (root >> 1) | (0x40000000l >> 22); } else { root = root >> 1; }
    if((0x40000000l >> 24) + root <= value) { value -= (0x40000000l >> 24) + root; root = (root >> 1) | (0x40000000l >> 24); } else { root = root >> 1; }
    if((0x40000000l >> 26) + root <= value) { value -= (0x40000000l >> 26) + root; root = (root >> 1) | (0x40000000l >> 26); } else { root = root >> 1; }
    if((0x40000000l >> 28) + root <= value) { value -= (0x40000000l >> 28) + root; root = (root >> 1) | (0x40000000l >> 28); } else { root = root >> 1; }
    if((0x40000000l >> 30) + root <= value) { value -= (0x40000000l >> 30) + root; root = (root >> 1) | (0x40000000l >> 30); } else { root = root >> 1; }
    // clang-format on

    if(root < value) ++root;
    root <<= (REAL_BITS / 2);
    return root;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* The function gen_rand_vector(addr, size) generates a vector of length  <size> with signed random values of average energy MEAN_NRG per random
   value. A suitable random number generator can be realized using one multiplication/accumulation per random value.
*/
static inline void gen_rand_vector(int32_t* spec, int16_t scale_factor, uint16_t size, uint8_t sub, uint32_t* __r1, uint32_t* __r2) {

#ifndef FIXED_POINT
    uint16_t i;
    int32_t energy = 0.0;
    (void)sub;

    scale_factor = min(max(scale_factor, -120), 120);

    for (i = 0; i < size; i++) {
        int32_t tmp = (int32_t)(int32_t)ne_rng(__r1, __r2);
        spec[i] = tmp;
        energy += tmp*tmp;
    }

    if (energy > 0) {
        int32_t scale = (int32_t)1.0/(int32_t)sqrt(energy);
        scale *= (int32_t)pow(2.0, 0.25 * scale_factor);
        for (i = 0; i < size; i++){
            spec[i] *= scale;
        }
    }
#else

    uint16_t i;
    int32_t  energy = 0;

    scale_factor = min(max(scale_factor, -120), 120);

    for (i = 0; i < size; i++){
        int32_t tmp = (int32_t)ne_rng(__r1, __r2);
        spec[i] = tmp;
        energy += tmp*tmp;
    }

    if (energy > 0) {
        int32_t scale = (int32_t)1.0/(int32_t)sqrt(energy);
        scale *= (int32_t)pow(2.0, 0.25 * scale_factor);
        for (i = 0; i < size; i++){
            spec[i] *= scale;
        }
    }
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void pns_decode(ic_stream_t* ics_left, ic_stream_t* ics_right, int32_t* spec_left, int32_t* spec_right, uint16_t frame_len, uint8_t channel_pair, uint8_t object_type,
                /* RNG states */ uint32_t* __r1, uint32_t* __r2) {

    uint8_t g, sfb, b;
    uint16_t begin, end;

    uint8_t group = 0;
    uint16_t nshort = frame_len >> 3;

    uint8_t sub = 0;

#ifdef FIXED_POINT
    /* IMDCT scaling */
    if (object_type == LD){
        sub = 9 /*9*/;
    }
    else {
        if (ics_left->window_sequence == EIGHT_SHORT_SEQUENCE)
            sub = 7 /*7*/;
        else
            sub = 10 /*10*/;
    }
#else
    (void)object_type;
#endif
    for (g = 0; g < ics_left->num_window_groups; g++) {
        /* Do perceptual noise substitution decoding */
        for (b = 0; b < ics_left->window_group_length[g]; b++){
            uint16_t base = group * nshort;
            for (sfb = 0; sfb < ics_left->max_sfb; sfb++){
                uint32_t r1_dep = 0, r2_dep = 0;

                if (is_noise(ics_left, g, sfb)){
#ifdef LTP_DEC
                    /* Simultaneous use of LTP and PNS is not prevented in the
                       syntax. If both LTP, and PNS are enabled on the same
                       scalefactor band, PNS takes precedence, and no prediction
                       is applied to this band.
                    */
                    ics_left->ltp.long_used[sfb] = 0;
                    ics_left->ltp2.long_used[sfb] = 0;
#endif

#ifdef MAIN_DEC
                    /* For scalefactor bands coded using PNS the corresponding
                       predictors are switched to "off".
                    */
                    ics_left->pred.prediction_used[sfb] = 0;
#endif

                    begin = min(base + ics_left->swb_offset[sfb], ics_left->swb_offset_max);
                    end = min(base + ics_left->swb_offset[sfb+1], ics_left->swb_offset_max);

                    r1_dep = *__r1;
                    r2_dep = *__r2;

                    /* Generate random vector */
                    gen_rand_vector(&spec_left[begin],
                        ics_left->scale_factors[g][sfb], end - begin, sub, __r1, __r2);
                }

                /* From the spec:
                   If the same scalefactor band and group is coded by perceptual noise substitution in both channels of a channel pair, the correlation of
                   the noise signal can be controlled by means of the ms_used field: While the default noise generation process works independently for each channel
                   (separate generation of random vectors), the same random vector is used for both channels if ms_used[] is set for a particular scalefactor band
                   and group. In this case, no M/S stereo coding is carried out (because M/S stereo coding and noise substitution coding are mutually exclusive).
                   If the same scalefactor band and group is coded by perceptual noise substitution in only one channel of a channel pair the setting of ms_used[]
                   is not evaluated.
                */
                if ((ics_right != NULL)
                    && is_noise(ics_right, g, sfb)){
#ifdef LTP_DEC
                    /* See comment above. */
                    ics_right->ltp.long_used[sfb] = 0;
                    ics_right->ltp2.long_used[sfb] = 0;
#endif
#ifdef MAIN_DEC
                    /* See comment above. */
                    ics_right->pred.prediction_used[sfb] = 0;
#endif

                    if (channel_pair && is_noise(ics_left, g, sfb) &&
                        (((ics_left->ms_mask_present == 1) &&
                        (ics_left->ms_used[g][sfb])) ||
                        (ics_left->ms_mask_present == 2))){
                        /*uint16_t c;*/

                        begin = min(base + ics_right->swb_offset[sfb], ics_right->swb_offset_max);
                        end = min(base + ics_right->swb_offset[sfb+1], ics_right->swb_offset_max);

                        /* Generate random vector dependent on left channel*/
                        gen_rand_vector(&spec_right[begin],
                            ics_right->scale_factors[g][sfb], end - begin, sub, &r1_dep, &r2_dep);
                    }
                    else /*if (ics_left->ms_mask_present == 0)*/ {
                        begin = min(base + ics_right->swb_offset[sfb], ics_right->swb_offset_max);
                        end = min(base + ics_right->swb_offset[sfb+1], ics_right->swb_offset_max);

                        /* Generate random vector */
                        gen_rand_vector(&spec_right[begin],
                            ics_right->scale_factors[g][sfb], end - begin, sub, __r1, __r2);
                    }
                }
            } /* sfb */
            group++;
        } /* b */
    } /* g */

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static hyb_info_t* hybrid_init(uint8_t numTimeSlotsRate) {
    uint8_t     i;
    hyb_info_t* hyb = (hyb_info_t*)faad_malloc(sizeof(hyb_info_t));
    hyb->resolution34[0] = 12;
    hyb->resolution34[1] = 8;
    hyb->resolution34[2] = 4;
    hyb->resolution34[3] = 4;
    hyb->resolution34[4] = 4;
    hyb->resolution20[0] = 8;
    hyb->resolution20[1] = 2;
    hyb->resolution20[2] = 2;
    hyb->frame_len = numTimeSlotsRate;
    hyb->work = (complex_t*)faad_malloc((hyb->frame_len + 12) * sizeof(complex_t));
    memset(hyb->work, 0, (hyb->frame_len + 12) * sizeof(complex_t));

    hyb->buffer = (complex_t**)faad_malloc(5 * sizeof(complex_t*));
    for(i = 0; i < 5; i++) {
        hyb->buffer[i] = (complex_t*)faad_malloc(hyb->frame_len * sizeof(complex_t));
        memset(hyb->buffer[i], 0, hyb->frame_len * sizeof(complex_t));
    }
    hyb->temp = (complex_t**)faad_malloc(hyb->frame_len * sizeof(complex_t*));
    for(i = 0; i < hyb->frame_len; i++) { hyb->temp[i] = (complex_t*)faad_malloc(12 /*max*/ * sizeof(complex_t)); }
    return hyb;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void hybrid_free(hyb_info_t* hyb) {
    uint8_t i;

    if(!hyb) return;
    if(hyb->work) faad_free(hyb->work);
    for(i = 0; i < 5; i++) {
        if(hyb->buffer[i]) faad_free(hyb->buffer[i]);
    }
    if(hyb->buffer) faad_free(hyb->buffer);
    for(i = 0; i < hyb->frame_len; i++) {
        if(hyb->temp[i]) faad_free(hyb->temp[i]);
    }
    if(hyb->temp) faad_free(hyb->temp);
    faad_free(hyb);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* real filter, size 2 */
static void channel_filter2(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid) {
    uint8_t i;
    (void)hyb;

    for(i = 0; i < frame_len; i++) {
        int32_t r0 = MUL_F(filter[0], (QMF_RE(buffer[0 + i]) + QMF_RE(buffer[12 + i])));
        int32_t r1 = MUL_F(filter[1], (QMF_RE(buffer[1 + i]) + QMF_RE(buffer[11 + i])));
        int32_t r2 = MUL_F(filter[2], (QMF_RE(buffer[2 + i]) + QMF_RE(buffer[10 + i])));
        int32_t r3 = MUL_F(filter[3], (QMF_RE(buffer[3 + i]) + QMF_RE(buffer[9 + i])));
        int32_t r4 = MUL_F(filter[4], (QMF_RE(buffer[4 + i]) + QMF_RE(buffer[8 + i])));
        int32_t r5 = MUL_F(filter[5], (QMF_RE(buffer[5 + i]) + QMF_RE(buffer[7 + i])));
        int32_t r6 = MUL_F(filter[6], QMF_RE(buffer[6 + i]));
        int32_t i0 = MUL_F(filter[0], (QMF_IM(buffer[0 + i]) + QMF_IM(buffer[12 + i])));
        int32_t i1 = MUL_F(filter[1], (QMF_IM(buffer[1 + i]) + QMF_IM(buffer[11 + i])));
        int32_t i2 = MUL_F(filter[2], (QMF_IM(buffer[2 + i]) + QMF_IM(buffer[10 + i])));
        int32_t i3 = MUL_F(filter[3], (QMF_IM(buffer[3 + i]) + QMF_IM(buffer[9 + i])));
        int32_t i4 = MUL_F(filter[4], (QMF_IM(buffer[4 + i]) + QMF_IM(buffer[8 + i])));
        int32_t i5 = MUL_F(filter[5], (QMF_IM(buffer[5 + i]) + QMF_IM(buffer[7 + i])));
        int32_t i6 = MUL_F(filter[6], QMF_IM(buffer[6 + i]));
        /* q = 0 */
        QMF_RE(X_hybrid[i][0]) = r0 + r1 + r2 + r3 + r4 + r5 + r6;
        QMF_IM(X_hybrid[i][0]) = i0 + i1 + i2 + i3 + i4 + i5 + i6;
        /* q = 1 */
        QMF_RE(X_hybrid[i][1]) = r0 - r1 + r2 - r3 + r4 - r5 + r6;
        QMF_IM(X_hybrid[i][1]) = i0 - i1 + i2 - i3 + i4 - i5 + i6;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* complex filter, size 4 */
static void channel_filter4(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid) {
    uint8_t i;
    (void)hyb;
    int32_t input_re1[2], input_re2[2], input_im1[2], input_im2[2];

    for(i = 0; i < frame_len; i++) {
        input_re1[0] = -MUL_F(filter[2], (QMF_RE(buffer[i + 2]) + QMF_RE(buffer[i + 10]))) + MUL_F(filter[6], QMF_RE(buffer[i + 6]));
        input_re1[1] = MUL_F(FRAC_CONST(-0.70710678118655), (MUL_F(filter[1], (QMF_RE(buffer[i + 1]) + QMF_RE(buffer[i + 11]))) + MUL_F(filter[3], (QMF_RE(buffer[i + 3]) + QMF_RE(buffer[i + 9]))) -
                                                             MUL_F(filter[5], (QMF_RE(buffer[i + 5]) + QMF_RE(buffer[i + 7])))));
        input_im1[0] = MUL_F(filter[0], (QMF_IM(buffer[i + 0]) - QMF_IM(buffer[i + 12]))) - MUL_F(filter[4], (QMF_IM(buffer[i + 4]) - QMF_IM(buffer[i + 8])));
        input_im1[1] = MUL_F(FRAC_CONST(0.70710678118655), (MUL_F(filter[1], (QMF_IM(buffer[i + 1]) - QMF_IM(buffer[i + 11]))) - MUL_F(filter[3], (QMF_IM(buffer[i + 3]) - QMF_IM(buffer[i + 9]))) -
                                                            MUL_F(filter[5], (QMF_IM(buffer[i + 5]) - QMF_IM(buffer[i + 7])))));
        input_re2[0] = MUL_F(filter[0], (QMF_RE(buffer[i + 0]) - QMF_RE(buffer[i + 12]))) - MUL_F(filter[4], (QMF_RE(buffer[i + 4]) - QMF_RE(buffer[i + 8])));
        input_re2[1] = MUL_F(FRAC_CONST(0.70710678118655), (MUL_F(filter[1], (QMF_RE(buffer[i + 1]) - QMF_RE(buffer[i + 11]))) - MUL_F(filter[3], (QMF_RE(buffer[i + 3]) - QMF_RE(buffer[i + 9]))) -
                                                            MUL_F(filter[5], (QMF_RE(buffer[i + 5]) - QMF_RE(buffer[i + 7])))));
        input_im2[0] = -MUL_F(filter[2], (QMF_IM(buffer[i + 2]) + QMF_IM(buffer[i + 10]))) + MUL_F(filter[6], QMF_IM(buffer[i + 6]));
        input_im2[1] = MUL_F(FRAC_CONST(-0.70710678118655), (MUL_F(filter[1], (QMF_IM(buffer[i + 1]) + QMF_IM(buffer[i + 11]))) + MUL_F(filter[3], (QMF_IM(buffer[i + 3]) + QMF_IM(buffer[i + 9]))) -
                                                             MUL_F(filter[5], (QMF_IM(buffer[i + 5]) + QMF_IM(buffer[i + 7])))));
        /* q == 0 */
        QMF_RE(X_hybrid[i][0]) = input_re1[0] + input_re1[1] + input_im1[0] + input_im1[1];
        QMF_IM(X_hybrid[i][0]) = -input_re2[0] - input_re2[1] + input_im2[0] + input_im2[1];
        /* q == 1 */
        QMF_RE(X_hybrid[i][1]) = input_re1[0] - input_re1[1] - input_im1[0] + input_im1[1];
        QMF_IM(X_hybrid[i][1]) = input_re2[0] - input_re2[1] + input_im2[0] - input_im2[1];
        /* q == 2 */
        QMF_RE(X_hybrid[i][2]) = input_re1[0] - input_re1[1] + input_im1[0] - input_im1[1];
        QMF_IM(X_hybrid[i][2]) = -input_re2[0] + input_re2[1] + input_im2[0] - input_im2[1];
        /* q == 3 */
        QMF_RE(X_hybrid[i][3]) = input_re1[0] + input_re1[1] - input_im1[0] - input_im1[1];
        QMF_IM(X_hybrid[i][3]) = input_re2[0] + input_re2[1] + input_im2[0] + input_im2[1];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* complex filter, size 8 */
static void channel_filter8(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid) {
    uint8_t i, n;
    (void)hyb;
    int32_t input_re1[4], input_re2[4], input_im1[4], input_im2[4];
    int32_t x[4];

    for(i = 0; i < frame_len; i++) {
        input_re1[0] = MUL_F(filter[6], QMF_RE(buffer[6 + i]));
        input_re1[1] = MUL_F(filter[5], (QMF_RE(buffer[5 + i]) + QMF_RE(buffer[7 + i])));
        input_re1[2] = -MUL_F(filter[0], (QMF_RE(buffer[0 + i]) + QMF_RE(buffer[12 + i]))) + MUL_F(filter[4], (QMF_RE(buffer[4 + i]) + QMF_RE(buffer[8 + i])));
        input_re1[3] = -MUL_F(filter[1], (QMF_RE(buffer[1 + i]) + QMF_RE(buffer[11 + i]))) + MUL_F(filter[3], (QMF_RE(buffer[3 + i]) + QMF_RE(buffer[9 + i])));
        input_im1[0] = MUL_F(filter[5], (QMF_IM(buffer[7 + i]) - QMF_IM(buffer[5 + i])));
        input_im1[1] = MUL_F(filter[0], (QMF_IM(buffer[12 + i]) - QMF_IM(buffer[0 + i]))) + MUL_F(filter[4], (QMF_IM(buffer[8 + i]) - QMF_IM(buffer[4 + i])));
        input_im1[2] = MUL_F(filter[1], (QMF_IM(buffer[11 + i]) - QMF_IM(buffer[1 + i]))) + MUL_F(filter[3], (QMF_IM(buffer[9 + i]) - QMF_IM(buffer[3 + i])));
        input_im1[3] = MUL_F(filter[2], (QMF_IM(buffer[10 + i]) - QMF_IM(buffer[2 + i])));

        for(n = 0; n < 4; n++) { x[n] = input_re1[n] - input_im1[3 - n]; }
        DCT3_4_unscaled(x, x);
        QMF_RE(X_hybrid[i][7]) = x[0];
        QMF_RE(X_hybrid[i][5]) = x[2];
        QMF_RE(X_hybrid[i][3]) = x[3];
        QMF_RE(X_hybrid[i][1]) = x[1];
        for(n = 0; n < 4; n++) { x[n] = input_re1[n] + input_im1[3 - n]; }
        DCT3_4_unscaled(x, x);
        QMF_RE(X_hybrid[i][6]) = x[1];
        QMF_RE(X_hybrid[i][4]) = x[3];
        QMF_RE(X_hybrid[i][2]) = x[2];
        QMF_RE(X_hybrid[i][0]) = x[0];

        input_im2[0] = MUL_F(filter[6], QMF_IM(buffer[6 + i]));
        input_im2[1] = MUL_F(filter[5], (QMF_IM(buffer[5 + i]) + QMF_IM(buffer[7 + i])));
        input_im2[2] = -MUL_F(filter[0], (QMF_IM(buffer[0 + i]) + QMF_IM(buffer[12 + i]))) + MUL_F(filter[4], (QMF_IM(buffer[4 + i]) + QMF_IM(buffer[8 + i])));
        input_im2[3] = -MUL_F(filter[1], (QMF_IM(buffer[1 + i]) + QMF_IM(buffer[11 + i]))) + MUL_F(filter[3], (QMF_IM(buffer[3 + i]) + QMF_IM(buffer[9 + i])));
        input_re2[0] = MUL_F(filter[5], (QMF_RE(buffer[7 + i]) - QMF_RE(buffer[5 + i])));
        input_re2[1] = MUL_F(filter[0], (QMF_RE(buffer[12 + i]) - QMF_RE(buffer[0 + i]))) + MUL_F(filter[4], (QMF_RE(buffer[8 + i]) - QMF_RE(buffer[4 + i])));
        input_re2[2] = MUL_F(filter[1], (QMF_RE(buffer[11 + i]) - QMF_RE(buffer[1 + i]))) + MUL_F(filter[3], (QMF_RE(buffer[9 + i]) - QMF_RE(buffer[3 + i])));
        input_re2[3] = MUL_F(filter[2], (QMF_RE(buffer[10 + i]) - QMF_RE(buffer[2 + i])));

        for(n = 0; n < 4; n++) { x[n] = input_im2[n] + input_re2[3 - n]; }
        DCT3_4_unscaled(x, x);
        QMF_IM(X_hybrid[i][7]) = x[0];
        QMF_IM(X_hybrid[i][5]) = x[2];
        QMF_IM(X_hybrid[i][3]) = x[3];
        QMF_IM(X_hybrid[i][1]) = x[1];

        for(n = 0; n < 4; n++) { x[n] = input_im2[n] - input_re2[3 - n]; }
        DCT3_4_unscaled(x, x);
        QMF_IM(X_hybrid[i][6]) = x[1];
        QMF_IM(X_hybrid[i][4]) = x[3];
        QMF_IM(X_hybrid[i][2]) = x[2];
        QMF_IM(X_hybrid[i][0]) = x[0];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void inline DCT3_6_unscaled(int32_t* y, int32_t* x) {
    int32_t f0, f1, f2, f3, f4, f5, f6, f7;

    f0 = MUL_F(x[3], FRAC_CONST(0.70710678118655));
    f1 = x[0] + f0;
    f2 = x[0] - f0;
    f3 = MUL_F((x[1] - x[5]), FRAC_CONST(0.70710678118655));
    f4 = MUL_F(x[2], FRAC_CONST(0.86602540378444)) + MUL_F(x[4], FRAC_CONST(0.5));
    f5 = f4 - x[4];
    f6 = MUL_F(x[1], FRAC_CONST(0.96592582628907)) + MUL_F(x[5], FRAC_CONST(0.25881904510252));
    f7 = f6 - f3;
    y[0] = f1 + f6 + f4;
    y[1] = f2 + f3 - x[4];
    y[2] = f7 + f2 - f5;
    y[3] = f1 - f7 - f5;
    y[4] = f1 - f3 - x[4];
    y[5] = f2 - f6 + f4;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* complex filter, size 12 */
static void channel_filter12(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid) {
    uint8_t i, n;
    (void)hyb;
    int32_t input_re1[6], input_re2[6], input_im1[6], input_im2[6];
    int32_t out_re1[6], out_re2[6], out_im1[6], out_im2[6];

    for(i = 0; i < frame_len; i++) {
        for(n = 0; n < 6; n++) {
            if(n == 0) {
                input_re1[0] = MUL_F(QMF_RE(buffer[6 + i]), filter[6]);
                input_re2[0] = MUL_F(QMF_IM(buffer[6 + i]), filter[6]);
            }
            else {
                input_re1[6 - n] = MUL_F((QMF_RE(buffer[n + i]) + QMF_RE(buffer[12 - n + i])), filter[n]);
                input_re2[6 - n] = MUL_F((QMF_IM(buffer[n + i]) + QMF_IM(buffer[12 - n + i])), filter[n]);
            }
            input_im2[n] = MUL_F((QMF_RE(buffer[n + i]) - QMF_RE(buffer[12 - n + i])), filter[n]);
            input_im1[n] = MUL_F((QMF_IM(buffer[n + i]) - QMF_IM(buffer[12 - n + i])), filter[n]);
        }

        DCT3_6_unscaled(out_re1, input_re1);
        DCT3_6_unscaled(out_re2, input_re2);
        DCT3_6_unscaled(out_im1, input_im1);
        DCT3_6_unscaled(out_im2, input_im2);
        for(n = 0; n < 6; n += 2) {
            QMF_RE(X_hybrid[i][n]) = out_re1[n] - out_im1[n];
            QMF_IM(X_hybrid[i][n]) = out_re2[n] + out_im2[n];
            QMF_RE(X_hybrid[i][n + 1]) = out_re1[n + 1] + out_im1[n + 1];
            QMF_IM(X_hybrid[i][n + 1]) = out_re2[n + 1] - out_im2[n + 1];
            QMF_RE(X_hybrid[i][10 - n]) = out_re1[n + 1] - out_im1[n + 1];
            QMF_IM(X_hybrid[i][10 - n]) = out_re2[n + 1] + out_im2[n + 1];
            QMF_RE(X_hybrid[i][11 - n]) = out_re1[n] + out_im1[n];
            QMF_IM(X_hybrid[i][11 - n]) = out_re2[n] - out_im2[n];
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Hybrid analysis: further split up QMF subbands to improve frequency resolution */
static void hybrid_analysis(hyb_info_t* hyb, complex_t* X[64], complex_t* X_hybrid[32], uint8_t use34, uint8_t numTimeSlotsRate) {
    uint8_t  k, n, band;
    uint8_t  offset = 0;
    uint8_t  qmf_bands = (use34) ? 5 : 3;
    uint8_t* resolution = (use34) ? hyb->resolution34 : hyb->resolution20;

    for(band = 0; band < qmf_bands; band++) {
        /* build working buffer */
        memcpy(hyb->work, hyb->buffer[band], 12 * sizeof(complex_t));
        /* add new samples */
        for(n = 0; n < hyb->frame_len; n++) {
            QMF_RE(hyb->work[12 + n]) = QMF_RE(X[n + 6 /*delay*/][band]);
            QMF_IM(hyb->work[12 + n]) = QMF_IM(X[n + 6 /*delay*/][band]);
        }
        /* store samples */
        memcpy(hyb->buffer[band], hyb->work + hyb->frame_len, 12 * sizeof(complex_t));
        switch(resolution[band]) {
        case 2:
            /* Type B real filter, Q[p] = 2 */
            channel_filter2(hyb, hyb->frame_len, p2_13_20, hyb->work, hyb->temp);
            break;
        case 4:
            /* Type A complex filter, Q[p] = 4 */
            channel_filter4(hyb, hyb->frame_len, p4_13_34, hyb->work, hyb->temp);
            break;
        case 8:
            /* Type A complex filter, Q[p] = 8 */
            channel_filter8(hyb, hyb->frame_len, (use34) ? p8_13_34 : p8_13_20, hyb->work, hyb->temp);
            break;
        case 12:
            /* Type A complex filter, Q[p] = 12 */
            channel_filter12(hyb, hyb->frame_len, p12_13_34, hyb->work, hyb->temp);
            break;
        }
        for(n = 0; n < hyb->frame_len; n++) {
            for(k = 0; k < resolution[band]; k++) {
                QMF_RE(X_hybrid[n][offset + k]) = QMF_RE(hyb->temp[n][k]);
                QMF_IM(X_hybrid[n][offset + k]) = QMF_IM(hyb->temp[n][k]);
            }
        }
        offset += resolution[band];
    }
    /* group hybrid channels */
    if(!use34) {
        for(n = 0; n < numTimeSlotsRate; n++) {
            QMF_RE(X_hybrid[n][3]) += QMF_RE(X_hybrid[n][4]);
            QMF_IM(X_hybrid[n][3]) += QMF_IM(X_hybrid[n][4]);
            QMF_RE(X_hybrid[n][4]) = 0;
            QMF_IM(X_hybrid[n][4]) = 0;
            QMF_RE(X_hybrid[n][2]) += QMF_RE(X_hybrid[n][5]);
            QMF_IM(X_hybrid[n][2]) += QMF_IM(X_hybrid[n][5]);
            QMF_RE(X_hybrid[n][5]) = 0;
            QMF_IM(X_hybrid[n][5]) = 0;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void hybrid_synthesis(hyb_info_t* hyb, complex_t* X[64], complex_t* X_hybrid[32], uint8_t use34, uint8_t numTimeSlotsRate) {
    uint8_t k, n, band;
    (void)hyb;
    (void)numTimeSlotsRate;
    uint8_t  offset = 0;
    uint8_t  qmf_bands = (use34) ? 5 : 3;
    uint8_t* resolution = (use34) ? hyb->resolution34 : hyb->resolution20;

    for(band = 0; band < qmf_bands; band++) {
        for(n = 0; n < hyb->frame_len; n++) {
            QMF_RE(X[n][band]) = 0;
            QMF_IM(X[n][band]) = 0;
            for(k = 0; k < resolution[band]; k++) {
                QMF_RE(X[n][band]) += QMF_RE(X_hybrid[n][offset + k]);
                QMF_IM(X[n][band]) += QMF_IM(X_hybrid[n][offset + k]);
            }
        }
        offset += resolution[band];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* limits the value i to the range [min,max] */
static int8_t delta_clip(int8_t i, int8_t min, int8_t max) {
    if(i < min) return min;
    else if(i > max)
        return max;
    else
        return i;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* delta decode array */
static void delta_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t min_index, int8_t max_index) {
    int8_t i;

    if(enable == 1) {
        if(dt_flag == 0) {
            /* delta coded in frequency direction */
            index[0] = 0 + index[0];
            index[0] = delta_clip(index[0], min_index, max_index);
            for(i = 1; i < nr_par; i++) {
                index[i] = index[i - 1] + index[i];
                index[i] = delta_clip(index[i], min_index, max_index);
            }
        }
        else {
            /* delta coded in time direction */
            for(i = 0; i < nr_par; i++) {
                index[i] = index_prev[i * stride] + index[i];
                // tmp2 = index[i];
                index[i] = delta_clip(index[i], min_index, max_index);
            }
        }
    }
    else {
        for(i = 0; i < nr_par; i++) { index[i] = 0; } /* set indices to zero */
    }
    if(stride == 2) { /* coarse */
        for(i = (nr_par << 1) - 1; i > 0; i--) { index[i] = index[i >> 1]; }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* delta modulo decode array */
/* in: log2 value of the modulo value to allow using AND instead of MOD */
static void delta_modulo_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t and_modulo) {
    int8_t i;

    if(enable == 1) {
        if(dt_flag == 0) {
            /* delta coded in frequency direction */
            index[0] = 0 + index[0];
            index[0] &= and_modulo;
            for(i = 1; i < nr_par; i++) {
                index[i] = index[i - 1] + index[i];
                index[i] &= and_modulo;
            }
        }
        else {
            /* delta coded in time direction */
            for(i = 0; i < nr_par; i++) {
                index[i] = index_prev[i * stride] + index[i];
                index[i] &= and_modulo;
            }
        }
    }
    else {
        /* set indices to zero */
        for(i = 0; i < nr_par; i++) { index[i] = 0; }
    }
    /* coarse */
    if(stride == 2) {
        index[0] = 0;
        for(i = (nr_par << 1) - 1; i > 0; i--) { index[i] = index[i >> 1]; }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void map20indexto34(int8_t* index, uint8_t bins) {
    index[0] = index[0];
    index[1] = (index[0] + index[1]) / 2;
    index[2] = index[1];
    index[3] = index[2];
    index[4] = (index[2] + index[3]) / 2;
    index[5] = index[3];
    index[6] = index[4];
    index[7] = index[4];
    index[8] = index[5];
    index[9] = index[5];
    index[10] = index[6];
    index[11] = index[7];
    index[12] = index[8];
    index[13] = index[8];
    index[14] = index[9];
    index[15] = index[9];
    index[16] = index[10];

    if(bins == 34) {
        index[17] = index[11];
        index[18] = index[12];
        index[19] = index[13];
        index[20] = index[14];
        index[21] = index[14];
        index[22] = index[15];
        index[23] = index[15];
        index[24] = index[16];
        index[25] = index[16];
        index[26] = index[17];
        index[27] = index[17];
        index[28] = index[18];
        index[29] = index[18];
        index[30] = index[18];
        index[31] = index[18];
        index[32] = index[19];
        index[33] = index[19];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* parse the bitstream data decoded in ps_data() */
static void ps_data_decode(ps_info_t* ps) {
    uint8_t env, bin;

    /* ps data not available, use data from previous frame */
    if(ps->ps_data_available == 0) { ps->num_env = 0; }
    for(env = 0; env < ps->num_env; env++) {
        int8_t* iid_index_prev;
        int8_t* icc_index_prev;
        int8_t* ipd_index_prev;
        int8_t* opd_index_prev;
        int8_t  num_iid_steps = (ps->iid_mode < 3) ? 7 : 15 /*fine quant*/;
        if(env == 0) {
            /* take last envelope from previous frame */
            iid_index_prev = ps->iid_index_prev;
            icc_index_prev = ps->icc_index_prev;
            ipd_index_prev = ps->ipd_index_prev;
            opd_index_prev = ps->opd_index_prev;
        }
        else {
            /* take index values from previous envelope */
            iid_index_prev = ps->iid_index[env - 1];
            icc_index_prev = ps->icc_index[env - 1];
            ipd_index_prev = ps->ipd_index[env - 1];
            opd_index_prev = ps->opd_index[env - 1];
        }
        //        iid = 1;
        /* delta decode iid parameters */
        delta_decode(ps->enable_iid, ps->iid_index[env], iid_index_prev, ps->iid_dt[env], ps->nr_iid_par, (ps->iid_mode == 0 || ps->iid_mode == 3) ? 2 : 1, -num_iid_steps, num_iid_steps);
        //        iid = 0;
        /* delta decode icc parameters */
        delta_decode(ps->enable_icc, ps->icc_index[env], icc_index_prev, ps->icc_dt[env], ps->nr_icc_par, (ps->icc_mode == 0 || ps->icc_mode == 3) ? 2 : 1, 0, 7);
        /* delta modulo decode ipd parameters */
        delta_modulo_decode(ps->enable_ipdopd, ps->ipd_index[env], ipd_index_prev, ps->ipd_dt[env], ps->nr_ipdopd_par, 1, 7);
        /* delta modulo decode opd parameters */
        delta_modulo_decode(ps->enable_ipdopd, ps->opd_index[env], opd_index_prev, ps->opd_dt[env], ps->nr_ipdopd_par, 1, 7);
    }
    /* handle error case */
    if(ps->num_env == 0) {
        /* force to 1 */
        ps->num_env = 1;
        if(ps->enable_iid) {
            for(bin = 0; bin < 34; bin++) ps->iid_index[0][bin] = ps->iid_index_prev[bin];
        }
        else {
            for(bin = 0; bin < 34; bin++) ps->iid_index[0][bin] = 0;
        }
        if(ps->enable_icc) {
            for(bin = 0; bin < 34; bin++) ps->icc_index[0][bin] = ps->icc_index_prev[bin];
        }
        else {
            for(bin = 0; bin < 34; bin++) ps->icc_index[0][bin] = 0;
        }
        if(ps->enable_ipdopd) {
            for(bin = 0; bin < 17; bin++) {
                ps->ipd_index[0][bin] = ps->ipd_index_prev[bin];
                ps->opd_index[0][bin] = ps->opd_index_prev[bin];
            }
        }
        else {
            for(bin = 0; bin < 17; bin++) {
                ps->ipd_index[0][bin] = 0;
                ps->opd_index[0][bin] = 0;
            }
        }
    }
    /* update previous indices */
    for(bin = 0; bin < 34; bin++) ps->iid_index_prev[bin] = ps->iid_index[ps->num_env - 1][bin];
    for(bin = 0; bin < 34; bin++) ps->icc_index_prev[bin] = ps->icc_index[ps->num_env - 1][bin];
    for(bin = 0; bin < 17; bin++) {
        ps->ipd_index_prev[bin] = ps->ipd_index[ps->num_env - 1][bin];
        ps->opd_index_prev[bin] = ps->opd_index[ps->num_env - 1][bin];
    }
    ps->ps_data_available = 0;
    if(ps->frame_class == 0) {
        ps->border_position[0] = 0;
        for(env = 1; env < ps->num_env; env++) { ps->border_position[env] = (env * ps->numTimeSlotsRate) / ps->num_env; }
        ps->border_position[ps->num_env] = ps->numTimeSlotsRate;
    }
    else {
        ps->border_position[0] = 0;
        if(ps->border_position[ps->num_env] < ps->numTimeSlotsRate) {
            for(bin = 0; bin < 34; bin++) {
                ps->iid_index[ps->num_env][bin] = ps->iid_index[ps->num_env - 1][bin];
                ps->icc_index[ps->num_env][bin] = ps->icc_index[ps->num_env - 1][bin];
            }
            for(bin = 0; bin < 17; bin++) {
                ps->ipd_index[ps->num_env][bin] = ps->ipd_index[ps->num_env - 1][bin];
                ps->opd_index[ps->num_env][bin] = ps->opd_index[ps->num_env - 1][bin];
            }
            ps->num_env++;
            ps->border_position[ps->num_env] = ps->numTimeSlotsRate;
        }
        for(env = 1; env < ps->num_env; env++) {
            int8_t thr = ps->numTimeSlotsRate - (ps->num_env - env);

            if(ps->border_position[env] > thr) { ps->border_position[env] = thr; }
            else {
                thr = ps->border_position[env - 1] + 1;
                if(ps->border_position[env] < thr) { ps->border_position[env] = thr; }
            }
        }
    }
    /* make sure that the indices of all parameters can be mapped to the same hybrid synthesis filterbank  */
    if(ps->use34hybrid_bands) {
        for(env = 0; env < ps->num_env; env++) {
            if(ps->iid_mode != 2 && ps->iid_mode != 5) map20indexto34(ps->iid_index[env], 34);
            if(ps->icc_mode != 2 && ps->icc_mode != 5) map20indexto34(ps->icc_index[env], 34);
            if(ps->ipd_mode != 2 && ps->ipd_mode != 5) {
                map20indexto34(ps->ipd_index[env], 17);
                map20indexto34(ps->opd_index[env], 17);
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* decorrelate the mono signal using an allpass filter */
static void ps_decorrelate(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64], complex_t* X_hybrid_left[32], complex_t* X_hybrid_right[32]) {

    uint8_t          gr, n, bk;
    uint8_t          temp_delay = 0;
    uint8_t          sb, maxsb;
    const complex_t* Phi_Fract_SubQmf;
    uint8_t          temp_delay_ser[NO_ALLPASS_LINKS] = {0};
    int32_t           P_SmoothPeakDecayDiffNrg, nrg;
    complex_t        inputLeft;
    int32_t**        P = NULL;

    P = (int32_t**)faad_malloc(32 * sizeof(P));
    for(uint8_t i = 0; i < 32; i++){P[i] = (int32_t*)faad_malloc(34 * sizeof(*(P[i])));}

    for(uint8_t i = 0; i < 32; i++) memset(m_G_TransientRatio[i], 0, 34 * sizeof(*(m_G_TransientRatio[i])));

    /* chose hybrid filterbank: 20 or 34 band case */
    if(ps->use34hybrid_bands) { Phi_Fract_SubQmf = Phi_Fract_SubQmf34; }
    else { Phi_Fract_SubQmf = Phi_Fract_SubQmf20; }

    /* clear the energy values */
    for(n = 0; n < 32; n++) {
        for(bk = 0; bk < 34; bk++) { P[n][bk] = 0; }
    }

    /* calculate the energy in each parameter band b(k) */
    for(gr = 0; gr < ps->num_groups; gr++) {
        /* select the parameter index b(k) to which this group belongs */
        bk = (~NEGATE_IPD_MASK) & ps->map_group2bk[gr];

        /* select the upper subband border for this group */
        maxsb = (gr < ps->num_hybrid_groups) ? ps->group_border[gr] + 1 : ps->group_border[gr + 1];

        for(sb = ps->group_border[gr]; sb < maxsb; sb++) {
            for(n = ps->border_position[0]; n < ps->border_position[ps->num_env]; n++) {
#ifdef FIXED_POINT
                uint32_t in_re, in_im;
#endif
                /* input from hybrid subbands or QMF subbands */
                if(gr < ps->num_hybrid_groups) {
                    RE(inputLeft) = QMF_RE(X_hybrid_left[n][sb]);
                    IM(inputLeft) = QMF_IM(X_hybrid_left[n][sb]);
                }
                else {
                    RE(inputLeft) = QMF_RE(X_left[n][sb]);
                    IM(inputLeft) = QMF_IM(X_left[n][sb]);
                }

                /* accumulate energy */
#ifdef FIXED_POINT
                /* NOTE: all input is scaled by 2^(-5) because of fixed point QMF
                 * meaning that P will be scaled by 2^(-10) compared to floating point version
                 */
                in_re = ((abs(RE(inputLeft)) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
                in_im = ((abs(IM(inputLeft)) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
                P[n][bk] += in_re * in_re + in_im * in_im;
#else
                P[n][bk] += MUL_R(RE(inputLeft), RE(inputLeft)) + MUL_R(IM(inputLeft), IM(inputLeft));
#endif
            }
        }
    }

    /* calculate transient reduction ratio for each parameter band b(k) */
    for(bk = 0; bk < ps->nr_par_bands; bk++) {
        for(n = ps->border_position[0]; n < ps->border_position[ps->num_env]; n++) {
            const int32_t gamma = COEF_CONST(1.5);

            ps->P_PeakDecayNrg[bk] = MUL_F(ps->P_PeakDecayNrg[bk], ps->alpha_decay);
            if(ps->P_PeakDecayNrg[bk] < P[n][bk]) ps->P_PeakDecayNrg[bk] = P[n][bk];

            /* apply smoothing filter to peak decay energy */
            P_SmoothPeakDecayDiffNrg = ps->P_SmoothPeakDecayDiffNrg_prev[bk];
            P_SmoothPeakDecayDiffNrg += MUL_F((ps->P_PeakDecayNrg[bk] - P[n][bk] - ps->P_SmoothPeakDecayDiffNrg_prev[bk]), ps->alpha_smooth);
            ps->P_SmoothPeakDecayDiffNrg_prev[bk] = P_SmoothPeakDecayDiffNrg;

            /* apply smoothing filter to energy */
            nrg = ps->P_prev[bk];
            nrg += MUL_F((P[n][bk] - ps->P_prev[bk]), ps->alpha_smooth);
            ps->P_prev[bk] = nrg;

            /* calculate transient ratio */
            if(MUL_C(P_SmoothPeakDecayDiffNrg, gamma) <= nrg) { m_G_TransientRatio[n][bk] = REAL_CONST(1.0); }
            else { m_G_TransientRatio[n][bk] = DIV_R(nrg, (MUL_C(P_SmoothPeakDecayDiffNrg, gamma))); }
        }
    }

    /* apply stereo decorrelation filter to the signal */
    for(gr = 0; gr < ps->num_groups; gr++) {
        if(gr < ps->num_hybrid_groups) maxsb = ps->group_border[gr] + 1;
        else maxsb = ps->group_border[gr + 1];

        /* QMF channel */
        for(sb = ps->group_border[gr]; sb < maxsb; sb++) {
            int32_t g_DecaySlope;
            int32_t g_DecaySlope_filt[NO_ALLPASS_LINKS];

            /* g_DecaySlope: [0..1] */
            if(gr < ps->num_hybrid_groups || sb <= ps->decay_cutoff) { g_DecaySlope = FRAC_CONST(1.0); }
            else {
                int8_t decay = ps->decay_cutoff - sb;
                if(decay <= -20 /* -1/DECAY_SLOPE */) { g_DecaySlope = 0; }
                else {
                    /* decay(int)*decay_slope(frac) = g_DecaySlope(frac) */
                    g_DecaySlope = FRAC_CONST(1.0) + DECAY_SLOPE * decay;
                }
            }

            /* calculate g_DecaySlope_filt for every n multiplied by filter_a[n] */
            for(n = 0; n < NO_ALLPASS_LINKS; n++) { g_DecaySlope_filt[n] = MUL_F(g_DecaySlope, filter_a[n]); }

            /* set delay indices */
            temp_delay = ps->saved_delay;
            for(n = 0; n < NO_ALLPASS_LINKS; n++) temp_delay_ser[n] = ps->delay_buf_index_ser[n];

            for(n = ps->border_position[0]; n < ps->border_position[ps->num_env]; n++) {
                complex_t tmp, tmp0, R0;
                uint8_t   m;

                if(gr < ps->num_hybrid_groups) {
                    /* hybrid filterbank input */
                    RE(inputLeft) = QMF_RE(X_hybrid_left[n][sb]);
                    IM(inputLeft) = QMF_IM(X_hybrid_left[n][sb]);
                }
                else {
                    /* QMF filterbank input */
                    RE(inputLeft) = QMF_RE(X_left[n][sb]);
                    IM(inputLeft) = QMF_IM(X_left[n][sb]);
                }

                if(sb > ps->nr_allpass_bands && gr >= ps->num_hybrid_groups) {
                    /* delay */

                    /* never hybrid subbands here, always QMF subbands */
                    RE(tmp) = RE(ps->delay_Qmf[ps->delay_buf_index_delay[sb]][sb]);
                    IM(tmp) = IM(ps->delay_Qmf[ps->delay_buf_index_delay[sb]][sb]);
                    RE(R0) = RE(tmp);
                    IM(R0) = IM(tmp);
                    RE(ps->delay_Qmf[ps->delay_buf_index_delay[sb]][sb]) = RE(inputLeft);
                    IM(ps->delay_Qmf[ps->delay_buf_index_delay[sb]][sb]) = IM(inputLeft);
                }
                else {
                    /* allpass filter */
                    complex_t Phi_Fract;

                    /* fetch parameters */
                    if(gr < ps->num_hybrid_groups) {
                        /* select data from the hybrid subbands */
                        RE(tmp0) = RE(ps->delay_SubQmf[temp_delay][sb]);
                        IM(tmp0) = IM(ps->delay_SubQmf[temp_delay][sb]);
                        RE(ps->delay_SubQmf[temp_delay][sb]) = RE(inputLeft);
                        IM(ps->delay_SubQmf[temp_delay][sb]) = IM(inputLeft);
                        RE(Phi_Fract) = RE(Phi_Fract_SubQmf[sb]);
                        IM(Phi_Fract) = IM(Phi_Fract_SubQmf[sb]);
                    }
                    else {
                        /* select data from the QMF subbands */
                        RE(tmp0) = RE(ps->delay_Qmf[temp_delay][sb]);
                        IM(tmp0) = IM(ps->delay_Qmf[temp_delay][sb]);
                        RE(ps->delay_Qmf[temp_delay][sb]) = RE(inputLeft);
                        IM(ps->delay_Qmf[temp_delay][sb]) = IM(inputLeft);
                        RE(Phi_Fract) = RE(Phi_Fract_Qmf[sb]);
                        IM(Phi_Fract) = IM(Phi_Fract_Qmf[sb]);
                    }

                    /* z^(-2) * Phi_Fract[k] */
                    ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(Phi_Fract), IM(Phi_Fract));

                    RE(R0) = RE(tmp);
                    IM(R0) = IM(tmp);
                    for(m = 0; m < NO_ALLPASS_LINKS; m++) {
                        complex_t Q_Fract_allpass, tmp2;
                        /* fetch parameters */
                        if(gr < ps->num_hybrid_groups) {
                            /* select data from the hybrid subbands */
                            RE(tmp0) = RE(ps->delay_SubQmf_ser[m][temp_delay_ser[m]][sb]);
                            IM(tmp0) = IM(ps->delay_SubQmf_ser[m][temp_delay_ser[m]][sb]);
                            if(ps->use34hybrid_bands) {
                                RE(Q_Fract_allpass) = RE(Q_Fract_allpass_SubQmf34[sb][m]);
                                IM(Q_Fract_allpass) = IM(Q_Fract_allpass_SubQmf34[sb][m]);
                            }
                            else {
                                RE(Q_Fract_allpass) = RE(Q_Fract_allpass_SubQmf20[sb][m]);
                                IM(Q_Fract_allpass) = IM(Q_Fract_allpass_SubQmf20[sb][m]);
                            }
                        }
                        else {
                            /* select data from the QMF subbands */
                            RE(tmp0) = RE(ps->delay_Qmf_ser[m][temp_delay_ser[m]][sb]);
                            IM(tmp0) = IM(ps->delay_Qmf_ser[m][temp_delay_ser[m]][sb]);
                            RE(Q_Fract_allpass) = RE(Q_Fract_allpass_Qmf[sb][m]);
                            IM(Q_Fract_allpass) = IM(Q_Fract_allpass_Qmf[sb][m]);
                        }
                        /* delay by a fraction */
                        /* z^(-d(m)) * Q_Fract_allpass[k,m] */
                        ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(Q_Fract_allpass), IM(Q_Fract_allpass));
                        /* -a(m) * g_DecaySlope[k] */
                        RE(tmp) += -MUL_F(g_DecaySlope_filt[m], RE(R0));
                        IM(tmp) += -MUL_F(g_DecaySlope_filt[m], IM(R0));
                        /* -a(m) * g_DecaySlope[k] * Q_Fract_allpass[k,m] * z^(-d(m)) */
                        RE(tmp2) = RE(R0) + MUL_F(g_DecaySlope_filt[m], RE(tmp));
                        IM(tmp2) = IM(R0) + MUL_F(g_DecaySlope_filt[m], IM(tmp));
                        /* store sample */
                        if(gr < ps->num_hybrid_groups) {
                            RE(ps->delay_SubQmf_ser[m][temp_delay_ser[m]][sb]) = RE(tmp2);
                            IM(ps->delay_SubQmf_ser[m][temp_delay_ser[m]][sb]) = IM(tmp2);
                        }
                        else {
                            RE(ps->delay_Qmf_ser[m][temp_delay_ser[m]][sb]) = RE(tmp2);
                            IM(ps->delay_Qmf_ser[m][temp_delay_ser[m]][sb]) = IM(tmp2);
                        }
                        /* store for next iteration (or as output value if last iteration) */
                        RE(R0) = RE(tmp);
                        IM(R0) = IM(tmp);
                    }
                }
                /* select b(k) for reading the transient ratio */
                bk = (~NEGATE_IPD_MASK) & ps->map_group2bk[gr];
                /* duck if a past transient is found */
                RE(R0) = MUL_R(m_G_TransientRatio[n][bk], RE(R0));
                IM(R0) = MUL_R(m_G_TransientRatio[n][bk], IM(R0));

                if(gr < ps->num_hybrid_groups) {
                    /* hybrid */
                    QMF_RE(X_hybrid_right[n][sb]) = RE(R0);
                    QMF_IM(X_hybrid_right[n][sb]) = IM(R0);
                }
                else {
                    /* QMF */
                    QMF_RE(X_right[n][sb]) = RE(R0);
                    QMF_IM(X_right[n][sb]) = IM(R0);
                }
                /* Update delay buffer index */
                if(++temp_delay >= 2) { temp_delay = 0; }
                /* update delay indices */
                if(sb > ps->nr_allpass_bands && gr >= ps->num_hybrid_groups) {
                    /* delay_D depends on the samplerate, it can hold the values 14 and 1 */
                    if(++ps->delay_buf_index_delay[sb] >= ps->delay_D[sb]) { ps->delay_buf_index_delay[sb] = 0; }
                }
                for(m = 0; m < NO_ALLPASS_LINKS; m++) {
                    if(++temp_delay_ser[m] >= ps->num_sample_delay_ser[m]) { temp_delay_ser[m] = 0; }
                }
            }
        }
    }
    /* update delay indices */
    ps->saved_delay = temp_delay;
    for(n = 0; n < NO_ALLPASS_LINKS; n++) ps->delay_buf_index_ser[n] = temp_delay_ser[n];

    if(P) {for(uint8_t i = 0; i < 32; i++){free(P[i]); P[i] = NULL;} free(P); P = NULL;}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const int32_t ipdopd_cos_tab[] = {FRAC_CONST(1.000000000000000),  FRAC_CONST(0.707106781186548),  FRAC_CONST(0.000000000000000), FRAC_CONST(-0.707106781186547), FRAC_CONST(-1.000000000000000),
                                  FRAC_CONST(-0.707106781186548), FRAC_CONST(-0.000000000000000), FRAC_CONST(0.707106781186547), FRAC_CONST(1.000000000000000)};
const int32_t ipdopd_sin_tab[] = {FRAC_CONST(0.000000000000000),  FRAC_CONST(0.707106781186547),  FRAC_CONST(1.000000000000000),  FRAC_CONST(0.707106781186548), FRAC_CONST(0.000000000000000),
                                  FRAC_CONST(-0.707106781186547), FRAC_CONST(-1.000000000000000), FRAC_CONST(-0.707106781186548), FRAC_CONST(-0.000000000000000)};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t magnitude_c(complex_t c) {

#ifdef FIXED_POINT
    #define ps_abs(A) (((A) > 0) ? (A) : (-(A)))
    #define ALPHA     FRAC_CONST(0.948059448969)
    #define BETA      FRAC_CONST(0.392699081699)

    int32_t abs_inphase = ps_abs(RE(c));
    int32_t abs_quadrature = ps_abs(IM(c));
    if(abs_inphase > abs_quadrature) { return MUL_F(abs_inphase, ALPHA) + MUL_F(abs_quadrature, BETA); }
    else { return MUL_F(abs_quadrature, ALPHA) + MUL_F(abs_inphase, BETA); }
#else
    return sqrt(RE(c)*RE(c) + IM(c)*IM(c));
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void ps_mix_phase(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64], complex_t* X_hybrid_left[32], complex_t* X_hybrid_right[32]) {

    uint8_t        n;
    uint8_t        gr;
    uint8_t        bk = 0;
    uint8_t        sb, maxsb;
    uint8_t        env;
    uint8_t        nr_ipdopd_par;
    complex_t      h11 = {0}, h12 = {0}, h21 = {0}, h22 = {0};                     // COEF
    complex_t      H11 = {0}, H12 = {0}, H21 = {0}, H22 = {0};                     // COEF
    complex_t      deltaH11 = {0}, deltaH12 = {0}, deltaH21 = {0}, deltaH22 = {0}; // COEF
    complex_t      tempLeft = {0}, tempRight = {0};                    // FRAC
    complex_t      phaseLeft = {0}, phaseRight = {0};                  // FRAC
    int32_t        L;
    const int32_t* sf_iid;
    uint8_t        no_iid_steps;

    if(ps->iid_mode >= 3) {
        no_iid_steps = 15;
        sf_iid = sf_iid_fine;
    }
    else {
        no_iid_steps = 7;
        sf_iid = sf_iid_normal;
    }

    if(ps->ipd_mode == 0 || ps->ipd_mode == 3) { nr_ipdopd_par = 11; /* resolution */ }
    else { nr_ipdopd_par = ps->nr_ipdopd_par; }

    for(gr = 0; gr < ps->num_groups; gr++) {
        bk = (~NEGATE_IPD_MASK) & ps->map_group2bk[gr];

        /* use one channel per group in the subqmf domain */
        maxsb = (gr < ps->num_hybrid_groups) ? ps->group_border[gr] + 1 : ps->group_border[gr + 1];

        for(env = 0; env < ps->num_env; env++) {
            uint8_t abs_iid = (uint8_t)abs(ps->iid_index[env][bk]);
            /* index range is supposed to be -7...7 or -15...15 depending on iid_mode
                (Table 8.24, ISO/IEC 14496-3:2005).
                if it is outside these boundaries, this is most likely an error. sanitize
                it and try to process further. */
            if(ps->iid_index[env][bk] < -no_iid_steps) {
                fprintf(stderr, "Warning: invalid iid_index: %d < %d\n", ps->iid_index[env][bk], -no_iid_steps);
                ps->iid_index[env][bk] = -no_iid_steps;
                abs_iid = no_iid_steps;
            }
            else if(ps->iid_index[env][bk] > no_iid_steps) {
                fprintf(stderr, "Warning: invalid iid_index: %d > %d\n", ps->iid_index[env][bk], no_iid_steps);
                ps->iid_index[env][bk] = no_iid_steps;
                abs_iid = no_iid_steps;
            }
            if(ps->icc_index[env][bk] < 0) {
                fprintf(stderr, "Warning: invalid icc_index: %d < 0\n", ps->icc_index[env][bk]);
                ps->icc_index[env][bk] = 0;
            }
            else if(ps->icc_index[env][bk] > 7) {
                fprintf(stderr, "Warning: invalid icc_index: %d > 7\n", ps->icc_index[env][bk]);
                ps->icc_index[env][bk] = 7;
            }

            if(ps->icc_mode < 3) {
                /* type 'A' mixing as described in 8.6.4.6.2.1 */
                int32_t c_1, c_2;   // COEF
                int32_t cosa, sina; // COEF
                int32_t cosb, sinb; // COEF
                int32_t ab1, ab2;   // COEF
                int32_t ab3, ab4;   // COEF

                /*
                c_1 = sqrt(2.0 / (1.0 + pow(10.0, quant_iid[no_iid_steps + iid_index] / 10.0)));
                c_2 = sqrt(2.0 / (1.0 + pow(10.0, quant_iid[no_iid_steps - iid_index] / 10.0)));
                alpha = 0.5 * acos(quant_rho[icc_index]);
                beta = alpha * ( c_1 - c_2 ) / sqrt(2.0);
                */

                // printf("%d\n", ps->iid_index[env][bk]);

                /* calculate the scalefactors c_1 and c_2 from the intensity differences */
                c_1 = sf_iid[no_iid_steps + ps->iid_index[env][bk]];
                c_2 = sf_iid[no_iid_steps - ps->iid_index[env][bk]];

                /* calculate alpha and beta using the ICC parameters */
                cosa = cos_alphas[ps->icc_index[env][bk]];
                sina = sin_alphas[ps->icc_index[env][bk]];

                if(ps->iid_mode >= 3) {
                    cosb = cos_betas_fine[abs_iid][ps->icc_index[env][bk]];
                    sinb = sin_betas_fine[abs_iid][ps->icc_index[env][bk]];
                }
                else {
                    cosb = cos_betas_normal[abs_iid][ps->icc_index[env][bk]];
                    sinb = sin_betas_normal[abs_iid][ps->icc_index[env][bk]];
                }

                ab1 = MUL_C(cosb, cosa);
                ab2 = MUL_C(sinb, sina);
                ab3 = MUL_C(sinb, cosa);
                ab4 = MUL_C(cosb, sina);

                /* h_xy: COEF */
                RE(h11) = MUL_C(c_2, (ab1 - ab2));
                RE(h12) = MUL_C(c_1, (ab1 + ab2));
                RE(h21) = MUL_C(c_2, (ab3 + ab4));
                RE(h22) = MUL_C(c_1, (ab3 - ab4));
            }
            else {
                /* type 'B' mixing as described in 8.6.4.6.2.2 */
                int32_t sina, cosa; // COEF
                int32_t cosg, sing; // COEF

                /*
                real_t c, rho, mu, alpha, gamma;
                uint8_t i;

                i = ps->iid_index[env][bk];
                c = (real_t)pow(10.0, ((i)?(((i>0)?1:-1)*quant_iid[((i>0)?i:-i)-1]):0.)/20.0);
                rho = quant_rho[ps->icc_index[env][bk]];

                if (rho == 0.0f && c == 1.)
                {
                    alpha = (real_t)M_PI/4.0f;
                    rho = 0.05f;
                } else {
                    if (rho <= 0.05f)
                    {
                        rho = 0.05f;
                    }
                    alpha = 0.5f*(real_t)atan( (2.0f*c*rho) / (c*c-1.0f) );

                    if (alpha < 0.)
                    {
                        alpha += (real_t)M_PI/2.0f;
                    }
                    if (rho < 0.)
                    {
                        alpha += (real_t)M_PI;
                    }
                }
                mu = c+1.0f/c;
                mu = 1+(4.0f*rho*rho-4.0f)/(mu*mu);
                gamma = (real_t)atan(sqrt((1.0f-sqrt(mu))/(1.0f+sqrt(mu))));
                */

                if(ps->iid_mode >= 3) {
                    cosa = sincos_alphas_B_fine[no_iid_steps + ps->iid_index[env][bk]][ps->icc_index[env][bk]];
                    sina = sincos_alphas_B_fine[30 - (no_iid_steps + ps->iid_index[env][bk])][ps->icc_index[env][bk]];
                    cosg = cos_gammas_fine[abs_iid][ps->icc_index[env][bk]];
                    sing = sin_gammas_fine[abs_iid][ps->icc_index[env][bk]];
                }
                else {
                    cosa = sincos_alphas_B_normal[no_iid_steps + ps->iid_index[env][bk]][ps->icc_index[env][bk]];
                    sina = sincos_alphas_B_normal[14 - (no_iid_steps + ps->iid_index[env][bk])][ps->icc_index[env][bk]];
                    cosg = cos_gammas_normal[abs_iid][ps->icc_index[env][bk]];
                    sing = sin_gammas_normal[abs_iid][ps->icc_index[env][bk]];
                }

                RE(h11) = MUL_C(COEF_SQRT2, MUL_C(cosa, cosg));
                RE(h12) = MUL_C(COEF_SQRT2, MUL_C(sina, cosg));
                RE(h21) = MUL_C(COEF_SQRT2, MUL_C(-cosa, sing));
                RE(h22) = MUL_C(COEF_SQRT2, MUL_C(sina, sing));
            }
            IM(h11) = IM(h12) = IM(h21) = IM(h22) = 0;

            /* calculate phase rotation parameters H_xy */
            /* note that the imaginary part of these parameters are only calculated when
               IPD and OPD are enabled
             */
            if((ps->enable_ipdopd) && (bk < nr_ipdopd_par)) {
                int8_t  i;
                int32_t xy, pq, xypq; // FRAC

                /* ringbuffer index */
                i = ps->phase_hist;

                /* previous value */
#ifdef FIXED_POINT
                /* divide by 4*2, shift right 3 bits;
                   extra halving to avoid overflows; it is ok, because result is normalized */
                RE(tempLeft) = RE(ps->ipd_prev[bk][i]) >> 3;
                IM(tempLeft) = IM(ps->ipd_prev[bk][i]) >> 3;
                RE(tempRight) = RE(ps->opd_prev[bk][i]) >> 3;
                IM(tempRight) = IM(ps->opd_prev[bk][i]) >> 3;
#else
                RE(tempLeft) = MUL_F(RE(ps->ipd_prev[bk][i]), FRAC_CONST(0.25));
                IM(tempLeft) = MUL_F(IM(ps->ipd_prev[bk][i]), FRAC_CONST(0.25));
                RE(tempRight) = MUL_F(RE(ps->opd_prev[bk][i]), FRAC_CONST(0.25));
                IM(tempRight) = MUL_F(IM(ps->opd_prev[bk][i]), FRAC_CONST(0.25));
#endif

                /* save current value */
                RE(ps->ipd_prev[bk][i]) = ipdopd_cos_tab[abs(ps->ipd_index[env][bk])];
                IM(ps->ipd_prev[bk][i]) = ipdopd_sin_tab[abs(ps->ipd_index[env][bk])];
                RE(ps->opd_prev[bk][i]) = ipdopd_cos_tab[abs(ps->opd_index[env][bk])];
                IM(ps->opd_prev[bk][i]) = ipdopd_sin_tab[abs(ps->opd_index[env][bk])];

                /* add current value */
#ifdef FIXED_POINT
                /* extra halving to avoid overflows */
                RE(tempLeft) += RE(ps->ipd_prev[bk][i]) >> 1;
                IM(tempLeft) += IM(ps->ipd_prev[bk][i]) >> 1;
                RE(tempRight) += RE(ps->opd_prev[bk][i]) >> 1;
                IM(tempRight) += IM(ps->opd_prev[bk][i]) >> 1;
#else
                RE(tempLeft) += RE(ps->ipd_prev[bk][i]);
                IM(tempLeft) += IM(ps->ipd_prev[bk][i]);
                RE(tempRight) += RE(ps->opd_prev[bk][i]);
                IM(tempRight) += IM(ps->opd_prev[bk][i]);
#endif

                /* ringbuffer index */
                if(i == 0) { i = 2; }
                i--;

                /* get value before previous */
#ifdef FIXED_POINT
                /* dividing by 2*2, shift right 2 bits; extra halving to avoid overflows */
                RE(tempLeft) += (RE(ps->ipd_prev[bk][i]) >> 2);
                IM(tempLeft) += (IM(ps->ipd_prev[bk][i]) >> 2);
                RE(tempRight) += (RE(ps->opd_prev[bk][i]) >> 2);
                IM(tempRight) += (IM(ps->opd_prev[bk][i]) >> 2);
#else
                RE(tempLeft) += MUL_F(RE(ps->ipd_prev[bk][i]), FRAC_CONST(0.5));
                IM(tempLeft) += MUL_F(IM(ps->ipd_prev[bk][i]), FRAC_CONST(0.5));
                RE(tempRight) += MUL_F(RE(ps->opd_prev[bk][i]), FRAC_CONST(0.5));
                IM(tempRight) += MUL_F(IM(ps->opd_prev[bk][i]), FRAC_CONST(0.5));
#endif

#if 0 /* original code */
                ipd = (float)atan2(IM(tempLeft), RE(tempLeft));
                opd = (float)atan2(IM(tempRight), RE(tempRight));

                /* phase rotation */
                RE(phaseLeft) = (float)cos(opd);
                IM(phaseLeft) = (float)sin(opd);
                opd -= ipd;
                RE(phaseRight) = (float)cos(opd);
                IM(phaseRight) = (float)sin(opd);
#else

                // x = IM(tempLeft)
                // y = RE(tempLeft)
                // p = IM(tempRight)
                // q = RE(tempRight)
                // cos(atan2(x,y)) = y/sqrt((x*x) + (y*y))
                // sin(atan2(x,y)) = x/sqrt((x*x) + (y*y))
                // cos(atan2(x,y)-atan2(p,q)) = (y*q + x*p) / ( sqrt((x*x) + (y*y)) * sqrt((p*p) + (q*q)) );
                // sin(atan2(x,y)-atan2(p,q)) = (x*q - y*p) / ( sqrt((x*x) + (y*y)) * sqrt((p*p) + (q*q)) );

                xy = magnitude_c(tempRight);
                pq = magnitude_c(tempLeft);

                if(xy != 0) {
                    RE(phaseLeft) = DIV_F(RE(tempRight), xy);
                    IM(phaseLeft) = DIV_F(IM(tempRight), xy);
                }
                else {
                    RE(phaseLeft) = 0;
                    IM(phaseLeft) = 0;
                }

                xypq = MUL_F(xy, pq);

                if(xypq != 0) {
                    int32_t tmp1 = MUL_F(RE(tempRight), RE(tempLeft)) + MUL_F(IM(tempRight), IM(tempLeft));
                    int32_t tmp2 = MUL_F(IM(tempRight), RE(tempLeft)) - MUL_F(RE(tempRight), IM(tempLeft));

                    RE(phaseRight) = DIV_F(tmp1, xypq);
                    IM(phaseRight) = DIV_F(tmp2, xypq);
                }
                else {
                    RE(phaseRight) = 0;
                    IM(phaseRight) = 0;
                }

#endif

                /* MUL_F(COEF, REAL) = COEF */
                IM(h11) = MUL_F(RE(h11), IM(phaseLeft));
                IM(h12) = MUL_F(RE(h12), IM(phaseRight));
                IM(h21) = MUL_F(RE(h21), IM(phaseLeft));
                IM(h22) = MUL_F(RE(h22), IM(phaseRight));

                RE(h11) = MUL_F(RE(h11), RE(phaseLeft));
                RE(h12) = MUL_F(RE(h12), RE(phaseRight));
                RE(h21) = MUL_F(RE(h21), RE(phaseLeft));
                RE(h22) = MUL_F(RE(h22), RE(phaseRight));
            }

            /* length of the envelope n_e+1 - n_e (in time samples) */
            /* 0 < L <= 32: integer */
            L = (int32_t)(ps->border_position[env + 1] - ps->border_position[env]);

            /* obtain final H_xy by means of linear interpolation */
            RE(deltaH11) = (RE(h11) - RE(ps->h11_prev[gr])) / L;
            RE(deltaH12) = (RE(h12) - RE(ps->h12_prev[gr])) / L;
            RE(deltaH21) = (RE(h21) - RE(ps->h21_prev[gr])) / L;
            RE(deltaH22) = (RE(h22) - RE(ps->h22_prev[gr])) / L;

            RE(H11) = RE(ps->h11_prev[gr]);
            RE(H12) = RE(ps->h12_prev[gr]);
            RE(H21) = RE(ps->h21_prev[gr]);
            RE(H22) = RE(ps->h22_prev[gr]);
            IM(H11) = IM(H12) = IM(H21) = IM(H22) = 0;

            RE(ps->h11_prev[gr]) = RE(h11);
            RE(ps->h12_prev[gr]) = RE(h12);
            RE(ps->h21_prev[gr]) = RE(h21);
            RE(ps->h22_prev[gr]) = RE(h22);

            /* only calculate imaginary part when needed */
            if((ps->enable_ipdopd) && (bk < nr_ipdopd_par)) {
                /* obtain final H_xy by means of linear interpolation */
                IM(deltaH11) = (IM(h11) - IM(ps->h11_prev[gr])) / L;
                IM(deltaH12) = (IM(h12) - IM(ps->h12_prev[gr])) / L;
                IM(deltaH21) = (IM(h21) - IM(ps->h21_prev[gr])) / L;
                IM(deltaH22) = (IM(h22) - IM(ps->h22_prev[gr])) / L;

                IM(H11) = IM(ps->h11_prev[gr]);
                IM(H12) = IM(ps->h12_prev[gr]);
                IM(H21) = IM(ps->h21_prev[gr]);
                IM(H22) = IM(ps->h22_prev[gr]);

                if((NEGATE_IPD_MASK & ps->map_group2bk[gr]) != 0) {
                    IM(deltaH11) = -IM(deltaH11);
                    IM(deltaH12) = -IM(deltaH12);
                    IM(deltaH21) = -IM(deltaH21);
                    IM(deltaH22) = -IM(deltaH22);

                    IM(H11) = -IM(H11);
                    IM(H12) = -IM(H12);
                    IM(H21) = -IM(H21);
                    IM(H22) = -IM(H22);
                }

                IM(ps->h11_prev[gr]) = IM(h11);
                IM(ps->h12_prev[gr]) = IM(h12);
                IM(ps->h21_prev[gr]) = IM(h21);
                IM(ps->h22_prev[gr]) = IM(h22);
            }

            /* apply H_xy to the current envelope band of the decorrelated subband */
            for(n = ps->border_position[env]; n < ps->border_position[env + 1]; n++) {
                /* addition finalises the interpolation over every n */
                RE(H11) += RE(deltaH11);
                RE(H12) += RE(deltaH12);
                RE(H21) += RE(deltaH21);
                RE(H22) += RE(deltaH22);
                if((ps->enable_ipdopd) && (bk < nr_ipdopd_par)) {
                    IM(H11) += IM(deltaH11);
                    IM(H12) += IM(deltaH12);
                    IM(H21) += IM(deltaH21);
                    IM(H22) += IM(deltaH22);
                }

                /* channel is an alias to the subband */
                for(sb = ps->group_border[gr]; sb < maxsb; sb++) {
                    complex_t inLeft, inRight; // precision_of in(Left|Right) == precision_of X_(left|right)

                    /* load decorrelated samples */
                    if(gr < ps->num_hybrid_groups) {
                        RE(inLeft) = RE(X_hybrid_left[n][sb]);
                        IM(inLeft) = IM(X_hybrid_left[n][sb]);
                        RE(inRight) = RE(X_hybrid_right[n][sb]);
                        IM(inRight) = IM(X_hybrid_right[n][sb]);
                    }
                    else {
                        RE(inLeft) = RE(X_left[n][sb]);
                        IM(inLeft) = IM(X_left[n][sb]);
                        RE(inRight) = RE(X_right[n][sb]);
                        IM(inRight) = IM(X_right[n][sb]);
                    }

                    /* precision_of temp(Left|Right) == precision_of X_(left|right) */

                    /* apply mixing */
                    RE(tempLeft) = MUL_C(RE(H11), RE(inLeft)) + MUL_C(RE(H21), RE(inRight));
                    IM(tempLeft) = MUL_C(RE(H11), IM(inLeft)) + MUL_C(RE(H21), IM(inRight));
                    RE(tempRight) = MUL_C(RE(H12), RE(inLeft)) + MUL_C(RE(H22), RE(inRight));
                    IM(tempRight) = MUL_C(RE(H12), IM(inLeft)) + MUL_C(RE(H22), IM(inRight));

                    /* only perform imaginary operations when needed */
                    if((ps->enable_ipdopd) && (bk < nr_ipdopd_par)) {
                        /* apply rotation */
                        RE(tempLeft) -= MUL_C(IM(H11), IM(inLeft)) + MUL_C(IM(H21), IM(inRight));
                        IM(tempLeft) += MUL_C(IM(H11), RE(inLeft)) + MUL_C(IM(H21), RE(inRight));
                        RE(tempRight) -= MUL_C(IM(H12), IM(inLeft)) + MUL_C(IM(H22), IM(inRight));
                        IM(tempRight) += MUL_C(IM(H12), RE(inLeft)) + MUL_C(IM(H22), RE(inRight));
                    }

                    /* store final samples */
                    if(gr < ps->num_hybrid_groups) {
                        RE(X_hybrid_left[n][sb]) = RE(tempLeft);
                        IM(X_hybrid_left[n][sb]) = IM(tempLeft);
                        RE(X_hybrid_right[n][sb]) = RE(tempRight);
                        IM(X_hybrid_right[n][sb]) = IM(tempRight);
                    }
                    else {
                        RE(X_left[n][sb]) = RE(tempLeft);
                        IM(X_left[n][sb]) = IM(tempLeft);
                        RE(X_right[n][sb]) = RE(tempRight);
                        IM(X_right[n][sb]) = IM(tempRight);
                    }
                }
            }

            /* shift phase smoother's circular buffer index */
            ps->phase_hist++;
            if(ps->phase_hist == 2) { ps->phase_hist = 0; }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ps_free(ps_info_t* ps) {
    /* free hybrid filterbank structures */
    hybrid_free((hyb_info_t*)ps->hyb);
    faad_free(ps);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ps_info_t* ps_init(uint8_t sr_index, uint8_t numTimeSlotsRate) {
    uint8_t i;
    (void)sr_index;
    uint8_t short_delay_band;

    ps_info_t* ps = (ps_info_t*)faad_malloc(sizeof(ps_info_t));
    memset(ps, 0, sizeof(ps_info_t));
    ps->hyb = hybrid_init(numTimeSlotsRate);
    ps->numTimeSlotsRate = numTimeSlotsRate;
    ps->ps_data_available = 0;
    /* delay stuff*/
    ps->saved_delay = 0;
    for(i = 0; i < 64; i++) { ps->delay_buf_index_delay[i] = 0; }
    for(i = 0; i < NO_ALLPASS_LINKS; i++) {
        ps->delay_buf_index_ser[i] = 0;
        /* THESE ARE CONSTANTS NOW */
        ps->num_sample_delay_ser[i] = delay_length_d[i];
    }
    /* THESE ARE CONSTANTS NOW */
    short_delay_band = 35;
    ps->nr_allpass_bands = 22;
    ps->alpha_decay = FRAC_CONST(0.76592833836465);
    ps->alpha_smooth = FRAC_CONST(0.25);
    /* THESE ARE CONSTANT NOW IF PS IS INDEPENDANT OF SAMPLERATE */
    for(i = 0; i < short_delay_band; i++) { ps->delay_D[i] = 14; }
    for(i = short_delay_band; i < 64; i++) { ps->delay_D[i] = 1; }
    /* mixing and phase */
    for(i = 0; i < 50; i++) {
        RE(ps->h11_prev[i]) = 1;
        IM(ps->h12_prev[i]) = 1;
        RE(ps->h11_prev[i]) = 1;
        IM(ps->h12_prev[i]) = 1;
    }
    ps->phase_hist = 0;
    for(i = 0; i < 20; i++) {
        RE(ps->ipd_prev[i][0]) = 0;
        IM(ps->ipd_prev[i][0]) = 0;
        RE(ps->ipd_prev[i][1]) = 0;
        IM(ps->ipd_prev[i][1]) = 0;
        RE(ps->opd_prev[i][0]) = 0;
        IM(ps->opd_prev[i][0]) = 0;
        RE(ps->opd_prev[i][1]) = 0;
        IM(ps->opd_prev[i][1]) = 0;
    }
    return ps;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* main Parametric Stereo decoding function */
uint8_t ps_decode(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64]) {

    for(uint8_t i = 0; i < 32; i++) memset(m_X_hybrid_left[i], 0, 34 * sizeof(*(m_X_hybrid_left[i])));
    for(uint8_t i = 0; i < 32; i++) memset(m_X_hybrid_right[i], 0, 34 * sizeof(*(m_X_hybrid_right[i])));

    /* delta decoding of the bitstream data */
    ps_data_decode(ps);
    /* set up some parameters depending on filterbank type */
    if(ps->use34hybrid_bands) {
        ps->group_border = (uint8_t*)group_border34;
        ps->map_group2bk = (uint16_t*)map_group2bk34;
        ps->num_groups = 32 + 18;
        ps->num_hybrid_groups = 32;
        ps->nr_par_bands = 34;
        ps->decay_cutoff = 5;
    }
    else {
        ps->group_border = (uint8_t*)group_border20;
        ps->map_group2bk = (uint16_t*)map_group2bk20;
        ps->num_groups = 10 + 12;
        ps->num_hybrid_groups = 10;
        ps->nr_par_bands = 20;
        ps->decay_cutoff = 3;
    }
    /* Perform further analysis on the lowest subbands to get a higher
     * frequency resolution
     */
    hybrid_analysis((hyb_info_t*)ps->hyb, X_left, m_X_hybrid_left, ps->use34hybrid_bands, ps->numTimeSlotsRate);
    /* decorrelate mono signal */
    ps_decorrelate(ps, X_left, X_right, m_X_hybrid_left, m_X_hybrid_right);
    /* apply mixing and phase parameters */
    ps_mix_phase(ps, X_left, X_right, m_X_hybrid_left, m_X_hybrid_right);
    /* hybrid synthesis, to rebuild the SBR QMF matrices */
    hybrid_synthesis((hyb_info_t*)ps->hyb, X_left, m_X_hybrid_left, ps->use34hybrid_bands, ps->numTimeSlotsRate);
    hybrid_synthesis((hyb_info_t*)ps->hyb, X_right, m_X_hybrid_right, ps->use34hybrid_bands, ps->numTimeSlotsRate);
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t is_ltp_ot(uint8_t object_type) { /* check if the object type is an object type that can have LTP */
    (void)object_type;
#ifdef LTP_DEC
    if((object_type == LTP)
    #ifdef ERROR_RESILIENCE
       || (object_type == ER_LTP)
    #endif
    #ifdef LD_DEC
       || (object_type == LD)
    #endif
    ) {
        return 1;
    }
#endif
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void lt_prediction(ic_stream_t* ics, ltp_info_t* ltp, int32_t* spec, int16_t* lt_pred_stat, fb_info_t* fb, uint8_t win_shape, uint8_t win_shape_prev, uint8_t sr_index, uint8_t object_type,
                   uint16_t frame_len) {
    uint8_t  sfb;
    uint16_t bin, i, num_samples;

    if(ics->window_sequence != EIGHT_SHORT_SEQUENCE) {
        if(ltp->data_present) {
            num_samples = frame_len << 1;
            for(i = 0; i < num_samples; i++) {
                /* The extra lookback M (N/2 for LD, 0 for LTP) is handled in the buffer updating */
                /* lt_pred_stat is a 16 bit int32_t, multiplied with the fixed point real this gives a real for m_x_est */
                m_x_est[i] = (int32_t)lt_pred_stat[num_samples + i - ltp->lag] * codebook[ltp->coef];
            }
            filter_bank_ltp(fb, ics->window_sequence, win_shape, win_shape_prev, m_x_est, m_X_est, object_type, frame_len);
            tns_encode_frame(ics, &(ics->tns), sr_index, object_type, m_X_est, frame_len);
            for(sfb = 0; sfb < ltp->last_band; sfb++) {
                if(ltp->long_used[sfb]) {
                    uint16_t low = ics->swb_offset[sfb];
                    uint16_t high = min(ics->swb_offset[sfb + 1], ics->swb_offset_max);
                    for(bin = low; bin < high; bin++) { spec[bin] += m_X_est[bin]; }
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef FIXED_POINT
static inline int16_t real_to_int16(int32_t sig_in) {
    if(sig_in >= 0) {
        sig_in += (1 << (REAL_BITS - 1));
        if(sig_in >= REAL_CONST(32768)) return 32767;
    }
    else {
        sig_in += -(1 << (REAL_BITS - 1));
        if(sig_in <= REAL_CONST(-32768)) return -32768;
    }
    return (sig_in >> REAL_BITS);
}
#else
static INLINE int16_t real_to_int16(real_t sig_in){
    if (sig_in >= 0) {
#ifndef HAS_LRINTF
        sig_in += 0.5f;
#endif
        if (sig_in >= 32768.0f) return 32767;
    } else {
#ifndef HAS_LRINTF
        sig_in += -0.5f;
#endif
        if (sig_in <= -32768.0f) return -32768;
    }
    return (int16_t)lrintf(sig_in);
}
#endif
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void lt_update_state(int16_t* lt_pred_stat, int32_t* time, int32_t* overlap, uint16_t frame_len, uint8_t object_type) {
    (void)object_type;
    uint16_t i;

/*
 * The reference point for index i and the content of the buffer lt_pred_stat are arranged so that lt_pred_stat(0 ... N/2 - 1) contains the
 * last aliased half window from the IMDCT, and lt_pred_stat(N/2 ... N-1) is always all zeros. The rest of lt_pred_stat (i<0) contains the
 * previous fully reconstructed time domain samples, i.e., output of the decoder.
 * These values are shifted up by N*2 to avoid (i<0)
 * For the LD object type an extra 512 samples lookback is accomodated here.
 */
#ifdef LD_DEC
    if(object_type == LD) {
        for(i = 0; i < frame_len; i++) {
            lt_pred_stat[i] /* extra 512 */ = lt_pred_stat[i + frame_len];
            lt_pred_stat[frame_len + i] = lt_pred_stat[i + (frame_len * 2)];
            lt_pred_stat[(frame_len * 2) + i] = real_to_int16(time[i]);
            lt_pred_stat[(frame_len * 3) + i] = real_to_int16(overlap[i]);
        }
    }
    else {
#endif
        for(i = 0; i < frame_len; i++) {
            lt_pred_stat[i] = lt_pred_stat[i + frame_len];
            lt_pred_stat[frame_len + i] = real_to_int16(time[i]);
            lt_pred_stat[(frame_len * 2) + i] = real_to_int16(overlap[i]);
        }
#ifdef LD_DEC
    }
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
mdct_info_t* faad_mdct_init(uint16_t N) {
    mdct_info_t* mdct = (mdct_info_t*)faad_malloc(sizeof(mdct_info_t));
    assert(N % 8 == 0);
    mdct->N = N;
    /* NOTE: For "small framelengths" the coefficients need to be scaled by sqrt("(nearest power of 2) > N" / N) */
    /* RE(mdct->sincos[k]) = scale*(int32_t)(cos(2.0*M_PI*(k+1./8.) / (int32_t)N));
     * IM(mdct->sincos[k]) = scale*(int32_t)(sin(2.0*M_PI*(k+1./8.) / (int32_t)N)); */
    /* scale is 1 for fixed point, sqrt(N) for floating point */
    switch(N) {
    case 2048: mdct->sincos = (complex_t*)mdct_tab_2048; break;
    case 256: mdct->sincos = (complex_t*)mdct_tab_256; break;
#ifdef LD_DEC
    case 1024: mdct->sincos = (complex_t*)mdct_tab_1024; break;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    case 1920: mdct->sincos = (complex_t*)mdct_tab_1920; break;
    case 240: mdct->sincos = (complex_t*)mdct_tab_240; break;
    #ifdef LD_DEC
    case 960: mdct->sincos = (complex_t*)mdct_tab_960; break;
    #endif
#endif
    }
    /* initialise fft */
    mdct->cfft = cffti(N / 4);
    return mdct;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void faad_mdct_end(mdct_info_t* mdct) {
    if(mdct != NULL) {
        cfftu(mdct->cfft);
        faad_free(mdct);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void faad_imdct(mdct_info_t* mdct, int32_t* X_in, int32_t* X_out) {
    uint16_t  k;
    complex_t x;
#ifdef ALLOW_SMALL_FRAMELENGTH
#ifdef FIXED_POINT
    int32_t scale = 0, b_scale = 0;
#endif
#endif
    complex_t* sincos = mdct->sincos;
    uint16_t   N = mdct->N;
    uint16_t   N2 = N >> 1;
    uint16_t   N4 = N >> 2;
    uint16_t   N8 = N >> 3;
#ifdef ALLOW_SMALL_FRAMELENGTH
#ifdef FIXED_POINT
    /* detect non-power of 2 */
    if(N & (N - 1)) {
        /* adjust scale for non-power of 2 MDCT */
        /* 2048/1920 */
        b_scale = 1;
        scale = COEF_CONST(1.0666666666666667);
    }
#endif
#endif
    /* pre-IFFT complex multiplication */
    for(k = 0; k < N4; k++) { ComplexMult(&IM(m_Z1_imdct[k]), &RE(m_Z1_imdct[k]), X_in[2 * k], X_in[N2 - 1 - 2 * k], RE(sincos[k]), IM(sincos[k])); }
    /* complex IFFT, any non-scaling FFT can be used here */
    cfftb(mdct->cfft, m_Z1_imdct);
    /* post-IFFT complex multiplication */
    for(k = 0; k < N4; k++) {
        RE(x) = RE(m_Z1_imdct[k]);
        IM(x) = IM(m_Z1_imdct[k]);
        ComplexMult(&IM(m_Z1_imdct[k]), &RE(m_Z1_imdct[k]), IM(x), RE(x), RE(sincos[k]), IM(sincos[k]));
#ifdef ALLOW_SMALL_FRAMELENGTH
#ifdef FIXED_POINT
        /* non-power of 2 MDCT scaling */
        if(b_scale) {
            RE(m_Z1_imdct[k]) = MUL_C(RE(m_Z1_imdct[k]), scale);
            IM(m_Z1_imdct[k]) = MUL_C(IM(m_Z1_imdct[k]), scale);
        }
#endif
#endif
    }
    /* reordering */
    for(k = 0; k < N8; k += 2) {
        X_out[2 * k] = IM(m_Z1_imdct[N8 + k]);
        X_out[2 + 2 * k] = IM(m_Z1_imdct[N8 + 1 + k]);
        X_out[1 + 2 * k] = -RE(m_Z1_imdct[N8 - 1 - k]);
        X_out[3 + 2 * k] = -RE(m_Z1_imdct[N8 - 2 - k]);
        X_out[N4 + 2 * k] = RE(m_Z1_imdct[k]);
        X_out[N4 + +2 + 2 * k] = RE(m_Z1_imdct[1 + k]);
        X_out[N4 + 1 + 2 * k] = -IM(m_Z1_imdct[N4 - 1 - k]);
        X_out[N4 + 3 + 2 * k] = -IM(m_Z1_imdct[N4 - 2 - k]);
        X_out[N2 + 2 * k] = RE(m_Z1_imdct[N8 + k]);
        X_out[N2 + +2 + 2 * k] = RE(m_Z1_imdct[N8 + 1 + k]);
        X_out[N2 + 1 + 2 * k] = -IM(m_Z1_imdct[N8 - 1 - k]);
        X_out[N2 + 3 + 2 * k] = -IM(m_Z1_imdct[N8 - 2 - k]);
        X_out[N2 + N4 + 2 * k] = -IM(m_Z1_imdct[k]);
        X_out[N2 + N4 + 2 + 2 * k] = -IM(m_Z1_imdct[1 + k]);
        X_out[N2 + N4 + 1 + 2 * k] = RE(m_Z1_imdct[N4 - 1 - k]);
        X_out[N2 + N4 + 3 + 2 * k] = RE(m_Z1_imdct[N4 - 2 - k]);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void faad_mdct(mdct_info_t* mdct, int32_t* X_in, int32_t* X_out) {
    uint16_t  k;
    complex_t x;
    complex_t* sincos = mdct->sincos;
    uint16_t   N = mdct->N;
    uint16_t   N2 = N >> 1;
    uint16_t   N4 = N >> 2;
    uint16_t   N8 = N >> 3;
#ifndef FIXED_POINT
    int32_t    scale = REAL_CONST(N);
#else
	int32_t scale = REAL_CONST(4.0 / N);
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
#ifdef FIXED_POINT
    /* detect non-power of 2 */
    if(N & (N - 1)) {
        /* adjust scale for non-power of 2 MDCT = sqrt(2048/1920) */
        scale = MUL_C(scale, COEF_CONST(1.0327955589886444));
    }
#endif
#endif
    for(k = 0; k < N8; k++) { /* pre-FFT complex multiplication */
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 + n];
        IM(x) = X_in[N4 + n] - X_in[N4 - 1 - n];
        ComplexMult(&RE(m_Z1_mdct[k]), &IM(m_Z1_mdct[k]), RE(x), IM(x), RE(sincos[k]), IM(sincos[k]));
        RE(m_Z1_mdct[k]) = MUL_R(RE(m_Z1_mdct[k]), scale);
        IM(m_Z1_mdct[k]) = MUL_R(IM(m_Z1_mdct[k]), scale);
        RE(x) = X_in[N2 - 1 - n] - X_in[n];
        IM(x) = X_in[N2 + n] + X_in[N - 1 - n];
        ComplexMult(&RE(m_Z1_mdct[k + N8]), &IM(m_Z1_mdct[k + N8]), RE(x), IM(x), RE(sincos[k + N8]), IM(sincos[k + N8]));
        RE(m_Z1_mdct[k + N8]) = MUL_R(RE(m_Z1_mdct[k + N8]), scale);
        IM(m_Z1_mdct[k + N8]) = MUL_R(IM(m_Z1_mdct[k + N8]), scale);
    }
    /* complex FFT, any non-scaling FFT can be used here  */
    cfftf(mdct->cfft, m_Z1_mdct);
    /* post-FFT complex multiplication */
    for(k = 0; k < N4; k++) {
        uint16_t n = k << 1;
        ComplexMult(&RE(x), &IM(x), RE(m_Z1_mdct[k]), IM(m_Z1_mdct[k]), RE(sincos[k]), IM(sincos[k]));
        X_out[n] = -RE(x);
        X_out[N2 - 1 - n] = IM(x);
        X_out[N2 + n] = -IM(x);
        X_out[N - 1 - n] = RE(x);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* defines if an object type can be decoded by this library or not */
static uint8_t ObjectTypesTable[32] = {
    0, /*  0 NULL */
#ifdef MAIN_DEC
    1, /*  1 AAC Main */
#else
    0, /*  1 AAC Main */
#endif
    1, /*  2 AAC LC */
#ifdef SSR_DEC
    1, /*  3 AAC SSR */
#else
    0, /*  3 AAC SSR */
#endif
#ifdef LTP_DEC
    1, /*  4 AAC LTP */
#else
    0, /*  4 AAC LTP */
#endif
#ifdef SBR_DEC
    1, /*  5 SBR */
#else
    0, /*  5 SBR */
#endif
    0, /*  6 AAC Scalable */
    0, /*  7 TwinVQ */
    0, /*  8 CELP */
    0, /*  9 HVXC */
    0, /* 10 Reserved */
    0, /* 11 Reserved */
    0, /* 12 TTSI */
    0, /* 13 Main synthetic */
    0, /* 14 Wavetable synthesis */
    0, /* 15 General MIDI */
    0, /* 16 Algorithmic Synthesis and Audio FX */

    /* MPEG-4 Version 2 */
#ifdef ERROR_RESILIENCE
    1, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
#ifdef LTP_DEC
    1, /* 19 ER AAC LTP */
#else
    0, /* 19 ER AAC LTP */
#endif
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
#ifdef LD_DEC
    1, /* 23 ER AAC LD */
#else
    0, /* 23 ER AAC LD */
#endif
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#else /* No ER defined */
    0, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    0, /* 19 ER AAC LTP */
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    0, /* 23 ER AAC LD */
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#endif
    0, /* 28 (Reserved) */
#ifdef PS_DEC
    1, /* 29 AAC LC + SBR + PS */
#else
    0, /* 29 AAC LC + SBR + PS */
#endif
    0, /* 30 (Reserved) */
    0  /* 31 (Reserved) */
};
/* Table 1.6.1 */
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int8_t NeAACDecAudioSpecificConfig(uint8_t* pBuffer, uint32_t buffer_size) { return AudioSpecificConfig2(pBuffer, buffer_size, NULL, 0); }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int8_t AudioSpecificConfigFrombitfile(bitfile_t* ld, program_config_t* pce, uint32_t buffer_size, uint8_t short_form) {
    int8_t   result = 0;
    uint32_t startpos = faad_get_processed_bits(ld);
    (void)startpos;
    (void)buffer_size;
    (void)short_form;
#ifdef SBR_DEC
    int8_t bits_to_decode = 0;
#endif
    if(m_mp4ASC == NULL) return -8;
    memset(m_mp4ASC, 0, sizeof(mp4AudioSpecificConfig_t));
    m_mp4ASC->objectTypeIndex = (uint8_t)faad_getbits(ld, 5);
    m_mp4ASC->samplingFrequencyIndex = (uint8_t)faad_getbits(ld, 4);
    if(m_mp4ASC->samplingFrequencyIndex == 0x0f) faad_getbits(ld, 24);
    m_mp4ASC->channelsConfiguration = (uint8_t)faad_getbits(ld, 4);
    m_mp4ASC->samplingFrequency = get_sample_rate(m_mp4ASC->samplingFrequencyIndex);
    if(ObjectTypesTable[m_mp4ASC->objectTypeIndex] != 1) { return -1; }
    if(m_mp4ASC->samplingFrequency == 0) { return -2; }
    if(m_mp4ASC->channelsConfiguration > 7) { return -3; }
#if(defined(PS_DEC))
    /* check if we have a mono file */
    if(m_mp4ASC->channelsConfiguration == 1) {
        /* upMatrix to 2 channels for implicit signalling of PS */
        m_mp4ASC->channelsConfiguration = 2;
    }
#endif
#ifdef SBR_DEC
    m_mp4ASC->sbr_present_flag = -1;
    if(m_mp4ASC->objectTypeIndex == 5 || m_mp4ASC->objectTypeIndex == 29) {
        uint8_t tmp;
        m_mp4ASC->sbr_present_flag = 1;
        tmp = (uint8_t)faad_getbits(ld, 4);
        /* check for downsampled SBR */
        if(tmp == m_mp4ASC->samplingFrequencyIndex) m_mp4ASC->downSampledSBR = 1;
        m_mp4ASC->samplingFrequencyIndex = tmp;
        if(m_mp4ASC->samplingFrequencyIndex == 15) { m_mp4ASC->samplingFrequency = (uint32_t)faad_getbits(ld, 24); }
        else { m_mp4ASC->samplingFrequency = get_sample_rate(m_mp4ASC->samplingFrequencyIndex); }
        m_mp4ASC->objectTypeIndex = (uint8_t)faad_getbits(ld, 5);
    }
#endif
    /* get GASpecificConfig */
    if(m_mp4ASC->objectTypeIndex == 1 || m_mp4ASC->objectTypeIndex == 2 || m_mp4ASC->objectTypeIndex == 3 || m_mp4ASC->objectTypeIndex == 4 || m_mp4ASC->objectTypeIndex == 6 || m_mp4ASC->objectTypeIndex == 7) {
        result = GASpecificConfig(ld, pce);
#ifdef ERROR_RESILIENCE
    }
    else if(m_mp4ASC->objectTypeIndex >= ER_OBJECT_START) { /* ER */
        result = GASpecificConfig(ld, pce);
        m_mp4ASC->epConfig = (uint8_t)faad_getbits(ld, 2);
        if(m_mp4ASC->epConfig != 0) result = -5;
#endif
    }
    else { result = -4; }
#ifdef SBR_DEC
    if(short_form) bits_to_decode = 0;
    else
        bits_to_decode = (int8_t)(buffer_size * 8 - (startpos - faad_get_processed_bits(ld)));
    if((m_mp4ASC->objectTypeIndex != 5 && m_mp4ASC->objectTypeIndex != 29) && (bits_to_decode >= 16)) {
        int16_t syncExtensionType = (int16_t)faad_getbits(ld, 11);
        if(syncExtensionType == 0x2b7) {
            uint8_t tmp_OTi = (uint8_t)faad_getbits(ld, 5);
            if(tmp_OTi == 5) {
                m_mp4ASC->sbr_present_flag = (uint8_t)faad_get1bit(ld);
                if(m_mp4ASC->sbr_present_flag) {
                    uint8_t tmp;
                    /* Don't set OT to SBR until checked that it is actually there */
                    m_mp4ASC->objectTypeIndex = tmp_OTi;
                    tmp = (uint8_t)faad_getbits(ld, 4);
                    /* check for downsampled SBR */
                    if(tmp == m_mp4ASC->samplingFrequencyIndex) m_mp4ASC->downSampledSBR = 1;
                    m_mp4ASC->samplingFrequencyIndex = tmp;
                    if(m_mp4ASC->samplingFrequencyIndex == 15) { m_mp4ASC->samplingFrequency = (uint32_t)faad_getbits(ld, 24); }
                    else { m_mp4ASC->samplingFrequency = get_sample_rate(m_mp4ASC->samplingFrequencyIndex); }
                }
            }
        }
    }
    /* no SBR signalled, this could mean either implicit signalling or no SBR in this file */
    /* MPEG specification states: assume SBR on files with samplerate <= 24000 Hz */
    if(m_mp4ASC->sbr_present_flag == (int8_t)-1) /* cannot be -1 on systems with uint8_t*/
    {
        if(m_mp4ASC->samplingFrequency <= 24000) {
            m_mp4ASC->samplingFrequency *= 2;
            m_mp4ASC->forceUpSampling = 1;
        }
        else /* > 24000*/ { m_mp4ASC->downSampledSBR = 1; }
    }
#endif
    return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int8_t AudioSpecificConfig2(uint8_t* pBuffer, uint32_t buffer_size, program_config_t* pce, uint8_t short_form) {
    uint8_t   ret = 0;
    bitfile_t ld;
    memset(&ld, 0, sizeof(ld));
    faad_initbits(&ld, pBuffer, buffer_size);
    faad_byte_align(&ld);
    ret = AudioSpecificConfigFrombitfile(&ld, pce, buffer_size, short_form);
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void extract_envelope_data(sbr_info_t* sbr, uint8_t ch) {
    uint8_t l, k;

    for(l = 0; l < sbr->L_E[ch]; l++) {
        if(sbr->bs_df_env[ch][l] == 0) {
            for(k = 1; k < sbr->n[sbr->f[ch][l]]; k++) {
                sbr->E[ch][k][l] = sbr->E[ch][k - 1][l] + sbr->E[ch][k][l];
                if(sbr->E[ch][k][l] < 0) sbr->E[ch][k][l] = 0;
            }
        }
        else { /* bs_df_env == 1 */
            uint8_t g = (l == 0) ? sbr->f_prev[ch] : sbr->f[ch][l - 1];
            int16_t E_prev;
            if(sbr->f[ch][l] == g) {
                for(k = 0; k < sbr->n[sbr->f[ch][l]]; k++) {
                    if(l == 0) E_prev = sbr->E_prev[ch][k];
                    else
                        E_prev = sbr->E[ch][k][l - 1];
                    sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                }
            }
            else if((g == 1) && (sbr->f[ch][l] == 0)) {
                uint8_t i;
                for(k = 0; k < sbr->n[sbr->f[ch][l]]; k++) {
                    for(i = 0; i < sbr->N_high; i++) {
                        if(sbr->f_table_res[HI_RES][i] == sbr->f_table_res[LO_RES][k]) {
                            if(l == 0) E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];
                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }
            }
            else if((g == 0) && (sbr->f[ch][l] == 1)) {
                uint8_t i;
                for(k = 0; k < sbr->n[sbr->f[ch][l]]; k++) {
                    for(i = 0; i < sbr->N_low; i++) {
                        if((sbr->f_table_res[LO_RES][i] <= sbr->f_table_res[HI_RES][k]) && (sbr->f_table_res[HI_RES][k] < sbr->f_table_res[LO_RES][i + 1])) {
                            if(l == 0) E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];
                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void extract_noise_floor_data(sbr_info_t* sbr, uint8_t ch) {
    uint8_t l, k;

    for(l = 0; l < sbr->L_Q[ch]; l++) {
        if(sbr->bs_df_noise[ch][l] == 0) {
            for(k = 1; k < sbr->N_Q; k++) { sbr->Q[ch][k][l] = sbr->Q[ch][k][l] + sbr->Q[ch][k - 1][l]; }
        }
        else {
            if(l == 0) {
                for(k = 0; k < sbr->N_Q; k++) { sbr->Q[ch][k][l] = sbr->Q_prev[ch][k] + sbr->Q[ch][k][0]; }
            }
            else {
                for(k = 0; k < sbr->N_Q; k++) { sbr->Q[ch][k][l] = sbr->Q[ch][k][l - 1] + sbr->Q[ch][k][l]; }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint16_t ps_data(ps_info_t* ps, bitfile_t* ld, uint8_t* header) {
    uint8_t  tmp, n;
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);

    *header = 0;

    /* check for new PS header */
    if(faad_get1bit(ld)) {
        *header = 1;
        ps->header_read = 1;
        ps->use34hybrid_bands = 0;
        /* Inter-channel Intensity Difference (IID) parameters enabled */
        ps->enable_iid = (uint8_t)faad_get1bit(ld);
        if(ps->enable_iid) {
            ps->iid_mode = (uint8_t)faad_getbits(ld, 3);
            ps->nr_iid_par = nr_iid_par_tab[ps->iid_mode];
            ps->nr_ipdopd_par = nr_ipdopd_par_tab[ps->iid_mode];
            if(ps->iid_mode == 2 || ps->iid_mode == 5) ps->use34hybrid_bands = 1;
            /* IPD freq res equal to IID freq res */
            ps->ipd_mode = ps->iid_mode;
        }
        /* Inter-channel Coherence (ICC) parameters enabled */
        ps->enable_icc = (uint8_t)faad_get1bit(ld);
        if(ps->enable_icc) {
            ps->icc_mode = (uint8_t)faad_getbits(ld, 3);
            ps->nr_icc_par = nr_icc_par_tab[ps->icc_mode];
            if(ps->icc_mode == 2 || ps->icc_mode == 5) ps->use34hybrid_bands = 1;
        }
        /* PS extension layer enabled */
        ps->enable_ext = (uint8_t)faad_get1bit(ld);
    }
    /* we are here, but no header has been read yet */
    if(ps->header_read == 0) {
        ps->ps_data_available = 0;
        return 1;
    }
    ps->frame_class = (uint8_t)faad_get1bit(ld);
    tmp = (uint8_t)faad_getbits(ld, 2);
    ps->num_env = num_env_tab[ps->frame_class][tmp];
    if(ps->frame_class) {
        for(n = 1; n < ps->num_env + 1; n++) { ps->border_position[n] = (uint8_t)faad_getbits(ld, 5) + 1; }
    }
    if(ps->enable_iid) {
        for(n = 0; n < ps->num_env; n++) {
            ps->iid_dt[n] = (uint8_t)faad_get1bit(ld);
            /* iid_data */
            if(ps->iid_mode < 3) { huff_data(ld, ps->iid_dt[n], ps->nr_iid_par, t_huff_iid_def, f_huff_iid_def, ps->iid_index[n]); }
            else { huff_data(ld, ps->iid_dt[n], ps->nr_iid_par, t_huff_iid_fine, f_huff_iid_fine, ps->iid_index[n]); }
        }
    }
    if(ps->enable_icc) {
        for(n = 0; n < ps->num_env; n++) {
            ps->icc_dt[n] = (uint8_t)faad_get1bit(ld);

            /* icc_data */
            huff_data(ld, ps->icc_dt[n], ps->nr_icc_par, t_huff_icc, f_huff_icc, ps->icc_index[n]);
        }
    }
    if(ps->enable_ext) {
        uint16_t num_bits_left;
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4);
        if(cnt == 15) { cnt += (uint16_t)faad_getbits(ld, 8); }
        num_bits_left = 8 * cnt;
        while(num_bits_left > 7) {
            uint8_t ps_extension_id = (uint8_t)faad_getbits(ld, 2);
            num_bits_left -= 2;
            num_bits_left -= ps_extension(ps, ld, ps_extension_id, num_bits_left);
        }
        faad_getbits(ld, num_bits_left);
    }
    bits = (uint16_t)faad_get_processed_bits(ld) - bits;
    ps->ps_data_available = 1;
    return bits;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint16_t ps_extension(ps_info_t* ps, bitfile_t* ld, const uint8_t ps_extension_id, const uint16_t num_bits_left) {
    uint8_t n;
    (void)num_bits_left;
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);

    if(ps_extension_id == 0) {
        ps->enable_ipdopd = (uint8_t)faad_get1bit(ld);
        if(ps->enable_ipdopd) {
            for(n = 0; n < ps->num_env; n++) {
                ps->ipd_dt[n] = (uint8_t)faad_get1bit(ld);
                /* ipd_data */
                huff_data(ld, ps->ipd_dt[n], ps->nr_ipdopd_par, t_huff_ipd, f_huff_ipd, ps->ipd_index[n]);
                ps->opd_dt[n] = (uint8_t)faad_get1bit(ld);
                /* opd_data */
                huff_data(ld, ps->opd_dt[n], ps->nr_ipdopd_par, t_huff_opd, f_huff_opd, ps->opd_index[n]);
            }
        }
        faad_get1bit(ld);
    }
    /* return number of bits read */
    bits = (uint16_t)faad_get_processed_bits(ld) - bits;
    return bits;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* read huffman data coded in either the frequency or the time direction */
static void huff_data(bitfile_t* ld, const uint8_t dt, const uint8_t nr_par, ps_huff_tab t_huff, ps_huff_tab f_huff, int8_t* par) {
    uint8_t n;

    if(dt) {
        /* coded in time direction */
        for(n = 0; n < nr_par; n++) { par[n] = ps_huff_dec(ld, t_huff); }
    }
    else {
        /* coded in frequency direction */
        par[0] = ps_huff_dec(ld, f_huff);
        for(n = 1; n < nr_par; n++) { par[n] = ps_huff_dec(ld, f_huff); }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t pulse_decode(ic_stream_t* ics, int16_t* spec_data, uint16_t framelen) {
    uint8_t       i;
    uint16_t      k;
    pulse_info_t* pul = &(ics->pul);

    k = min(ics->swb_offset[pul->pulse_start_sfb], ics->swb_offset_max);
    for(i = 0; i <= pul->number_pulse; i++) {
        k += pul->pulse_offset[i];
        if(k >= framelen) return 15; /* should not be possible */
        if(spec_data[k] > 0) spec_data[k] += pul->pulse_amp[i];
        else
            spec_data[k] -= pul->pulse_amp[i];
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t rvlc_scale_factor_data(ic_stream_t* ics, bitfile_t* ld) {
    uint8_t bits = 9;

    ics->sf_concealment = faad_get1bit(ld);
    ics->rev_global_gain = (uint8_t)faad_getbits(ld, 8);
    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) bits = 11;
    /* the number of bits used for the huffman codewords */
    ics->length_of_rvlc_sf = (uint16_t)faad_getbits(ld, bits);
    if(ics->noise_used) {
        ics->dpcm_noise_nrg = (uint16_t)faad_getbits(ld, 9);
        ics->length_of_rvlc_sf -= 9;
    }
    ics->sf_escapes_present = faad_get1bit(ld);
    if(ics->sf_escapes_present) { ics->length_of_rvlc_escapes = (uint8_t)faad_getbits(ld, 8); }
    if(ics->noise_used) { ics->dpcm_noise_last_position = (uint16_t)faad_getbits(ld, 9); }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t rvlc_decode_scale_factors(ic_stream_t* ics, bitfile_t* ld) {
    uint8_t   result;
    uint8_t   intensity_used = 0;
    uint8_t*  rvlc_sf_buffer = NULL;
    uint8_t*  rvlc_esc_buffer = NULL;
    bitfile_t ld_rvlc_sf = {0, 0, 0, 0 ,0 ,0, 0, 0, 0}, ld_rvlc_esc = {0, 0, 0, 0 ,0 ,0, 0, 0, 0};
    //    bitfile_t ld_rvlc_sf_rev, ld_rvlc_esc_rev;

    if(ics->length_of_rvlc_sf > 0) {
        /* We read length_of_rvlc_sf bits here to put it in a seperate bitfile_t. */
        rvlc_sf_buffer = faad_getbitbuffer(ld, ics->length_of_rvlc_sf);
        faad_initbits(&ld_rvlc_sf, (void*)rvlc_sf_buffer, bit2byte(ics->length_of_rvlc_sf));
        //        faad_initbits_rev(&ld_rvlc_sf_rev, (void*)rvlc_sf_buffer,
        //            ics->length_of_rvlc_sf);
    }
    if(ics->sf_escapes_present) {
        /* We read length_of_rvlc_escapes bits here to put it in a seperate bitfile_t. */
        rvlc_esc_buffer = faad_getbitbuffer(ld, ics->length_of_rvlc_escapes);
        faad_initbits(&ld_rvlc_esc, (void*)rvlc_esc_buffer, bit2byte(ics->length_of_rvlc_escapes));
        //        faad_initbits_rev(&ld_rvlc_esc_rev, (void*)rvlc_esc_buffer, ics->length_of_rvlc_escapes);
    }
    /* decode the rvlc scale factors and escapes */
    result = rvlc_decode_sf_forward(ics, &ld_rvlc_sf, &ld_rvlc_esc, &intensity_used);
    //    result = rvlc_decode_sf_reverse(ics, &ld_rvlc_sf_rev, &ld_rvlc_esc_rev, intensity_used);
    if(rvlc_esc_buffer) faad_free(rvlc_esc_buffer);
    if(rvlc_sf_buffer) faad_free(rvlc_sf_buffer);
    if(ics->length_of_rvlc_sf > 0)
        if(ics->sf_escapes_present) return result;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t rvlc_decode_sf_forward(ic_stream_t* ics, bitfile_t* ld_sf, bitfile_t* ld_esc, uint8_t* intensity_used) {

    int8_t g, sfb;
    int8_t t = 0;
    int8_t error = ld_sf->error | ld_esc->error;
    int8_t noise_pcm_flag = 1;

    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->global_gain - 90 - 256;
    int16_t scale_factor_max = 255;

    for(g = 0; g < ics->num_window_groups; g++) {
        for(sfb = 0; sfb < ics->max_sfb; sfb++) {
            if(error) { ics->scale_factors[g][sfb] = 0; }
            else {
                switch(ics->sfb_cb[g][sfb]) {
                    case ZERO_HCB: /* zero book */ ics->scale_factors[g][sfb] = 0; break;
                    case INTENSITY_HCB: /* intensity books */
                    case INTENSITY_HCB2:
                        *intensity_used = 1;
                        /* decode intensity position */
                        t = rvlc_huffman_sf(ld_sf, ld_esc /*, +1*/);
                        is_position += t;
                        ics->scale_factors[g][sfb] = is_position;
                        break;
                    case NOISE_HCB: /* noise books */
                        /* decode noise energy */
                        if(noise_pcm_flag) {
                            int16_t n = ics->dpcm_noise_nrg;
                            noise_pcm_flag = 0;
                            noise_energy += n;
                        }
                        else {
                            t = rvlc_huffman_sf(ld_sf, ld_esc /*, +1*/);
                            noise_energy += t;
                        }
                        ics->scale_factors[g][sfb] = noise_energy;
                        break;
                    default: /* spectral books */
                        /* decode scale factor */
                        t = rvlc_huffman_sf(ld_sf, ld_esc /*, +1*/);
                        scale_factor += t;
                        if(scale_factor < 0 || scale_factor > 255) return 4;
                        ics->scale_factors[g][sfb] = min(scale_factor, scale_factor_max);
                        break;
                }
                if(t == 99) { error = 1; }
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int8_t rvlc_huffman_sf(bitfile_t *ld_sf, bitfile_t *ld_esc /*, int8_t direction*/){
    uint16_t                 i, j;
    int16_t                  index;
    uint32_t                 cw;
    const rvlc_huff_table_t* h = book_rvlc;
    int8_t                   direction = +1;

    i = h->len;
    if(direction > 0) cw = faad_getbits(ld_sf, i);
    else cw = 0 /* faad_getbits_rev(ld_sf, i DEBUGVAR(1,0,"")) */;

    while((cw != h->cw) && (i < 10)) {
        h++;
        j = h->len - i;
        i += j;
        cw <<= j;
        if(direction > 0) cw |= faad_getbits(ld_sf, j);
        else cw |= 0 /* faad_getbits_rev(ld_sf, j DEBUGVAR(1,0,"")) */;
    }

    index = h->index;

    if(index == +ESC_VAL) {
        int8_t esc = rvlc_huffman_esc(ld_esc /*, direction*/);
        if(esc == 99) return 99;
        index += esc;
    }
    if(index == -ESC_VAL) {
        int8_t esc = rvlc_huffman_esc(ld_esc /*, direction*/);
        if(esc == 99) return 99;
        index -= esc;
    }

    return (int8_t)index;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int8_t rvlc_huffman_esc(bitfile_t* ld /*, int8_t direction*/) {
    uint16_t                 i, j;
    uint32_t                 cw;
    const rvlc_huff_table_t* h = book_escape;
    int8_t                   direction = +1;

    i = h->len;
    if(direction > 0) cw = faad_getbits(ld, i);
    else cw = 0 /* faad_getbits_rev(ld, i DEBUGVAR(1,0,"")) */;

    while((cw != h->cw) && (i < 21)) {
        h++;
        j = h->len - i;
        i += j;
        cw <<= j;
        if(direction > 0) cw |= faad_getbits(ld, j);
        else cw |= 0 /* faad_getbits_rev(ld, j DEBUGVAR(1,0,"")) */;
    }

    return (int8_t)h->index;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// FFT decimation in frequency,  4*16*2+16=128+16=144 multiplications, 6*16*2+10*8+4*16*2=192+80+128=400 additions
static void fft_dif(int32_t* Real, int32_t* Imag) {
    int32_t  w_real, w_imag;                                     // For faster access
    int32_t  point1_real, point1_imag, point2_real, point2_imag; // For faster access
    uint32_t j, i, i2, w_index;                                  // Counters

    // First 2 stages of 32 point FFT decimation in frequency 4*16*2=64*2=128 multiplications, 6*16*2=96*2=192 additions. Stage 1 of 32 point FFT decimation in frequency
    for(i = 0; i < 16; i++) {
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 16;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        w_real = w_array_real[i];
        w_imag = w_array_imag[i];
        point1_real -= point2_real; // temp1 = x[i] - x[i2]
        point1_imag -= point2_imag;
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag)); // x[i2] = (x[i] - x[i2]) * w
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
    }
    // Stage 2 of 32 point FFT decimation in frequency
    for(j = 0, w_index = 0; j < 8; j++, w_index += 2) {
        w_real = w_array_real[w_index];
        w_imag = w_array_imag[w_index];
        i = j;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        point1_real -= point2_real; // temp1 = x[i] - x[i2]
        point1_imag -= point2_imag;
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag)); // x[i2] = (x[i] - x[i2]) * w
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
        i = j + 16;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        point1_real -= point2_real; // temp1 = x[i] - x[i2]
        point1_imag -= point2_imag;
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag)); // x[i2] = (x[i] - x[i2]) * w
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
    }
    // Stage 3 of 32 point FFT decimation in frequency, 2*4*2=16 multiplications, 4*4*2+6*4*2=10*8=80 additions
    for(i = 0; i < 32; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        Real[i] += point2_real; // out[i1] = point1 + point2
        Imag[i] += point2_imag;
        Real[i2] = point1_real - point2_real; // out[i2] = point1 - point2
        Imag[i2] = point1_imag - point2_imag;
    }
    w_real = w_array_real[4];    // = sqrt(2)/2
    for(i = 1; i < 32; i += 8) { // w_imag = -w_real; // = w_array_imag[4]; // = -sqrt(2)/2
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        point1_real -= point2_real; // temp1 = x[i] - x[i2]
        point1_imag -= point2_imag;
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = MUL_F(point1_real + point1_imag, w_real); // x[i2] = (x[i] - x[i2]) * w
        Imag[i2] = MUL_F(point1_imag - point1_real, w_real);
    }
    for(i = 2; i < 32; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        Real[i] += point2_real; // x[i] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = point1_imag - point2_imag; // x[i2] = (x[i] - x[i2]) * (-i)
        Imag[i2] = point2_real - point1_real;
    }
    w_real = w_array_real[12];   // = -sqrt(2)/2
    for(i = 3; i < 32; i += 8) { // w_imag = w_real; // = w_array_imag[12]; // = -sqrt(2)/2
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        point1_real -= point2_real; // temp1 = x[i] - x[i2]
        point1_imag -= point2_imag;
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = MUL_F(point1_real - point1_imag, w_real); // x[i2] = (x[i] - x[i2]) * w
        Imag[i2] = MUL_F(point1_real + point1_imag, w_real);
    }
    // Stage 4 of 32 point FFT decimation in frequency (no multiplications) 16*4=64 additions
    for(i = 0; i < 32; i += 4) {
        i2 = i + 2;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        Real[i] += point2_real; // x[i1] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = point1_real - point2_real; // x[i2] = x[i] - x[i2]
        Imag[i2] = point1_imag - point2_imag;
    }
    for(i = 1; i < 32; i += 4) {
        i2 = i + 2;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        Real[i] += point2_real; // x[i] = x[i] + x[i2]
        Imag[i] += point2_imag;
        Real[i2] = point1_imag - point2_imag; // x[i2] = (x[i] - x[i2]) * (-i)
        Imag[i2] = point2_real - point1_real;
    }
    // Stage 5 of 32 point FFT decimation in frequency (no multiplications) 16*4=64 additions
    for(i = 0; i < 32; i += 2) {
        i2 = i + 1;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        Real[i] += point2_real; // out[i1] = point1 + point2
        Imag[i] += point2_imag;
        Real[i2] = point1_real - point2_real; // out[i2] = point1 - point2
        Imag[i2] = point1_imag - point2_imag;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* size 64 only! */
void dct4_kernel(int32_t* in_real, int32_t* in_imag, int32_t* out_real, int32_t* out_imag) {
    // Tables with bit reverse values for 5 bits, bit reverse of i at i-th position
    const uint8_t bit_rev_tab[32] = {0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31};
    uint32_t      i, i_rev;

    /* Step 2: modulate */
    // 3*32=96 multiplications
    // 3*32=96 additions
    for(i = 0; i < 32; i++) {
        int32_t x_re, x_im, tmp;
        x_re = in_real[i];
        x_im = in_imag[i];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i]);
        in_real[i] = MUL_C(x_im, dct4_64_tab[i + 64]) + tmp;
        in_imag[i] = MUL_C(x_re, dct4_64_tab[i + 32]) + tmp;
    }
    /* Step 3: FFT, but with output in bit reverse order */
    fft_dif(in_real, in_imag);
    /* Step 4: modulate + bitreverse reordering */
    // 3*31+2=95 multiplications
    // 3*31+2=95 additions
    for(i = 0; i < 16; i++) {
        int32_t x_re, x_im, tmp;
        i_rev = bit_rev_tab[i];
        x_re = in_real[i_rev];
        x_im = in_imag[i_rev];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i + 3 * 32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5 * 32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4 * 32]) + tmp;
    }
    // i = 16, i_rev = 1 = rev(16);
    out_imag[16] = MUL_C(in_imag[1] - in_real[1], dct4_64_tab[16 + 3 * 32]);
    out_real[16] = MUL_C(in_real[1] + in_imag[1], dct4_64_tab[16 + 3 * 32]);
    for(i = 17; i < 32; i++) {
        int32_t x_re, x_im, tmp;
        i_rev = bit_rev_tab[i];
        x_re = in_real[i_rev];
        x_im = in_imag[i_rev];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i + 3 * 32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5 * 32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4 * 32]) + tmp;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
sbr_info_t* sbrDecodeInit(uint16_t framelength, uint8_t id_aac, uint32_t sample_rate, uint8_t downSampledSBR) {
    sbr_info_t* sbr = (sbr_info_t*)faad_malloc(sizeof(sbr_info_t));
    memset(sbr, 0, sizeof(sbr_info_t));

    /* save id of the parent element */
    sbr->id_aac = id_aac;
    sbr->sample_rate = sample_rate;
    sbr->bs_freq_scale = 2;
    sbr->bs_alter_scale = 1;
    sbr->bs_noise_bands = 2;
    sbr->bs_limiter_bands = 2;
    sbr->bs_limiter_gains = 2;
    sbr->bs_interpol_freq = 1;
    sbr->bs_smoothing_mode = 1;
    sbr->bs_start_freq = 5;
    sbr->bs_amp_res = 1;
    sbr->bs_samplerate_mode = 1;
    sbr->prevEnvIsShort[0] = -1;
    sbr->prevEnvIsShort[1] = -1;
    sbr->header_count = 0;
    sbr->Reset = 1;
    sbr->tHFGen = T_HFGEN;
    sbr->tHFAdj = T_HFADJ;
    sbr->bsco = 0;
    sbr->bsco_prev = 0;
    sbr->M_prev = 0;
    sbr->frame_len = framelength;
    sbr->bs_start_freq_prev = -1; /* force sbr reset */
    if(framelength == 960) {
        sbr->numTimeSlotsRate = RATE * NO_TIME_SLOTS_960;
        sbr->numTimeSlots = NO_TIME_SLOTS_960;
    }
    else if(framelength == 1024) {
        sbr->numTimeSlotsRate = RATE * NO_TIME_SLOTS;
        sbr->numTimeSlots = NO_TIME_SLOTS;
    }
    else {
        faad_free(sbr);
        return NULL;
    }
    sbr->GQ_ringbuf_index[0] = 0;
    sbr->GQ_ringbuf_index[1] = 0;
    if(id_aac == ID_CPE) {
        /* stereo */
        uint8_t j;
        sbr->qmfa[0] = qmfa_init(32);
        sbr->qmfa[1] = qmfa_init(32);
        sbr->qmfs[0] = qmfs_init((downSampledSBR) ? 32 : 64);
        sbr->qmfs[1] = qmfs_init((downSampledSBR) ? 32 : 64);
        for(j = 0; j < 5; j++) {
            sbr->G_temp_prev[0][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
            sbr->G_temp_prev[1][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
            sbr->Q_temp_prev[0][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
            sbr->Q_temp_prev[1][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
        }
        memset(sbr->Xsbr[0], 0, (sbr->numTimeSlotsRate + sbr->tHFGen) * 64 * sizeof(complex_t));
        memset(sbr->Xsbr[1], 0, (sbr->numTimeSlotsRate + sbr->tHFGen) * 64 * sizeof(complex_t));
    }
    else {
        /* mono */
        uint8_t j;
        sbr->qmfa[0] = qmfa_init(32);
        sbr->qmfs[0] = qmfs_init((downSampledSBR) ? 32 : 64);
        sbr->qmfs[1] = NULL;
        for(j = 0; j < 5; j++) {
            sbr->G_temp_prev[0][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
            sbr->Q_temp_prev[0][j] = (int32_t*)faad_malloc(64 * sizeof(int32_t));
        }
        memset(sbr->Xsbr[0], 0, (sbr->numTimeSlotsRate + sbr->tHFGen) * 64 * sizeof(complex_t));
    }
    return sbr;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void sbrDecodeEnd(sbr_info_t* sbr) {
    uint8_t j;

    if(sbr) {
        qmfa_end(sbr->qmfa[0]);
        qmfs_end(sbr->qmfs[0]);
        if(sbr->qmfs[1] != NULL) {
            qmfa_end(sbr->qmfa[1]);
            qmfs_end(sbr->qmfs[1]);
        }
        for(j = 0; j < 5; j++) {
            if(sbr->G_temp_prev[0][j]) faad_free(sbr->G_temp_prev[0][j]);
            if(sbr->Q_temp_prev[0][j]) faad_free(sbr->Q_temp_prev[0][j]);
            if(sbr->G_temp_prev[1][j]) faad_free(sbr->G_temp_prev[1][j]);
            if(sbr->Q_temp_prev[1][j]) faad_free(sbr->Q_temp_prev[1][j]);
        }
#ifdef PS_DEC
        if(sbr->ps != NULL) ps_free(sbr->ps);
#endif
        faad_free(sbr);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void sbrReset(sbr_info_t* sbr) {
    uint8_t j;
    if(sbr->qmfa[0] != NULL) memset(sbr->qmfa[0]->x, 0, 2 * sbr->qmfa[0]->channels * 10 * sizeof(int32_t));
    if(sbr->qmfa[1] != NULL) memset(sbr->qmfa[1]->x, 0, 2 * sbr->qmfa[1]->channels * 10 * sizeof(int32_t));
    if(sbr->qmfs[0] != NULL) memset(sbr->qmfs[0]->v, 0, 2 * sbr->qmfs[0]->channels * 20 * sizeof(int32_t));
    if(sbr->qmfs[1] != NULL) memset(sbr->qmfs[1]->v, 0, 2 * sbr->qmfs[1]->channels * 20 * sizeof(int32_t));
    for(j = 0; j < 5; j++) {
        if(sbr->G_temp_prev[0][j] != NULL) memset(sbr->G_temp_prev[0][j], 0, 64 * sizeof(int32_t));
        if(sbr->G_temp_prev[1][j] != NULL) memset(sbr->G_temp_prev[1][j], 0, 64 * sizeof(int32_t));
        if(sbr->Q_temp_prev[0][j] != NULL) memset(sbr->Q_temp_prev[0][j], 0, 64 * sizeof(int32_t));
        if(sbr->Q_temp_prev[1][j] != NULL) memset(sbr->Q_temp_prev[1][j], 0, 64 * sizeof(int32_t));
    }
    memset(sbr->Xsbr[0], 0, (sbr->numTimeSlotsRate + sbr->tHFGen) * 64 * sizeof(complex_t));
    memset(sbr->Xsbr[1], 0, (sbr->numTimeSlotsRate + sbr->tHFGen) * 64 * sizeof(complex_t));
    sbr->GQ_ringbuf_index[0] = 0;
    sbr->GQ_ringbuf_index[1] = 0;
    sbr->header_count = 0;
    sbr->Reset = 1;
    sbr->L_E_prev[0] = 0;
    sbr->L_E_prev[1] = 0;
    sbr->bs_freq_scale = 2;
    sbr->bs_alter_scale = 1;
    sbr->bs_noise_bands = 2;
    sbr->bs_limiter_bands = 2;
    sbr->bs_limiter_gains = 2;
    sbr->bs_interpol_freq = 1;
    sbr->bs_smoothing_mode = 1;
    sbr->bs_start_freq = 5;
    sbr->bs_amp_res = 1;
    sbr->bs_samplerate_mode = 1;
    sbr->prevEnvIsShort[0] = -1;
    sbr->prevEnvIsShort[1] = -1;
    sbr->bsco = 0;
    sbr->bsco_prev = 0;
    sbr->M_prev = 0;
    sbr->bs_start_freq_prev = -1;
    sbr->f_prev[0] = 0;
    sbr->f_prev[1] = 0;
    for(j = 0; j < MAX_M; j++) {
        sbr->E_prev[0][j] = 0;
        sbr->Q_prev[0][j] = 0;
        sbr->E_prev[1][j] = 0;
        sbr->Q_prev[1][j] = 0;
        sbr->bs_add_harmonic_prev[0][j] = 0;
        sbr->bs_add_harmonic_prev[1][j] = 0;
    }
    sbr->bs_add_harmonic_flag_prev[0] = 0;
    sbr->bs_add_harmonic_flag_prev[1] = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t sbr_save_prev_data(sbr_info_t* sbr, uint8_t ch) {
    uint8_t i;

    /* save data for next frame */
    sbr->kx_prev = sbr->kx;
    sbr->M_prev = sbr->M;
    sbr->bsco_prev = sbr->bsco;
    sbr->L_E_prev[ch] = sbr->L_E[ch];
    /* sbr->L_E[ch] can become 0 on files with bit errors */
    if(sbr->L_E[ch] <= 0) return 19;
    sbr->f_prev[ch] = sbr->f[ch][sbr->L_E[ch] - 1];
    for(i = 0; i < MAX_M; i++) {
        sbr->E_prev[ch][i] = sbr->E[ch][i][sbr->L_E[ch] - 1];
        sbr->Q_prev[ch][i] = sbr->Q[ch][i][sbr->L_Q[ch] - 1];
    }
    for(i = 0; i < MAX_M; i++) { sbr->bs_add_harmonic_prev[ch][i] = sbr->bs_add_harmonic[ch][i]; }
    sbr->bs_add_harmonic_flag_prev[ch] = sbr->bs_add_harmonic_flag[ch];
    if(sbr->l_A[ch] == sbr->L_E[ch]) sbr->prevEnvIsShort[ch] = 0;
    else
        sbr->prevEnvIsShort[ch] = -1;
    return 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void sbr_save_matrix(sbr_info_t* sbr, uint8_t ch) {
    uint8_t i;

    for(i = 0; i < sbr->tHFGen; i++) { memmove(sbr->Xsbr[ch][i], sbr->Xsbr[ch][i + sbr->numTimeSlotsRate], 64 * sizeof(complex_t)); }
    for(i = sbr->tHFGen; i < MAX_NTSRHFG; i++) { memset(sbr->Xsbr[ch][i], 0, 64 * sizeof(complex_t)); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t sbr_process_channel(sbr_info_t* sbr, int32_t* channel_buf, complex_t* X[64], uint8_t ch, uint8_t dont_process, const uint8_t downSampledSBR) {
    (void)downSampledSBR;
    int16_t k, l;
    uint8_t ret = 0;

    sbr->bsco = 0;
    /* subband analysis */
    if(dont_process) { sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], channel_buf, sbr->Xsbr[ch], sbr->tHFGen, 32); }
    else { sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], channel_buf, sbr->Xsbr[ch], sbr->tHFGen, sbr->kx); }
    if(!dont_process) {
        /* insert high frequencies here */
        /* hf generation using patching */
        hf_generation(sbr, sbr->Xsbr[ch], sbr->Xsbr[ch], ch);

        /* hf adjustment */
        ret = hf_adjustment(sbr, sbr->Xsbr[ch], ch);

        if(ret > 0) { dont_process = 1; }
    }
    if((sbr->just_seeked != 0) || dont_process) {
        for(l = 0; l < sbr->numTimeSlotsRate; l++) {
            for(k = 0; k < 32; k++) {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for(k = 32; k < 64; k++) {
                QMF_RE(X[l][k]) = 0;
                QMF_IM(X[l][k]) = 0;
            }
        }
    }
    else {
        for(l = 0; l < sbr->numTimeSlotsRate; l++) {
            uint8_t kx_band, M_band, bsco_band;
            if(l < sbr->t_E[ch][0]) {
                kx_band = sbr->kx_prev;
                M_band = sbr->M_prev;
                bsco_band = sbr->bsco_prev;
            }
            else {
                kx_band = sbr->kx;
                M_band = sbr->M;
                bsco_band = sbr->bsco;
            }
            for(k = 0; k < kx_band + bsco_band; k++) {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for(k = kx_band + bsco_band; k < kx_band + M_band; k++) {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for(k = max(kx_band + bsco_band, kx_band + M_band); k < 64; k++) {
                QMF_RE(X[l][k]) = 0;
                QMF_IM(X[l][k]) = 0;
            }
        }
    }
    return ret;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t sbrDecodeCoupleFrame(sbr_info_t* sbr, int32_t* left_chan, int32_t* right_chan, const uint8_t just_seeked, const uint8_t downSampledSBR) {

    uint8_t dont_process = 0;
    uint8_t ret = 0;
 //   for(uint8_t i = 0; i < MAX_NTSRHFG; i++) memset(m_X_dcf[i], 0, 64 * sizeof(*(m_X_dcf[i])));

    if(sbr == NULL) return 20;
    /* case can occur due to bit errors */
    if(sbr->id_aac != ID_CPE) return 21;

    if(sbr->ret || (sbr->header_count == 0)) {
        /* don't process just upsample */
        dont_process = 1;
        /* Re-activate reset for next frame */
        if(sbr->ret && sbr->Reset) sbr->bs_start_freq_prev = -1;
    }
    if(just_seeked) { sbr->just_seeked = 1; }
    else { sbr->just_seeked = 0; }
    sbr->ret += sbr_process_channel(sbr, left_chan, m_X_dcf, 0, dont_process, downSampledSBR);
    /* subband synthesis */
    if(downSampledSBR) { sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], m_X_dcf, left_chan); }
    else { sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], m_X_dcf, left_chan); }
    sbr->ret += sbr_process_channel(sbr, right_chan, m_X_dcf, 1, dont_process, downSampledSBR);
    /* subband synthesis */
    if(downSampledSBR) { sbr_qmf_synthesis_32(sbr, sbr->qmfs[1], m_X_dcf, right_chan); }
    else { sbr_qmf_synthesis_64(sbr, sbr->qmfs[1], m_X_dcf, right_chan); }

    if(sbr->bs_header_flag) sbr->just_seeked = 0;
    if(sbr->header_count != 0 && sbr->ret == 0) {
        ret = sbr_save_prev_data(sbr, 0);
        if(ret) return ret;
        ret = sbr_save_prev_data(sbr, 1);
        if(ret) return ret;
    }
    sbr_save_matrix(sbr, 0);
    sbr_save_matrix(sbr, 1);
    sbr->frame++;
    return 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t sbrDecodeSingleFrame(sbr_info_t* sbr, int32_t* channel, const uint8_t just_seeked, const uint8_t downSampledSBR) {
    //    printf(ANSI_ESC_YELLOW "sbrDecodeSingleFrame" ANSI_ESC_WHITE "\n");
    uint8_t dont_process = 0;
    uint8_t ret = 0;
//    for(uint8_t i = 0; i < MAX_NTSRHFG; i++) memset(m_X_dsf[i], 0, 64 * sizeof(*(m_X_dsf[i])));

    if(sbr == NULL) return 20;
    /* case can occur due to bit errors */
    if(sbr->id_aac != ID_SCE && sbr->id_aac != ID_LFE) return 21;

    if(sbr->ret || (sbr->header_count == 0)) {
        /* don't process just upsample */
        dont_process = 1;
        /* Re-activate reset for next frame */
        if(sbr->ret && sbr->Reset) sbr->bs_start_freq_prev = -1;
    }
    if(just_seeked) { sbr->just_seeked = 1; }
    else { sbr->just_seeked = 0; }
    sbr->ret += sbr_process_channel(sbr, channel, m_X_dsf, 0, dont_process, downSampledSBR);
    /* subband synthesis */
    if(downSampledSBR) { sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], m_X_dsf, channel); }
    else { sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], m_X_dsf, channel); }
    if(sbr->bs_header_flag) sbr->just_seeked = 0;

    if(sbr->header_count != 0 && sbr->ret == 0) {
        ret = sbr_save_prev_data(sbr, 0);
        if(ret) return ret;
    }
    sbr_save_matrix(sbr, 0);
    sbr->frame++;

    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t sbrDecodeSingleFramePS(sbr_info_t* sbr, int32_t* left_channel, int32_t* right_channel, const uint8_t just_seeked, const uint8_t downSampledSBR) {

    uint8_t l, k;
    uint8_t dont_process = 0;
    uint8_t ret = 0;

    for(uint8_t i = 0; i < 38; i++) memset(m_X_left[i], 0, 64 * sizeof(*(m_X_left[i])));
    for(uint8_t i = 0; i < 38; i++) memset(m_X_right[i], 0, 64 * sizeof(*(m_X_right[i])));

    if(sbr == NULL) return 20;
    /* case can occur due to bit errors */
    if(sbr->id_aac != ID_SCE && sbr->id_aac != ID_LFE) return 21;

    if(sbr->ret || (sbr->header_count == 0)) {
        /* don't process just upsample */
        dont_process = 1;
        /* Re-activate reset for next frame */
        if(sbr->ret && sbr->Reset) sbr->bs_start_freq_prev = -1;
    }
    if(just_seeked) { sbr->just_seeked = 1; }
    else { sbr->just_seeked = 0; }
    if(sbr->qmfs[1] == NULL) { sbr->qmfs[1] = qmfs_init((downSampledSBR) ? 32 : 64); }
    sbr->ret += sbr_process_channel(sbr, left_channel, m_X_left, 0, dont_process, downSampledSBR);
    /* copy some extra data for PS */
    for(l = sbr->numTimeSlotsRate; l < sbr->numTimeSlotsRate + 6; l++) {
        for(k = 0; k < 5; k++) {
            QMF_RE(m_X_left[l][k]) = QMF_RE(sbr->Xsbr[0][sbr->tHFAdj + l][k]);
            QMF_IM(m_X_left[l][k]) = QMF_IM(sbr->Xsbr[0][sbr->tHFAdj + l][k]);
        }
    }
/* perform parametric stereo */
#ifdef PS_DEC
    ps_decode(sbr->ps, m_X_left, m_X_right);
#endif
    /* subband synthesis */
    if(downSampledSBR) {
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], m_X_left, left_channel);
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[1], m_X_right, right_channel);
    }
    else {
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], m_X_left, left_channel);
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[1], m_X_right, right_channel);
    }

    if(sbr->bs_header_flag) sbr->just_seeked = 0;
    if(sbr->header_count != 0 && sbr->ret == 0) {
        ret = sbr_save_prev_data(sbr, 0);
        if(ret) return ret;
    }
    sbr_save_matrix(sbr, 0);
    sbr->frame++;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* calculate the start QMF channel for the master frequency band table */
/* parameter is also called k0 */
uint8_t qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode, uint32_t sample_rate) {
    static const uint8_t startMinTable[12] = {7, 7, 10, 11, 12, 16, 16, 17, 24, 32, 35, 48};
    static const uint8_t offsetIndexTable[12] = {5, 5, 4, 4, 4, 3, 2, 1, 0, 6, 6, 6};
    static const int8_t  offset[7][16] = {{-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7}, {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13},
                                          {-5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16},  {-6, -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16},
                                          {-4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20},  {-2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24},
                                          {0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24, 28, 33}};
    uint8_t              startMin = startMinTable[get_sr_index(sample_rate)];
    uint8_t              offsetIndex = offsetIndexTable[get_sr_index(sample_rate)];

    if(bs_samplerate_mode) { return startMin + offset[offsetIndex][bs_start_freq]; }
    else { return startMin + offset[6][bs_start_freq]; }
}

static int32_t longcmp(const void* a, const void* b) { return ((int32_t)(*(int32_t*)a - *(int32_t*)b)); }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* calculate the stop QMF channel for the master frequency band table parameter is also called k2 */
uint8_t qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate, uint8_t k0) {
    if(bs_stop_freq == 15) { return min(64, k0 * 3); }
    else if(bs_stop_freq == 14) { return min(64, k0 * 2); }
    else {
        static const uint8_t stopMinTable[12] = {13, 15, 20, 21, 23, 32, 32, 35, 48, 64, 70, 96};
        static const int8_t  offset[12][14] = {
            {0, 2, 4, 6, 8, 11, 14, 18, 22, 26, 31, 37, 44, 51}, {0, 2, 4, 6, 8, 11, 14, 18, 22, 26, 31, 36, 42, 49},     {0, 2, 4, 6, 8, 11, 14, 17, 21, 25, 29, 34, 39, 44},
            {0, 2, 4, 6, 8, 11, 14, 17, 20, 24, 28, 33, 38, 43}, {0, 2, 4, 6, 8, 11, 14, 17, 20, 24, 28, 32, 36, 41},     {0, 2, 4, 6, 8, 10, 12, 14, 17, 20, 23, 26, 29, 32},
            {0, 2, 4, 6, 8, 10, 12, 14, 17, 20, 23, 26, 29, 32}, {0, 1, 3, 5, 7, 9, 11, 13, 15, 17, 20, 23, 26, 29},      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},          {0, -1, -2, -3, -4, -5, -6, -6, -6, -6, -6, -6, -6, -6}, {0, -3, -6, -9, -12, -15, -18, -20, -22, -24, -26, -28, -30, -32}};
        uint8_t stopMin = stopMinTable[get_sr_index(sample_rate)];
        /* bs_stop_freq <= 13 */
        return min(64, stopMin + offset[get_sr_index(sample_rate)][min(bs_stop_freq, 13)]);
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* calculate the master frequency table from k0, k2, bs_freq_scale and bs_alter_scale version for bs_freq_scale = 0 */
uint8_t master_frequency_table_fs0(sbr_info_t* sbr, uint8_t k0, uint8_t k2, uint8_t bs_alter_scale) {
    int8_t   incr;
    uint8_t  k;
    uint8_t  dk;
    uint32_t nrBands, k2Achieved;
    int32_t  k2Diff, vDk[64] = {0};

    /* mft only defined for k2 > k0 */
    if(k2 <= k0) {
        sbr->N_master = 0;
        return 1;
    }
    dk = bs_alter_scale ? 2 : 1;
#if 0 /* replaced by float-less design */
    nrBands = 2 * (int32_t)((float)(k2-k0)/(dk*2) + (-1+dk)/2.0f);
#else
    if(bs_alter_scale) { nrBands = (((k2 - k0 + 2) >> 2) << 1); }
    else { nrBands = (((k2 - k0) >> 1) << 1); }
#endif
    nrBands = min(nrBands, 63);
    if(nrBands <= 0) return 1;
    k2Achieved = k0 + nrBands * dk;
    k2Diff = k2 - k2Achieved;
    for(k = 0; k < nrBands; k++) vDk[k] = dk;
    if(k2Diff) {
        incr = (k2Diff > 0) ? -1 : 1;
        k = (uint8_t)((k2Diff > 0) ? (nrBands - 1) : 0);
        while(k2Diff != 0) {
            vDk[k] -= incr;
            k += incr;
            k2Diff += incr;
        }
    }
    sbr->f_master[0] = k0;
    for(k = 1; k <= nrBands; k++) sbr->f_master[k] = (uint8_t)(sbr->f_master[k - 1] + vDk[k - 1]);
    sbr->N_master = (uint8_t)nrBands;
    sbr->N_master = (min(sbr->N_master, 64));
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* This function finds the number of bands using this formula: bands * log(a1/a0)/log(2.0) + 0.5 */
static int32_t find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1) {
#ifdef FIXED_POINT
    /* table with log2() values */
    static const int32_t log2Table[65] = {COEF_CONST(0.0),          COEF_CONST(0.0),          COEF_CONST(1.0000000000), COEF_CONST(1.5849625007), COEF_CONST(2.0000000000), COEF_CONST(2.3219280949),
                                          COEF_CONST(2.5849625007), COEF_CONST(2.8073549221), COEF_CONST(3.0000000000), COEF_CONST(3.1699250014), COEF_CONST(3.3219280949), COEF_CONST(3.4594316186),
                                          COEF_CONST(3.5849625007), COEF_CONST(3.7004397181), COEF_CONST(3.8073549221), COEF_CONST(3.9068905956), COEF_CONST(4.0000000000), COEF_CONST(4.0874628413),
                                          COEF_CONST(4.1699250014), COEF_CONST(4.2479275134), COEF_CONST(4.3219280949), COEF_CONST(4.3923174228), COEF_CONST(4.4594316186), COEF_CONST(4.5235619561),
                                          COEF_CONST(4.5849625007), COEF_CONST(4.6438561898), COEF_CONST(4.7004397181), COEF_CONST(4.7548875022), COEF_CONST(4.8073549221), COEF_CONST(4.8579809951),
                                          COEF_CONST(4.9068905956), COEF_CONST(4.9541963104), COEF_CONST(5.0000000000), COEF_CONST(5.0443941194), COEF_CONST(5.0874628413), COEF_CONST(5.1292830169),
                                          COEF_CONST(5.1699250014), COEF_CONST(5.2094533656), COEF_CONST(5.2479275134), COEF_CONST(5.2854022189), COEF_CONST(5.3219280949), COEF_CONST(5.3575520046),
                                          COEF_CONST(5.3923174228), COEF_CONST(5.4262647547), COEF_CONST(5.4594316186), COEF_CONST(5.4918530963), COEF_CONST(5.5235619561), COEF_CONST(5.5545888517),
                                          COEF_CONST(5.5849625007), COEF_CONST(5.6147098441), COEF_CONST(5.6438561898), COEF_CONST(5.6724253420), COEF_CONST(5.7004397181), COEF_CONST(5.7279204546),
                                          COEF_CONST(5.7548875022), COEF_CONST(5.7813597135), COEF_CONST(5.8073549221), COEF_CONST(5.8328900142), COEF_CONST(5.8579809951), COEF_CONST(5.8826430494),
                                          COEF_CONST(5.9068905956), COEF_CONST(5.9307373376), COEF_CONST(5.9541963104), COEF_CONST(5.9772799235), COEF_CONST(6.0)};
    int32_t              r0 = log2Table[a0]; /* coef */
    int32_t              r1 = log2Table[a1]; /* coef */
    int32_t              r2 = (r1 - r0);     /* coef */

    if(warp) r2 = MUL_C(r2, COEF_CONST(1.0 / 1.3));
    /* convert r2 to real and then multiply and round */
    r2 = (r2 >> (COEF_BITS - REAL_BITS)) * bands + (1 << (REAL_BITS - 1));
    return (r2 >> REAL_BITS);
#else
    real_t div = (real_t)log(2.0);
    if (warp) div *= (real_t)1.3;

    return (int32_t)(bands * log((float)a1/(float)a0)/div + 0.5);
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t find_initial_power(uint8_t bands, uint8_t a0, uint8_t a1) {
#ifdef FIXED_POINT
    /* table with log() values */
    static const int32_t logTable[65] = {COEF_CONST(0.0),          COEF_CONST(0.0),          COEF_CONST(0.6931471806), COEF_CONST(1.0986122887), COEF_CONST(1.3862943611), COEF_CONST(1.6094379124),
                                         COEF_CONST(1.7917594692), COEF_CONST(1.9459101491), COEF_CONST(2.0794415417), COEF_CONST(2.1972245773), COEF_CONST(2.3025850930), COEF_CONST(2.3978952728),
                                         COEF_CONST(2.4849066498), COEF_CONST(2.5649493575), COEF_CONST(2.6390573296), COEF_CONST(2.7080502011), COEF_CONST(2.7725887222), COEF_CONST(2.8332133441),
                                         COEF_CONST(2.8903717579), COEF_CONST(2.9444389792), COEF_CONST(2.9957322736), COEF_CONST(3.0445224377), COEF_CONST(3.0910424534), COEF_CONST(3.1354942159),
                                         COEF_CONST(3.1780538303), COEF_CONST(3.2188758249), COEF_CONST(3.2580965380), COEF_CONST(3.2958368660), COEF_CONST(3.3322045102), COEF_CONST(3.3672958300),
                                         COEF_CONST(3.4011973817), COEF_CONST(3.4339872045), COEF_CONST(3.4657359028), COEF_CONST(3.4965075615), COEF_CONST(3.5263605246), COEF_CONST(3.5553480615),
                                         COEF_CONST(3.5835189385), COEF_CONST(3.6109179126), COEF_CONST(3.6375861597), COEF_CONST(3.6635616461), COEF_CONST(3.6888794541), COEF_CONST(3.7135720667),
                                         COEF_CONST(3.7376696183), COEF_CONST(3.7612001157), COEF_CONST(3.7841896339), COEF_CONST(3.8066624898), COEF_CONST(3.8286413965), COEF_CONST(3.8501476017),
                                         COEF_CONST(3.8712010109), COEF_CONST(3.8918202981), COEF_CONST(3.9120230054), COEF_CONST(3.9318256327), COEF_CONST(3.9512437186), COEF_CONST(3.9702919136),
                                         COEF_CONST(3.9889840466), COEF_CONST(4.0073331852), COEF_CONST(4.0253516907), COEF_CONST(4.0430512678), COEF_CONST(4.0604430105), COEF_CONST(4.0775374439),
                                         COEF_CONST(4.0943445622), COEF_CONST(4.1108738642), COEF_CONST(4.1271343850), COEF_CONST(4.1431347264), COEF_CONST(4.158883083)};
    /* standard Taylor polynomial coefficients for exp(x) around 0 */
    /* a polynomial around x=1 is more precise, as most values are around 1.07,
       but this is just fine already */
    static const int32_t c1 = COEF_CONST(1.0);
    static const int32_t c2 = COEF_CONST(1.0 / 2.0);
    static const int32_t c3 = COEF_CONST(1.0 / 6.0);
    static const int32_t c4 = COEF_CONST(1.0 / 24.0);

    int32_t r0 = logTable[a0];      /* coef */
    int32_t r1 = logTable[a1];      /* coef */
    int32_t r2 = (r1 - r0) / bands; /* coef */
    int32_t rexp = c1 + MUL_C((c1 + MUL_C((c2 + MUL_C((c3 + MUL_C(c4, r2)), r2)), r2)), r2);
    return (rexp >> (COEF_BITS - REAL_BITS)); /* real */
#else
    return (real_t)pow((real_t)a1/(real_t)a0, 1.0/(real_t)bands);
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int int32cmp(const void *a, const void *b){
    return ((int)(*(int32_t*)a - *(int32_t*)b));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* version for bs_freq_scale > 0 */
uint8_t master_frequency_table(sbr_info_t* sbr, uint8_t k0, uint8_t k2, uint8_t bs_freq_scale, uint8_t bs_alter_scale) {
    uint8_t k, bands, twoRegions;
    uint8_t k1;
    uint8_t nrBand0, nrBand1;
    int32_t vDk0[64], vDk1[64];
    int32_t vk0[64], vk1[64];
    uint8_t temp1[] = {6, 5, 4};
    int32_t  q, qk;
    int32_t A_1;

    memset(m_vDk0, 0, 64 * sizeof(int32_t));
    memset(m_vDk1, 0, 64 * sizeof(int32_t));
    memset(m_vk0, 0, 64 * sizeof(int32_t));
    memset(m_vk1, 0, 64 * sizeof(int32_t));

#ifdef FIXED_POINT
    int32_t rk2, rk0;
#endif
    (void)bs_alter_scale; /* TODO: remove parameter? */

    /* mft only defined for k2 > k0 */
    if(k2 <= k0) {
        sbr->N_master = 0;
        return 1;
    }

    bands = temp1[bs_freq_scale - 1];

#ifdef FIXED_POINT
    rk0 = (int32_t)k0 << REAL_BITS;
    rk2 = (int32_t)k2 << REAL_BITS;
    if(rk2 > MUL_C(rk0, COEF_CONST(2.2449)))
#else
    if((float)k2 / (float)k0 > 2.2449)
#endif
    {
        twoRegions = 1;
        k1 = k0 << 1;
    }
    else {
        twoRegions = 0;
        k1 = k2;
    }

    nrBand0 = (uint8_t)(2 * find_bands(0, bands, k0, k1));
    nrBand0 = min(nrBand0, 63);
    if(nrBand0 <= 0) return 1;

    q = find_initial_power(nrBand0, k0, k1);
#ifdef FIXED_POINT
    qk = (int32_t)k0 << REAL_BITS;
    // A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
    A_1 = k0;
#else
    qk = REAL_CONST(k0);
    A_1 = (int32_t)(qk + .5);
#endif
    for(k = 0; k <= nrBand0; k++) {
        int32_t A_0 = A_1;
#ifdef FIXED_POINT
        qk = MUL_R(qk, q);
        A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
#else
        qk *= q;
        A_1 = (int32_t)(qk + 0.5);
#endif
        vDk0[k] = A_1 - A_0;
    }

    /* needed? */
    qsort(vDk0, nrBand0, sizeof(vDk0[0]), int32cmp);

    vk0[0] = k0;
    for(k = 1; k <= nrBand0; k++) {
        vk0[k] = vk0[k - 1] + vDk0[k - 1];
        if(vDk0[k - 1] == 0) return 1;
    }

    if(!twoRegions) {
        for(k = 0; k <= nrBand0; k++) sbr->f_master[k] = (uint8_t)vk0[k];

        sbr->N_master = nrBand0;
        sbr->N_master = min(sbr->N_master, 64);
        return 0;
    }

    nrBand1 = (uint8_t)(2 * find_bands(1 /* warped */, bands, k1, k2));
    nrBand1 = min(nrBand1, 63);

    q = find_initial_power(nrBand1, k1, k2);
#ifdef FIXED_POINT
    qk = (int32_t)k1 << REAL_BITS;
    // A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
    A_1 = k1;
#else
    qk = REAL_CONST(k1);
    A_1 = (int32_t)(qk + .5);
#endif
    for(k = 0; k <= nrBand1 - 1; k++) {
        int32_t A_0 = A_1;
#ifdef FIXED_POINT
        qk = MUL_R(qk, q);
        A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
#else
        qk *= q;
        A_1 = (int32_t)(qk + 0.5);
#endif
        vDk1[k] = A_1 - A_0;
    }

    if(vDk1[0] < vDk0[nrBand0 - 1]) {
        int32_t change;

        /* needed? */
        qsort(vDk1, nrBand1 + 1, sizeof(vDk1[0]), int32cmp);
        change = vDk0[nrBand0 - 1] - vDk1[0];
        vDk1[0] = vDk0[nrBand0 - 1];
        vDk1[nrBand1 - 1] = vDk1[nrBand1 - 1] - change;
    }

    /* needed? */
    qsort(vDk1, nrBand1, sizeof(vDk1[0]), int32cmp);
    vk1[0] = k1;
    for(k = 1; k <= nrBand1; k++) {
        vk1[k] = vk1[k - 1] + vDk1[k - 1];
        if(vDk1[k - 1] == 0) return 1;
    }

    sbr->N_master = nrBand0 + nrBand1;
    sbr->N_master = min(sbr->N_master, 64);
    for(k = 0; k <= nrBand0; k++) { sbr->f_master[k] = (uint8_t)vk0[k]; }
    for(k = nrBand0 + 1; k <= sbr->N_master; k++) { sbr->f_master[k] = (uint8_t)vk1[k - nrBand0]; }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* calculate the derived frequency border tables from f_master */
uint8_t derived_frequency_table(sbr_info_t* sbr, uint8_t bs_xover_band, uint8_t k2) {
    uint8_t  k, i;
    uint32_t minus;

    /* The following relation shall be satisfied: bs_xover_band < N_Master */
    if(sbr->N_master <= bs_xover_band) return 1;
    sbr->N_high = sbr->N_master - bs_xover_band;
    sbr->N_low = (sbr->N_high >> 1) + (sbr->N_high - ((sbr->N_high >> 1) << 1));
    sbr->n[0] = sbr->N_low;
    sbr->n[1] = sbr->N_high;
    for(k = 0; k <= sbr->N_high; k++) { sbr->f_table_res[HI_RES][k] = sbr->f_master[k + bs_xover_band]; }
    sbr->M = sbr->f_table_res[HI_RES][sbr->N_high] - sbr->f_table_res[HI_RES][0];
    if(sbr->M > MAX_M) return 1;
    sbr->kx = sbr->f_table_res[HI_RES][0];
    if(sbr->kx > 32) return 1;
    if(sbr->kx + sbr->M > 64) return 1;
    minus = (sbr->N_high & 1) ? 1 : 0;
    for(k = 0; k <= sbr->N_low; k++) {
        if(k == 0) i = 0;
        else
            i = (uint8_t)(2 * k - minus);
        sbr->f_table_res[LO_RES][k] = sbr->f_table_res[HI_RES][i];
    }
    sbr->N_Q = 0;
    if(sbr->bs_noise_bands == 0) { sbr->N_Q = 1; }
    else {
        sbr->N_Q = (uint8_t)(max(1, find_bands(0, sbr->bs_noise_bands, sbr->kx, k2)));
        sbr->N_Q = min(5, sbr->N_Q);
    }
    for(k = 0; k <= sbr->N_Q; k++) {
        if(k == 0) { i = 0; }
        else {
            /* i = i + (int32_t)((sbr->N_low - i)/(sbr->N_Q + 1 - k)); */
            i = i + (sbr->N_low - i) / (sbr->N_Q + 1 - k);
        }
        sbr->f_table_noise[k] = sbr->f_table_res[LO_RES][i];
    }
    /* build table for mapping k to g in hf patching */
    for(k = 0; k < 64; k++) {
        uint8_t g;
        for(g = 0; g < sbr->N_Q; g++) {
            if((sbr->f_table_noise[g] <= k) && (k < sbr->f_table_noise[g + 1])) {
                sbr->table_map_k_to_g[k] = g;
                break;
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* TODO: blegh, ugly Modified to calculate for all possible bs_limiter_bands always This reduces the number calls to this functions needed (now only on header reset) */
void limiter_frequency_table(sbr_info_t* sbr) {
    static const int32_t limiterBandsCompare[] = {REAL_CONST(1.327152), REAL_CONST(1.185093), REAL_CONST(1.119872)};

    uint8_t k, s;
    int8_t  nrLim;
    sbr->f_table_lim[0][0] = sbr->f_table_res[LO_RES][0] - sbr->kx;
    sbr->f_table_lim[0][1] = sbr->f_table_res[LO_RES][sbr->N_low] - sbr->kx;
    sbr->N_L[0] = 1;

    int32_t* m_lim_imTable = (int32_t*)faad_malloc(100 * sizeof(int32_t));
    uint8_t* m_patchBorders = (uint8_t*)faad_malloc(64 * sizeof(uint8_t));
    for(s = 1; s < 4; s++) {
        memset(m_lim_imTable, 0, 100 * sizeof(int32_t));
        memset(m_patchBorders, 0, 64 * sizeof(uint8_t));

        m_patchBorders[0] = sbr->kx;
        for(k = 1; k <= sbr->noPatches; k++) { m_patchBorders[k] = m_patchBorders[k - 1] + sbr->patchNoSubbands[k - 1]; }
        for(k = 0; k <= sbr->N_low; k++) { m_lim_imTable[k] = sbr->f_table_res[LO_RES][k]; }
        for(k = 1; k < sbr->noPatches; k++) { m_lim_imTable[k + sbr->N_low] = m_patchBorders[k]; }
        /* needed */
        qsort(m_lim_imTable, sbr->noPatches + sbr->N_low, sizeof(m_lim_imTable[0]), (__compar_fn_t)longcmp);
        k = 1;
        nrLim = sbr->noPatches + sbr->N_low - 1;
        if(nrLim < 0) // TODO: BIG FAT PROBLEM
            return;

    restart:
        if(k <= nrLim) {
            int32_t nOctaves;

            if(m_lim_imTable[k - 1] != 0)
#ifdef FIXED_POINT
                nOctaves = DIV_R((m_lim_imTable[k] << REAL_BITS), REAL_CONST(m_lim_imTable[k - 1]));
#else
                nOctaves = (real_t)limTable[k]/(real_t)limTable[k-1];
#endif
            else
                nOctaves = 0;
            if(nOctaves < limiterBandsCompare[s - 1]) {
                uint8_t i;
                if(m_lim_imTable[k] != m_lim_imTable[k - 1]) {
                    uint8_t found = 0, found2 = 0;
                    for(i = 0; i <= sbr->noPatches; i++) {
                        if(m_lim_imTable[k] == m_patchBorders[i]) found = 1;
                    }
                    if(found) {
                        found2 = 0;
                        for(i = 0; i <= sbr->noPatches; i++) {
                            if(m_lim_imTable[k - 1] == m_patchBorders[i]) found2 = 1;
                        }
                        if(found2) {
                            k++;
                            goto restart;
                        }
                        else {
                            /* remove (k-1)th element */
                            m_lim_imTable[k - 1] = sbr->f_table_res[LO_RES][sbr->N_low];
                            qsort(m_lim_imTable, sbr->noPatches + sbr->N_low, sizeof(m_lim_imTable[0]), (__compar_fn_t)longcmp);
                            nrLim--;
                            goto restart;
                        }
                    }
                }
                /* remove kth element */
                m_lim_imTable[k] = sbr->f_table_res[LO_RES][sbr->N_low];
                qsort(m_lim_imTable, nrLim, sizeof(m_lim_imTable[0]), (__compar_fn_t)longcmp);
                nrLim--;
                goto restart;
            }
            else {
                k++;
                goto restart;
            }
        }
        sbr->N_L[s] = nrLim;
        for(k = 0; k <= nrLim; k++) { sbr->f_table_lim[s][k] = m_lim_imTable[k] - sbr->kx; }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t hf_adjustment(sbr_info_t* sbr, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch) {

    memset(m_adj, 0, sizeof(sbr_hfadj_info_t));
    uint8_t ret = 0;

    if(sbr->bs_frame_class[ch] == FIXFIX) { sbr->l_A[ch] = -1; }
    else if(sbr->bs_frame_class[ch] == VARFIX) {
        if(sbr->bs_pointer[ch] > 1) sbr->l_A[ch] = sbr->bs_pointer[ch] - 1;
        else
            sbr->l_A[ch] = -1;
    }
    else {
        if(sbr->bs_pointer[ch] == 0) sbr->l_A[ch] = -1;
        else
            sbr->l_A[ch] = sbr->L_E[ch] + 1 - sbr->bs_pointer[ch];
    }
    ret = estimate_current_envelope(sbr, m_adj, Xsbr, ch);
    if(ret > 0) {
        return 1;
    }
    calculate_gain(sbr, m_adj, ch);
    hf_assembly(sbr, m_adj, Xsbr, ch);
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t get_S_mapped(sbr_info_t* sbr, uint8_t ch, uint8_t l, uint8_t current_band) {
    if(sbr->f[ch][l] == HI_RES) {
        /* in case of using f_table_high we just have 1 to 1 mapping from bs_add_harmonic[l][k] */
        if((l >= sbr->l_A[ch]) || (sbr->bs_add_harmonic_prev[ch][current_band] && sbr->bs_add_harmonic_flag_prev[ch])) { return sbr->bs_add_harmonic[ch][current_band]; }
    }
    else {
        uint8_t b, lb, ub;
        /* in case of f_table_low we check if any of the HI_RES bands within this LO_RES band has bs_add_harmonic[l][k] turned on
         * (note that borders in the LO_RES table are also present in the HI_RES table) find first HI_RES band in current LO_RES band */
        lb = 2 * current_band - ((sbr->N_high & 1) ? 1 : 0);
        /* find first HI_RES band in next LO_RES band */
        ub = 2 * (current_band + 1) - ((sbr->N_high & 1) ? 1 : 0);
        /* check all HI_RES bands in current LO_RES band for sinusoid */
        for(b = lb; b < ub; b++) {
            if((l >= sbr->l_A[ch]) || (sbr->bs_add_harmonic_prev[ch][b] && sbr->bs_add_harmonic_flag_prev[ch])) {
                if(sbr->bs_add_harmonic[ch][b] == 1) return 1;
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t estimate_current_envelope(sbr_info_t* sbr, sbr_hfadj_info_t* adj, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch) {
    (void)adj;
    uint8_t m, l, j, k, k_l, k_h, p;
    int32_t nrg, div;

    if(sbr->bs_interpol_freq == 1) {
        for(l = 0; l < sbr->L_E[ch]; l++) {
            uint8_t i, l_i, u_i;
            l_i = sbr->t_E[ch][l];
            u_i = sbr->t_E[ch][l + 1];
            div = (int32_t)(u_i - l_i);
            if(div <= 0) {log_e("div %i", div); div = 1;}
            for(m = 0; m < sbr->M; m++) {
                nrg = 0;
                for(i = l_i + sbr->tHFAdj; i < u_i + sbr->tHFAdj; i++) {
                    nrg += ((QMF_RE(Xsbr[i][m + sbr->kx]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) * ((QMF_RE(Xsbr[i][m + sbr->kx]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) +
                           ((QMF_IM(Xsbr[i][m + sbr->kx]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) * ((QMF_IM(Xsbr[i][m + sbr->kx]) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
                }
                sbr->E_curr[ch][m][l] = nrg / div;
            }
        }
    }
    else {
        for(l = 0; l < sbr->L_E[ch]; l++) {
            for(p = 0; p < sbr->n[sbr->f[ch][l]]; p++) {
                k_l = sbr->f_table_res[sbr->f[ch][l]][p];
                k_h = sbr->f_table_res[sbr->f[ch][l]][p + 1];
                for(k = k_l; k < k_h; k++) {
                    uint8_t i, l_i, u_i;
                    nrg = 0;
                    l_i = sbr->t_E[ch][l];
                    u_i = sbr->t_E[ch][l + 1];
                    div = (int32_t)((u_i - l_i) * (k_h - k_l));
                    if(div == 0) div = 1;
                    for(i = l_i + sbr->tHFAdj; i < u_i + sbr->tHFAdj; i++) {
                        for(j = k_l; j < k_h; j++) {
                            nrg += ((QMF_RE(Xsbr[i][j]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) * ((QMF_RE(Xsbr[i][j]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) +
                                   ((QMF_IM(Xsbr[i][j]) + (1 << (REAL_BITS - 1))) >> REAL_BITS) * ((QMF_IM(Xsbr[i][j]) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
                        }
                    }
                    sbr->E_curr[ch][k - sbr->kx][l] = nrg / div;
                }
            }
        }
    }
    return 0;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t find_log2_E(sbr_info_t* sbr, uint8_t k, uint8_t l, uint8_t ch) {
    /* check for coupled energy/noise data */
    if(sbr->bs_coupling == 1) {
        uint8_t amp0 = (sbr->amp_res[0]) ? 0 : 1;
        uint8_t amp1 = (sbr->amp_res[1]) ? 0 : 1;
        int32_t tmp = (7 << REAL_BITS) + (sbr->E[0][k][l] << (REAL_BITS - amp0));
        int32_t pan;

        /* E[1] should always be even so shifting is OK */
        uint8_t E = sbr->E[1][k][l] >> amp1;
        if(ch == 0) {
            if(E > 12) {
                /* negative */
                pan = pan_log2_tab[-12 + E];
            }
            else {
                /* positive */
                pan = pan_log2_tab[12 - E] + ((12 - E) << REAL_BITS);
            }
        }
        else {
            if(E < 12) {
                /* negative */
                pan = pan_log2_tab[-E + 12];
            }
            else {
                /* positive */
                pan = pan_log2_tab[E - 12] + ((E - 12) << REAL_BITS);
            }
        }
        /* tmp / pan in log2 */
        return tmp - pan;
    }
    else {
        uint8_t amp = (sbr->amp_res[ch]) ? 0 : 1;
        return (6 << REAL_BITS) + (sbr->E[ch][k][l] << (REAL_BITS - amp));
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t find_log2_Q(sbr_info_t* sbr, uint8_t k, uint8_t l, uint8_t ch) {
    /* check for coupled energy/noise data */
    if(sbr->bs_coupling == 1) {
        int32_t tmp = (7 << REAL_BITS) - (sbr->Q[0][k][l] << REAL_BITS);
        int32_t pan;
        uint8_t Q = sbr->Q[1][k][l];

        if(ch == 0) {
            if(Q > 12) {
                /* negative */
                pan = pan_log2_tab[-12 + Q];
            }
            else {
                /* positive */
                pan = pan_log2_tab[12 - Q] + ((12 - Q) << REAL_BITS);
            }
        }
        else {
            if(Q < 12) {
                /* negative */
                pan = pan_log2_tab[-Q + 12];
            }
            else {
                /* positive */
                pan = pan_log2_tab[Q - 12] + ((Q - 12) << REAL_BITS);
            }
        }
        /* tmp / pan in log2 */
        return tmp - pan;
    }
    else { return (6 << REAL_BITS) - (sbr->Q[ch][k][l] << REAL_BITS); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int32_t find_log2_Qplus1(sbr_info_t* sbr, uint8_t k, uint8_t l, uint8_t ch) {
    /* check for coupled energy/noise data */
    if(sbr->bs_coupling == 1) {
        if((sbr->Q[0][k][l] >= 0) && (sbr->Q[0][k][l] <= 30) && (sbr->Q[1][k][l] >= 0) && (sbr->Q[1][k][l] <= 24)) {
            if(ch == 0) { return log_Qplus1_pan[sbr->Q[0][k][l]][sbr->Q[1][k][l] >> 1]; }
            else { return log_Qplus1_pan[sbr->Q[0][k][l]][12 - (sbr->Q[1][k][l] >> 1)]; }
        }
        else { return 0; }
    }
    else {
        if(sbr->Q[ch][k][l] >= 0 && sbr->Q[ch][k][l] <= 30) { return log_Qplus1[sbr->Q[ch][k][l]]; }
        else { return 0; }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void calculate_gain(sbr_info_t* sbr, sbr_hfadj_info_t* adj, uint8_t ch) {
    /* log2 values of limiter gains */
    static int32_t limGain[] = {REAL_CONST(-1.0), REAL_CONST(0.0), REAL_CONST(1.0), REAL_CONST(33.219)};
    uint8_t        m, l, k;

    uint8_t current_t_noise_band = 0;
    uint8_t S_mapped;
    int32_t G_boost;

    for(l = 0; l < sbr->L_E[ch]; l++) {
        uint8_t current_f_noise_band = 0;
        uint8_t current_res_band = 0;
        uint8_t current_res_band2 = 0;
        uint8_t current_hi_res_band = 0;
        int32_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;
        S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);
        if(sbr->t_E[ch][l + 1] > sbr->t_Q[ch][current_t_noise_band + 1]) { current_t_noise_band++; }
        for(k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++) {
            int32_t Q_M = 0;
            int32_t G_max;
            int32_t den = 0;
            int32_t acc1 = 0;
            int32_t acc2 = 0;
            uint8_t current_res_band_size = 0;
            uint8_t Q_M_size = 0;
            uint8_t ml1, ml2;
            /* bounds of current limiter bands */
            ml1 = sbr->f_table_lim[sbr->bs_limiter_bands][k];
            ml2 = sbr->f_table_lim[sbr->bs_limiter_bands][k + 1];
            if(ml1 > MAX_M) ml1 = MAX_M;
            if(ml2 > MAX_M) ml2 = MAX_M;
            /* calculate the accumulated E_orig and E_curr over the limiter band */
            for(m = ml1; m < ml2; m++) {
                if((m + sbr->kx) < sbr->f_table_res[sbr->f[ch][l]][current_res_band + 1]) { current_res_band_size++; }
                else {
                    acc1 += pow2_int(-REAL_CONST(10) + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch));
                    current_res_band++;
                    current_res_band_size = 1;
                }
                acc2 += sbr->E_curr[ch][m][l];
            }
            acc1 += pow2_int(-REAL_CONST(10) + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch));
            if(acc1 == 0) acc1 = LOG2_MIN_INF;
            else
                acc1 = log2_int(acc1);
            /* calculate the maximum gain ratio of the energy of the original signal and the energy of the HF generated signal */
            G_max = acc1 - log2_int(acc2) + limGain[sbr->bs_limiter_gains];
            G_max = min(G_max, limGain[3]);
            for(m = ml1; m < ml2; m++) {
                int32_t G;
                int32_t E_curr, E_orig;
                int32_t Q_orig, Q_orig_plus1;
                uint8_t S_index_mapped;
                /* check if m is on a noise band border */
                if((m + sbr->kx) == sbr->f_table_noise[current_f_noise_band + 1]) {
                    /* step to next noise band */
                    current_f_noise_band++;
                }
                /* check if m is on a resolution band border */
                if((m + sbr->kx) == sbr->f_table_res[sbr->f[ch][l]][current_res_band2 + 1]) {
                    /* accumulate a whole range of equal Q_Ms */
                    if(Q_M_size > 0) den += pow2_int(log2_int_tab[Q_M_size] + Q_M);
                    Q_M_size = 0;
                    /* step to next resolution band */
                    current_res_band2++;
                    /* if we move to a new resolution band, we should check if we are going to add a sinusoid in this band */
                    S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);
                }
                /* check if m is on a HI_RES band border */
                if((m + sbr->kx) == sbr->f_table_res[HI_RES][current_hi_res_band + 1]) {
                    /* step to next HI_RES band */
                    current_hi_res_band++;
                }
                /* find S_index_mapped S_index_mapped can only be 1 for the m in the middle of the current HI_RES band  */
                S_index_mapped = 0;
                if((l >= sbr->l_A[ch]) || (sbr->bs_add_harmonic_prev[ch][current_hi_res_band] && sbr->bs_add_harmonic_flag_prev[ch])) {
                    /* find the middle subband of the HI_RES frequency band */
                    if((m + sbr->kx) == (sbr->f_table_res[HI_RES][current_hi_res_band + 1] + sbr->f_table_res[HI_RES][current_hi_res_band]) >> 1)
                        S_index_mapped = sbr->bs_add_harmonic[ch][current_hi_res_band];
                }
                /* find bitstream parameters */
                if(sbr->E_curr[ch][m][l] == 0) E_curr = LOG2_MIN_INF;
                else
                    E_curr = log2_int(sbr->E_curr[ch][m][l]);
                E_orig = -REAL_CONST(10) + find_log2_E(sbr, current_res_band2, l, ch);

                Q_orig = find_log2_Q(sbr, current_f_noise_band, current_t_noise_band, ch);
                Q_orig_plus1 = find_log2_Qplus1(sbr, current_f_noise_band, current_t_noise_band, ch);
                /* Q_M only depends on E_orig and Q_div2:
                 * since N_Q <= N_Low <= N_High we only need to recalculate Q_M on a change of current res band (HI or LO) */
                Q_M = E_orig + Q_orig - Q_orig_plus1;
                /* m_S_M only depends on E_orig, Q_div and S_index_mapped:
                 * S_index_mapped can only be non-zero once per HI_RES band */
                if(S_index_mapped == 0) { m_S_M[m] = LOG2_MIN_INF; /* -inf */ }
                else {
                    m_S_M[m] = E_orig - Q_orig_plus1;
                    /* accumulate sinusoid part of the total energy */
                    den += pow2_int(m_S_M[m]);
                }
                /* calculate gain */
                /* ratio of the energy of the original signal and the energy of the HF generated signal */
                /* E_curr here is officially E_curr+1 so the log2() of that can never be < 0 */
                /* scaled by -10 */
                G = E_orig - max(-REAL_CONST(10), E_curr);
                if((S_mapped == 0) && (delta == 1)) {
                    /* G = G * 1/(1+Q) */
                    G -= Q_orig_plus1;
                }
                else if(S_mapped == 1) {
                    /* G = G * Q/(1+Q) */
                    G += Q_orig - Q_orig_plus1;
                }
                /* limit the additional noise energy level and apply the limiter */
                if(G_max > G) {
                    m_Q_M_lim[m] = Q_M;
                    m_G_lim[m] = G;
                    if((S_index_mapped == 0) && (l != sbr->l_A[ch])) { Q_M_size++; }
                }
                else {
                    /* G > G_max */
                    m_Q_M_lim[m] = Q_M + G_max - G;
                    m_G_lim[m] = G_max;

                    /* accumulate limited Q_M */
                    if((S_index_mapped == 0) && (l != sbr->l_A[ch])) { den += pow2_int(m_Q_M_lim[m]); }
                }
                /* accumulate the total energy E_curr changes for every m so we do need to accumulate every m */
                den += pow2_int(E_curr + m_G_lim[m]);
            }
            /* accumulate last range of equal Q_Ms */
            if(Q_M_size > 0) { den += pow2_int(log2_int_tab[Q_M_size] + Q_M); }
            /* calculate the final gain */
            /* G_boost: [0..2.51188643] */
            G_boost = acc1 - log2_int(den);
            G_boost = min(G_boost, REAL_CONST(1.328771237) /* log2(1.584893192 ^ 2) */);
            for(m = ml1; m < ml2; m++) {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
                adj->G_lim_boost[l][m] = pow2_fix((m_G_lim[m] + G_boost) >> 1);
                adj->Q_M_lim_boost[l][m] = pow2_fix((m_Q_M_lim[m] + G_boost) >> 1);
                if(m_S_M[m] != LOG2_MIN_INF) { adj->S_M_boost[l][m] = pow2_int((m_S_M[m] + G_boost) >> 1); }
                else { adj->S_M_boost[l][m] = 0; }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void hf_assembly(sbr_info_t* sbr, sbr_hfadj_info_t* adj, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch) {
    static int32_t h_smooth[] = {FRAC_CONST(0.03183050093751), FRAC_CONST(0.11516383427084), FRAC_CONST(0.21816949906249), FRAC_CONST(0.30150283239582), FRAC_CONST(0.33333333333333)};
    static int8_t  phi_re[] = {1, 0, -1, 0};
    static int8_t  phi_im[] = {0, 1, 0, -1};

    uint8_t  m, l, i, n;
    uint16_t fIndexNoise = 0;
    uint8_t  fIndexSine = 0;
    uint8_t  assembly_reset = 0;
    int32_t  G_filt, Q_filt;
    uint8_t  h_SL;

    if(sbr->Reset == 1) {
        assembly_reset = 1;
        fIndexNoise = 0;
    }
    else { fIndexNoise = sbr->index_noise_prev[ch]; }
    fIndexSine = sbr->psi_is_prev[ch];
    for(l = 0; l < sbr->L_E[ch]; l++) {
        uint8_t no_noise = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 1 : 0;
        h_SL = (sbr->bs_smoothing_mode == 1) ? 0 : 4;
        h_SL = (no_noise ? 0 : h_SL);
        if(assembly_reset) {
            for(n = 0; n < 4; n++) {
                memcpy(sbr->G_temp_prev[ch][n], adj->G_lim_boost[l], sbr->M * sizeof(int32_t));
                memcpy(sbr->Q_temp_prev[ch][n], adj->Q_M_lim_boost[l], sbr->M * sizeof(int32_t));
            }
            /* reset ringbuffer index */
            sbr->GQ_ringbuf_index[ch] = 4;
            assembly_reset = 0;
        }
        for(i = sbr->t_E[ch][l]; i < sbr->t_E[ch][l + 1]; i++) {
            /* load new values into ringbuffer */
            memcpy(sbr->G_temp_prev[ch][sbr->GQ_ringbuf_index[ch]], adj->G_lim_boost[l], sbr->M * sizeof(int32_t));
            memcpy(sbr->Q_temp_prev[ch][sbr->GQ_ringbuf_index[ch]], adj->Q_M_lim_boost[l], sbr->M * sizeof(int32_t));
            for(m = 0; m < sbr->M; m++) {
                complex_t psi;
                G_filt = 0;
                Q_filt = 0;
                if(h_SL != 0) {
                    uint8_t ri = sbr->GQ_ringbuf_index[ch];
                    for(n = 0; n <= 4; n++) {
                        int32_t curr_h_smooth = h_smooth[n];
                        ri++;
                        if(ri >= 5) ri -= 5;
                        G_filt += MUL_F(sbr->G_temp_prev[ch][ri][m], curr_h_smooth);
                        Q_filt += MUL_F(sbr->Q_temp_prev[ch][ri][m], curr_h_smooth);
                    }
                }
                else {

                    G_filt = sbr->G_temp_prev[ch][sbr->GQ_ringbuf_index[ch]][m];
                    Q_filt = sbr->Q_temp_prev[ch][sbr->GQ_ringbuf_index[ch]][m];
                }
                Q_filt = (adj->S_M_boost[l][m] != 0 || no_noise) ? 0 : Q_filt;
                /* add noise to the output */
                fIndexNoise = (fIndexNoise + 1) & 511;
                /* the smoothed gain values are applied to Xsbr */
                /* V is defined, not calculated */
                // QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_Q2(G_filt, QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                //     + MUL_F(Q_filt, RE(V[fIndexNoise]));
                QMF_RE(Xsbr[i + sbr->tHFAdj][m + sbr->kx]) = MUL_R(G_filt, QMF_RE(Xsbr[i + sbr->tHFAdj][m + sbr->kx])) + MUL_F(Q_filt, RE(noise_V[fIndexNoise]));
                if(sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42) QMF_RE(Xsbr[i + sbr->tHFAdj][m + sbr->kx]) = 16428320;
                // QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_Q2(G_filt, QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                //     + MUL_F(Q_filt, IM(V[fIndexNoise]));
                QMF_IM(Xsbr[i + sbr->tHFAdj][m + sbr->kx]) = MUL_R(G_filt, QMF_IM(Xsbr[i + sbr->tHFAdj][m + sbr->kx])) + MUL_F(Q_filt, IM(noise_V[fIndexNoise]));
                {
                    int8_t rev = (((m + sbr->kx) & 1) ? -1 : 1);
                    QMF_RE(psi) = adj->S_M_boost[l][m] * phi_re[fIndexSine];
                    QMF_RE(Xsbr[i + sbr->tHFAdj][m + sbr->kx]) += (QMF_RE(psi) << REAL_BITS);
                    QMF_IM(psi) = rev * adj->S_M_boost[l][m] * phi_im[fIndexSine];
                    QMF_IM(Xsbr[i + sbr->tHFAdj][m + sbr->kx]) += (QMF_IM(psi) << REAL_BITS);
                }
            }
            fIndexSine = (fIndexSine + 1) & 3;
            /* update the ringbuffer index used for filtering G and Q with h_smooth */
            sbr->GQ_ringbuf_index[ch]++;
            if(sbr->GQ_ringbuf_index[ch] >= 5) sbr->GQ_ringbuf_index[ch] = 0;
        }
    }
    sbr->index_noise_prev[ch] = fIndexNoise;
    sbr->psi_is_prev[ch] = fIndexSine;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void hf_generation(sbr_info_t* sbr, complex_t Xlow[MAX_NTSRHFG][64], complex_t Xhigh[MAX_NTSRHFG][64], uint8_t ch) {
    uint8_t   l, i, x;
    complex_t alpha_0[64], alpha_1[64]; // ⏫⏫⏫
    uint8_t   offset = sbr->tHFAdj;
    uint8_t   first = sbr->t_E[ch][0];
    uint8_t   last = sbr->t_E[ch][sbr->L_E[ch]];

    calc_chirp_factors(sbr, ch);
    if((ch == 0) && (sbr->Reset)) patch_construction(sbr);
    /* calculate the prediction coefficients */
    /* actual HF generation */
    for(i = 0; i < sbr->noPatches; i++) {
        for(x = 0; x < sbr->patchNoSubbands[i]; x++) {
            int32_t a0_r, a0_i, a1_r, a1_i;
            int32_t bw, bw2;
            uint8_t q, p, k, g;
            /* find the low and high band for patching */
            k = sbr->kx + x;
            for(q = 0; q < i; q++) { k += sbr->patchNoSubbands[q]; }
            p = sbr->patchStartSubband[i] + x;
            g = sbr->table_map_k_to_g[k];
            bw = sbr->bwArray[ch][g];
            bw2 = MUL_C(bw, bw);
            /* do the patching */
            /* with or without filtering */
            if(bw2 > 0) {
                int32_t temp1_r, temp2_r, temp3_r;
                int32_t temp1_i, temp2_i, temp3_i;
                calc_prediction_coef(sbr, Xlow, alpha_0, alpha_1, p);
                a0_r = MUL_C(RE(alpha_0[p]), bw);
                a1_r = MUL_C(RE(alpha_1[p]), bw2);
                a0_i = MUL_C(IM(alpha_0[p]), bw);
                a1_i = MUL_C(IM(alpha_1[p]), bw2);
                temp2_r = QMF_RE(Xlow[first - 2 + offset][p]);
                temp3_r = QMF_RE(Xlow[first - 1 + offset][p]);
                temp2_i = QMF_IM(Xlow[first - 2 + offset][p]);
                temp3_i = QMF_IM(Xlow[first - 1 + offset][p]);
                for(l = first; l < last; l++) {
                    temp1_r = temp2_r;
                    temp2_r = temp3_r;
                    temp3_r = QMF_RE(Xlow[l + offset][p]);
                    temp1_i = temp2_i;
                    temp2_i = temp3_i;
                    temp3_i = QMF_IM(Xlow[l + offset][p]);
                    QMF_RE(Xhigh[l + offset][k]) = temp3_r + (MUL_R(a0_r, temp2_r) - MUL_R(a0_i, temp2_i) + MUL_R(a1_r, temp1_r) - MUL_R(a1_i, temp1_i));
                    QMF_IM(Xhigh[l + offset][k]) = temp3_i + (MUL_R(a0_i, temp2_r) + MUL_R(a0_r, temp2_i) + MUL_R(a1_i, temp1_r) + MUL_R(a1_r, temp1_i));
                }
            }
            else {
                for(l = first; l < last; l++) {
                    QMF_RE(Xhigh[l + offset][k]) = QMF_RE(Xlow[l + offset][p]);
                    QMF_IM(Xhigh[l + offset][k]) = QMF_IM(Xlow[l + offset][p]);
                }
            }
        }
    }
    if(sbr->Reset) { limiter_frequency_table(sbr); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void auto_correlation(sbr_info_t* sbr, acorr_coef_t* ac, complex_t buffer[MAX_NTSRHFG][64], uint8_t bd, uint8_t len) {
    int32_t r01r = 0, r01i = 0, r02r = 0, r02i = 0, r11r = 0;
    int32_t temp1_r, temp1_i, temp2_r, temp2_i, temp3_r, temp3_i, temp4_r, temp4_i, temp5_r, temp5_i;
#ifdef FIXED_POINT
    const int32_t rel = FRAC_CONST(0.999999); // 1 / (1 + 1e-6f);
    uint32_t     mask, exp;
    int32_t       half;
#else
    const real_t rel = 1 / (1 + 1e-6f);
#endif
    int8_t  j;
    uint8_t offset = sbr->tHFAdj;

#ifdef FIXED_POINT
    mask = 0;

    for(j = (offset - 2); j < (len + offset); j++) {
        int32_t x;
        x = QMF_RE(buffer[j][bd]) >> REAL_BITS;
        mask |= x ^ (x >> 31);
        x = QMF_IM(buffer[j][bd]) >> REAL_BITS;
        mask |= x ^ (x >> 31);
    }

    exp = wl_min_lzc(mask);

    /* All-zero input. */
    if(exp == 0) {
        RE(ac->r01) = 0;
        IM(ac->r01) = 0;
        RE(ac->r02) = 0;
        IM(ac->r02) = 0;
        RE(ac->r11) = 0;
        // IM(ac->r11) = 0; // unused
        RE(ac->r12) = 0;
        IM(ac->r12) = 0;
        RE(ac->r22) = 0;
        // IM(ac->r22) = 0; // unused
        ac->det = 0;
        return;
    }
    /* Otherwise exp > 0. */
    /* improves accuracy */
    exp -= 1;
    /* Now exp is 0..31 */
    half = (1 << exp) >> 1;

    temp2_r = (QMF_RE(buffer[offset - 2][bd]) + half) >> exp;
    temp2_i = (QMF_IM(buffer[offset - 2][bd]) + half) >> exp;
    temp3_r = (QMF_RE(buffer[offset - 1][bd]) + half) >> exp;
    temp3_i = (QMF_IM(buffer[offset - 1][bd]) + half) >> exp;
    // Save these because they are needed after loop
    temp4_r = temp2_r;
    temp4_i = temp2_i;
    temp5_r = temp3_r;
    temp5_i = temp3_i;

    for(j = offset; j < len + offset; j++) {
        temp1_r = temp2_r; // temp1_r = (QMF_RE(buffer[offset-2][bd] + (1<<(exp-1))) >> exp;
        temp1_i = temp2_i; // temp1_i = (QMF_IM(buffer[offset-2][bd] + (1<<(exp-1))) >> exp;
        temp2_r = temp3_r; // temp2_r = (QMF_RE(buffer[offset-1][bd] + (1<<(exp-1))) >> exp;
        temp2_i = temp3_i; // temp2_i = (QMF_IM(buffer[offset-1][bd] + (1<<(exp-1))) >> exp;
        temp3_r = (QMF_RE(buffer[j][bd]) + half) >> exp;
        temp3_i = (QMF_IM(buffer[j][bd]) + half) >> exp;
        r01r += MUL_R(temp3_r, temp2_r) + MUL_R(temp3_i, temp2_i);
        r01i += MUL_R(temp3_i, temp2_r) - MUL_R(temp3_r, temp2_i);
        r02r += MUL_R(temp3_r, temp1_r) + MUL_R(temp3_i, temp1_i);
        r02i += MUL_R(temp3_i, temp1_r) - MUL_R(temp3_r, temp1_i);
        r11r += MUL_R(temp2_r, temp2_r) + MUL_R(temp2_i, temp2_i);
    }

    // These are actual values in temporary variable at this point
    // temp1_r = (QMF_RE(buffer[len+offset-1-2][bd] + (1<<(exp-1))) >> exp;
    // temp1_i = (QMF_IM(buffer[len+offset-1-2][bd] + (1<<(exp-1))) >> exp;
    // temp2_r = (QMF_RE(buffer[len+offset-1-1][bd] + (1<<(exp-1))) >> exp;
    // temp2_i = (QMF_IM(buffer[len+offset-1-1][bd] + (1<<(exp-1))) >> exp;
    // temp3_r = (QMF_RE(buffer[len+offset-1][bd]) + (1<<(exp-1))) >> exp;
    // temp3_i = (QMF_IM(buffer[len+offset-1][bd]) + (1<<(exp-1))) >> exp;
    // temp4_r = (QMF_RE(buffer[offset-2][bd]) + (1<<(exp-1))) >> exp;
    // temp4_i = (QMF_IM(buffer[offset-2][bd]) + (1<<(exp-1))) >> exp;
    // temp5_r = (QMF_RE(buffer[offset-1][bd]) + (1<<(exp-1))) >> exp;
    // temp5_i = (QMF_IM(buffer[offset-1][bd]) + (1<<(exp-1))) >> exp;

    RE(ac->r12) = r01r - (MUL_R(temp3_r, temp2_r) + MUL_R(temp3_i, temp2_i)) + (MUL_R(temp5_r, temp4_r) + MUL_R(temp5_i, temp4_i));
    IM(ac->r12) = r01i - (MUL_R(temp3_i, temp2_r) - MUL_R(temp3_r, temp2_i)) + (MUL_R(temp5_i, temp4_r) - MUL_R(temp5_r, temp4_i));
    RE(ac->r22) = r11r - (MUL_R(temp2_r, temp2_r) + MUL_R(temp2_i, temp2_i)) + (MUL_R(temp4_r, temp4_r) + MUL_R(temp4_i, temp4_i));

#else

    temp2_r = QMF_RE(buffer[offset - 2][bd]);
    temp2_i = QMF_IM(buffer[offset - 2][bd]);
    temp3_r = QMF_RE(buffer[offset - 1][bd]);
    temp3_i = QMF_IM(buffer[offset - 1][bd]);
    // Save these because they are needed after loop
    temp4_r = temp2_r;
    temp4_i = temp2_i;
    temp5_r = temp3_r;
    temp5_i = temp3_i;

    for(j = offset; j < len + offset; j++) {
        temp1_r = temp2_r; // temp1_r = QMF_RE(buffer[j-2][bd];
        temp1_i = temp2_i; // temp1_i = QMF_IM(buffer[j-2][bd];
        temp2_r = temp3_r; // temp2_r = QMF_RE(buffer[j-1][bd];
        temp2_i = temp3_i; // temp2_i = QMF_IM(buffer[j-1][bd];
        temp3_r = QMF_RE(buffer[j][bd]);
        temp3_i = QMF_IM(buffer[j][bd]);
        r01r += temp3_r * temp2_r + temp3_i * temp2_i;
        r01i += temp3_i * temp2_r - temp3_r * temp2_i;
        r02r += temp3_r * temp1_r + temp3_i * temp1_i;
        r02i += temp3_i * temp1_r - temp3_r * temp1_i;
        r11r += temp2_r * temp2_r + temp2_i * temp2_i;
    }

    // These are actual values in temporary variable at this point
    // temp1_r = QMF_RE(buffer[len+offset-1-2][bd];
    // temp1_i = QMF_IM(buffer[len+offset-1-2][bd];
    // temp2_r = QMF_RE(buffer[len+offset-1-1][bd];
    // temp2_i = QMF_IM(buffer[len+offset-1-1][bd];
    // temp3_r = QMF_RE(buffer[len+offset-1][bd]);
    // temp3_i = QMF_IM(buffer[len+offset-1][bd]);
    // temp4_r = QMF_RE(buffer[offset-2][bd]);
    // temp4_i = QMF_IM(buffer[offset-2][bd]);
    // temp5_r = QMF_RE(buffer[offset-1][bd]);
    // temp5_i = QMF_IM(buffer[offset-1][bd]);

    RE(ac->r12) = r01r - (temp3_r * temp2_r + temp3_i * temp2_i) + (temp5_r * temp4_r + temp5_i * temp4_i);
    IM(ac->r12) = r01i - (temp3_i * temp2_r - temp3_r * temp2_i) + (temp5_i * temp4_r - temp5_r * temp4_i);
    RE(ac->r22) = r11r - (temp2_r * temp2_r + temp2_i * temp2_i) + (temp4_r * temp4_r + temp4_i * temp4_i);

#endif

    RE(ac->r01) = r01r;
    IM(ac->r01) = r01i;
    RE(ac->r02) = r02r;
    IM(ac->r02) = r02i;
    RE(ac->r11) = r11r;

    ac->det = MUL_R(RE(ac->r11), RE(ac->r22)) - MUL_F(rel, (MUL_R(RE(ac->r12), RE(ac->r12)) + MUL_R(IM(ac->r12), IM(ac->r12))));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void calc_prediction_coef(sbr_info_t* sbr, complex_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, uint8_t k) {
    int32_t      tmp;
    acorr_coef_t ac;

    auto_correlation(sbr, &ac, Xlow, k, sbr->numTimeSlotsRate + 6);

    if(ac.det == 0) {
        RE(alpha_1[k]) = 0;
        IM(alpha_1[k]) = 0;
    }
    else {
#ifdef FIXED_POINT
        tmp = (MUL_R(RE(ac.r01), RE(ac.r12)) - MUL_R(IM(ac.r01), IM(ac.r12)) - MUL_R(RE(ac.r02), RE(ac.r11)));
        RE(alpha_1[k]) = DIV_R(tmp, ac.det);
        tmp = (MUL_R(IM(ac.r01), RE(ac.r12)) + MUL_R(RE(ac.r01), IM(ac.r12)) - MUL_R(IM(ac.r02), RE(ac.r11)));
        IM(alpha_1[k]) = DIV_R(tmp, ac.det);
#else
        tmp = REAL_CONST(1.0) / ac.det;
        RE(alpha_1[k]) = (MUL_R(RE(ac.r01), RE(ac.r12)) - MUL_R(IM(ac.r01), IM(ac.r12)) - MUL_R(RE(ac.r02), RE(ac.r11))) * tmp;
        IM(alpha_1[k]) = (MUL_R(IM(ac.r01), RE(ac.r12)) + MUL_R(RE(ac.r01), IM(ac.r12)) - MUL_R(IM(ac.r02), RE(ac.r11))) * tmp;
#endif
    }

    if(RE(ac.r11) == 0) {
        RE(alpha_0[k]) = 0;
        IM(alpha_0[k]) = 0;
    }
    else {
#ifdef FIXED_POINT
        tmp = -(RE(ac.r01) + MUL_R(RE(alpha_1[k]), RE(ac.r12)) + MUL_R(IM(alpha_1[k]), IM(ac.r12)));
        RE(alpha_0[k]) = DIV_R(tmp, RE(ac.r11));
        tmp = -(IM(ac.r01) + MUL_R(IM(alpha_1[k]), RE(ac.r12)) - MUL_R(RE(alpha_1[k]), IM(ac.r12)));
        IM(alpha_0[k]) = DIV_R(tmp, RE(ac.r11));
#else
        tmp = 1.0f / RE(ac.r11);
        RE(alpha_0[k]) = -(RE(ac.r01) + MUL_R(RE(alpha_1[k]), RE(ac.r12)) + MUL_R(IM(alpha_1[k]), IM(ac.r12))) * tmp;
        IM(alpha_0[k]) = -(IM(ac.r01) + MUL_R(IM(alpha_1[k]), RE(ac.r12)) - MUL_R(RE(alpha_1[k]), IM(ac.r12))) * tmp;
#endif
    }

    /* Sanity check; important: use "yes" check to filter-out NaN values. */
    if((MUL_R(RE(alpha_0[k]), RE(alpha_0[k])) + MUL_R(IM(alpha_0[k]), IM(alpha_0[k])) <= REAL_CONST(16)) &&
       (MUL_R(RE(alpha_1[k]), RE(alpha_1[k])) + MUL_R(IM(alpha_1[k]), IM(alpha_1[k])) <= REAL_CONST(16)))
        return;
    /* Fallback */
    RE(alpha_0[k]) = 0;
    IM(alpha_0[k]) = 0;
    RE(alpha_1[k]) = 0;
    IM(alpha_1[k]) = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* FIXED POINT: bwArray = COEF */
static int32_t mapNewBw(uint8_t invf_mode, uint8_t invf_mode_prev) {
    switch(invf_mode) {
    case 1:                     /* LOW */
        if(invf_mode_prev == 0) /* NONE */
            return COEF_CONST(0.6);
        else
            return COEF_CONST(0.75);

    case 2: /* MID */ return COEF_CONST(0.9);

    case 3: /* HIGH */ return COEF_CONST(0.98);

    default:                    /* NONE */
        if(invf_mode_prev == 1) /* LOW */
            return COEF_CONST(0.6);
        else
            return COEF_CONST(0.0);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* FIXED POINT: bwArray = COEF */
static void calc_chirp_factors(sbr_info_t* sbr, uint8_t ch) {
    uint8_t i;

    for(i = 0; i < sbr->N_Q; i++) {
        sbr->bwArray[ch][i] = mapNewBw(sbr->bs_invf_mode[ch][i], sbr->bs_invf_mode_prev[ch][i]);
        if(sbr->bwArray[ch][i] < sbr->bwArray_prev[ch][i]) sbr->bwArray[ch][i] = MUL_F(sbr->bwArray[ch][i], FRAC_CONST(0.75)) + MUL_F(sbr->bwArray_prev[ch][i], FRAC_CONST(0.25));
        else
            sbr->bwArray[ch][i] = MUL_F(sbr->bwArray[ch][i], FRAC_CONST(0.90625)) + MUL_F(sbr->bwArray_prev[ch][i], FRAC_CONST(0.09375));
        if(sbr->bwArray[ch][i] < COEF_CONST(0.015625)) sbr->bwArray[ch][i] = COEF_CONST(0.0);
        if(sbr->bwArray[ch][i] >= COEF_CONST(0.99609375)) sbr->bwArray[ch][i] = COEF_CONST(0.99609375);
        sbr->bwArray_prev[ch][i] = sbr->bwArray[ch][i];
        sbr->bs_invf_mode_prev[ch][i] = sbr->bs_invf_mode[ch][i];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void patch_construction(sbr_info_t* sbr) {
    uint8_t i, k;
    uint8_t odd, sb;
    uint8_t msb = sbr->k0;
    uint8_t usb = sbr->kx;
    uint8_t goalSbTab[] = {21, 23, 32, 43, 46, 64, 85, 93, 128, 0, 0, 0};
    /* (uint8_t)(2.048e6/sbr->sample_rate + 0.5); */
    uint8_t goalSb = goalSbTab[get_sr_index(sbr->sample_rate)];

    sbr->noPatches = 0;
    if(goalSb < (sbr->kx + sbr->M)) {
        for(i = 0, k = 0; sbr->f_master[i] < goalSb; i++) k = i + 1;
    }
    else { k = sbr->N_master; }
    if(sbr->N_master == 0) {
        sbr->noPatches = 0;
        sbr->patchNoSubbands[0] = 0;
        sbr->patchStartSubband[0] = 0;
        return;
    }
    do {
        uint8_t j = k + 1;
        do {
            j--;
            sb = sbr->f_master[j];
            odd = (sb - 2 + sbr->k0) % 2;
        } while(sb > (sbr->k0 - 1 + msb - odd));
        sbr->patchNoSubbands[sbr->noPatches] = max(sb - usb, 0);
        sbr->patchStartSubband[sbr->noPatches] = sbr->k0 - odd - sbr->patchNoSubbands[sbr->noPatches];
        if(sbr->patchNoSubbands[sbr->noPatches] > 0) {
            usb = sb;
            msb = sb;
            sbr->noPatches++;
        }
        else { msb = sbr->kx; }
        if(sbr->f_master[k] - sb < 3) k = sbr->N_master;
    } while(sb != (sbr->kx + sbr->M));
    if((sbr->patchNoSubbands[sbr->noPatches - 1] < 3) && (sbr->noPatches > 1)) { sbr->noPatches--; }
    sbr->noPatches = min(sbr->noPatches, 5);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int16_t sbr_huff_dec(bitfile_t* ld, sbr_huff_tab t_huff) {
    uint8_t bit;
    int16_t index = 0;

    while(index >= 0) {
        bit = (uint8_t)faad_get1bit(ld);
        index = t_huff[index][bit];
    }
    return index + 64;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 10 */
void sbr_envelope(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t      env, band;
    int8_t       delta = 0;
    sbr_huff_tab t_huff, f_huff;

    if((sbr->L_E[ch] == 1) && (sbr->bs_frame_class[ch] == FIXFIX)) sbr->amp_res[ch] = 0;
    else
        sbr->amp_res[ch] = sbr->bs_amp_res;

    if((sbr->bs_coupling) && (ch == 1)) {
        delta = 1;
        if(sbr->amp_res[ch]) {
            t_huff = t_huffman_env_bal_3_0dB;
            f_huff = f_huffman_env_bal_3_0dB;
        }
        else {
            t_huff = t_huffman_env_bal_1_5dB;
            f_huff = f_huffman_env_bal_1_5dB;
        }
    }
    else {
        delta = 0;
        if(sbr->amp_res[ch]) {
            t_huff = t_huffman_env_3_0dB;
            f_huff = f_huffman_env_3_0dB;
        }
        else {
            t_huff = t_huffman_env_1_5dB;
            f_huff = f_huffman_env_1_5dB;
        }
    }

    for(env = 0; env < sbr->L_E[ch]; env++) {
        if(sbr->bs_df_env[ch][env] == 0) {
            if((sbr->bs_coupling == 1) && (ch == 1)) {
                if(sbr->amp_res[ch]) { sbr->E[ch][0][env] = (uint16_t)(faad_getbits(ld, 5) << delta); }
                else { sbr->E[ch][0][env] = (uint16_t)(faad_getbits(ld, 6) << delta); }
            }
            else {
                if(sbr->amp_res[ch]) { sbr->E[ch][0][env] = (uint16_t)(faad_getbits(ld, 6) << delta); }
                else { sbr->E[ch][0][env] = (uint16_t)(faad_getbits(ld, 7) << delta); }
            }
            for(band = 1; band < sbr->n[sbr->f[ch][env]]; band++) { sbr->E[ch][band][env] = (sbr_huff_dec(ld, f_huff)); }
        }
        else {
            for(band = 0; band < sbr->n[sbr->f[ch][env]]; band++) { sbr->E[ch][band][env] = (sbr_huff_dec(ld, t_huff)); }
        }
    }
    extract_envelope_data(sbr, ch);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 11 */
void sbr_noise(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t      noise, band;
    int8_t       delta = 0;
    sbr_huff_tab t_huff, f_huff;

    if((sbr->bs_coupling == 1) && (ch == 1)) {
        delta = 1;
        t_huff = t_huffman_noise_bal_3_0dB;
        f_huff = f_huffman_env_bal_3_0dB;
    }
    else {
        delta = 0;
        t_huff = t_huffman_noise_3_0dB;
        f_huff = f_huffman_env_3_0dB;
    }
    for(noise = 0; noise < sbr->L_Q[ch]; noise++) {
        if(sbr->bs_df_noise[ch][noise] == 0) {
            if((sbr->bs_coupling == 1) && (ch == 1)) { sbr->Q[ch][0][noise] = (faad_getbits(ld, 5) << delta); }
            else { sbr->Q[ch][0][noise] = (faad_getbits(ld, 5) << delta); }
            for(band = 1; band < sbr->N_Q; band++) { sbr->Q[ch][band][noise] = (sbr_huff_dec(ld, f_huff) << delta); }
        }
        else {
            for(band = 0; band < sbr->N_Q; band++) { sbr->Q[ch][band][noise] = (sbr_huff_dec(ld, t_huff) << delta); }
        }
    }
    extract_noise_floor_data(sbr, ch);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
qmfa_info_t* qmfa_init(uint8_t channels) {
    qmfa_info_t* qmfa = (qmfa_info_t*)faad_malloc(sizeof(qmfa_info_t));
    /* x is implemented as double ringbuffer */
    qmfa->x = (int32_t*)faad_malloc(2 * channels * 10 * sizeof(int32_t));
    memset(qmfa->x, 0, 2 * channels * 10 * sizeof(int32_t));
    /* ringbuffer index */
    qmfa->x_index = 0;
    qmfa->channels = channels;
    return qmfa;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void qmfa_end(qmfa_info_t* qmfa) {
    if(qmfa) {
        if(qmfa->x) faad_free(qmfa->x);
        faad_free(qmfa);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void sbr_qmf_analysis_32(sbr_info_t* sbr, qmfa_info_t* qmfa, const int32_t* input, complex_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx) {
    int32_t u[64];
#ifndef SBR_LOW_POWER
    int32_t in_real[32], in_imag[32], out_real[32], out_imag[32]; // ⏫⏫⏫ ??
#else
    int32_t y[32];
#endif
    uint32_t in = 0;
    uint8_t  l;

    /* qmf subsample l */
    for(l = 0; l < sbr->numTimeSlotsRate; l++) {
        int16_t n;

        /* shift input buffer x */
        /* input buffer is not shifted anymore, x is implemented as double ringbuffer */
        // memmove(qmfa->x + 32, qmfa->x, (320-32)*sizeof(real_t));

        /* add new samples to input buffer x */
        for(n = 32 - 1; n >= 0; n--) {
#ifdef FIXED_POINT
            qmfa->x[qmfa->x_index + n] = qmfa->x[qmfa->x_index + n + 320] = (input[in++]) >> 4;
#else
            qmfa->x[qmfa->x_index + n] = qmfa->x[qmfa->x_index + n + 320] = input[in++];
#endif
        }

        /* window and summation to create array u */
        for(n = 0; n < 64; n++) {
            u[n] = MUL_F(qmfa->x[qmfa->x_index + n], qmf_c[2 * n]) + MUL_F(qmfa->x[qmfa->x_index + n + 64], qmf_c[2 * (n + 64)]) + MUL_F(qmfa->x[qmfa->x_index + n + 128], qmf_c[2 * (n + 128)]) +
                   MUL_F(qmfa->x[qmfa->x_index + n + 192], qmf_c[2 * (n + 192)]) + MUL_F(qmfa->x[qmfa->x_index + n + 256], qmf_c[2 * (n + 256)]);
        }

        /* update ringbuffer index */
        qmfa->x_index -= 32;
        if(qmfa->x_index < 0) qmfa->x_index = (320 - 32);

            /* calculate 32 subband samples by introducing X */
#ifdef SBR_LOW_POWER
        y[0] = u[48];
        for(n = 1; n < 16; n++) y[n] = u[n + 48] + u[48 - n];
        for(n = 16; n < 32; n++) y[n] = -u[n - 16] + u[48 - n];

        DCT3_32_unscaled(u, y);

        for(n = 0; n < 32; n++) {
            if(n < kx) {
    #ifdef FIXED_POINT
                QMF_RE(X[l + offset][n]) = u[n] /*<< 1*/;
    #else
                QMF_RE(X[l + offset][n]) = 2. * u[n];
    #endif
            }
            else { QMF_RE(X[l + offset][n]) = 0; }
        }
#else
        // Reordering of data moved from DCT_IV to here
        in_imag[31] = u[1];
        in_real[0] = u[0];
        for(n = 1; n < 31; n++) {
            in_imag[31 - n] = u[n + 1];
            in_real[n] = -u[64 - n];
        }
        in_imag[0] = u[32];
        in_real[31] = -u[33];

        // dct4_kernel is DCT_IV without reordering which is done before and after FFT
        dct4_kernel(in_real, in_imag, out_real, out_imag);

        // Reordering of data moved from DCT_IV to here
        for(n = 0; n < 16; n++) {
            if(2 * n + 1 < kx) {
    #ifdef FIXED_POINT
                QMF_RE(X[l + offset][2 * n]) = out_real[n];
                QMF_IM(X[l + offset][2 * n]) = out_imag[n];
                QMF_RE(X[l + offset][2 * n + 1]) = -out_imag[31 - n];
                QMF_IM(X[l + offset][2 * n + 1]) = -out_real[31 - n];
    #else
                QMF_RE(X[l + offset][2 * n]) = 2. * out_real[n];
                QMF_IM(X[l + offset][2 * n]) = 2. * out_imag[n];
                QMF_RE(X[l + offset][2 * n + 1]) = -2. * out_imag[31 - n];
                QMF_IM(X[l + offset][2 * n + 1]) = -2. * out_real[31 - n];
    #endif
            }
            else {
                if(2 * n < kx) {
    #ifdef FIXED_POINT
                    QMF_RE(X[l + offset][2 * n]) = out_real[n];
                    QMF_IM(X[l + offset][2 * n]) = out_imag[n];
    #else
                    QMF_RE(X[l + offset][2 * n]) = 2. * out_real[n];
                    QMF_IM(X[l + offset][2 * n]) = 2. * out_imag[n];
    #endif
                }
                else {
                    QMF_RE(X[l + offset][2 * n]) = 0;
                    QMF_IM(X[l + offset][2 * n]) = 0;
                }
                QMF_RE(X[l + offset][2 * n + 1]) = 0;
                QMF_IM(X[l + offset][2 * n + 1]) = 0;
            }
        }
#endif
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
qmfs_info_t* qmfs_init(uint8_t channels) {
    qmfs_info_t* qmfs = (qmfs_info_t*)faad_malloc(sizeof(qmfs_info_t));
    /* v is a double ringbuffer */
    qmfs->v = (int32_t*)faad_malloc(2 * channels * 20 * sizeof(int32_t));
    memset(qmfs->v, 0, 2 * channels * 20 * sizeof(int32_t));
    qmfs->v_index = 0;
    qmfs->channels = channels;
    return qmfs;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void qmfs_end(qmfs_info_t* qmfs) {
    if(qmfs) {
        if(qmfs->v) faad_free(qmfs->v);
        faad_free(qmfs);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void sbr_qmf_synthesis_32(sbr_info_t* sbr, qmfs_info_t* qmfs, complex_t* X[64], int32_t* output) {
    int32_t x1[32], x2[32]; // ⏫⏫⏫
#ifndef FIXED_POINT
    int32_t scale = 1.f / 64.f;
#endif
    int32_t n, k, out = 0;
    uint8_t l;

    /* qmf subsample l */
    for(l = 0; l < sbr->numTimeSlotsRate; l++) {
        /* shift buffer v */
        /* buffer is not shifted, we are using a ringbuffer */
        // memmove(qmfs->v + 64, qmfs->v, (640-64)*sizeof(real_t));

        /* calculate 64 samples */
        /* complex pre-twiddle */
        for(k = 0; k < 32; k++) {
            x1[k] = MUL_F(QMF_RE(X[l][k]), RE(qmf32_pre_twiddle[k])) - MUL_F(QMF_IM(X[l][k]), IM(qmf32_pre_twiddle[k]));
            x2[k] = MUL_F(QMF_IM(X[l][k]), RE(qmf32_pre_twiddle[k])) + MUL_F(QMF_RE(X[l][k]), IM(qmf32_pre_twiddle[k]));

#ifndef FIXED_POINT
            x1[k] *= scale;
            x2[k] *= scale;
#else
            x1[k] >>= 1;
            x2[k] >>= 1;
#endif
        }
        /* transform */
        DCT4_32(x1, x1);
        DST4_32(x2, x2);

        for(n = 0; n < 32; n++) {
            qmfs->v[qmfs->v_index + n] = qmfs->v[qmfs->v_index + 640 + n] = -x1[n] + x2[n];
            qmfs->v[qmfs->v_index + 63 - n] = qmfs->v[qmfs->v_index + 640 + 63 - n] = x1[n] + x2[n];
        }

        /* calculate 32 output samples and window */
        for(k = 0; k < 32; k++) {
            output[out++] = MUL_F(qmfs->v[qmfs->v_index + k], qmf_c[2 * k]) + MUL_F(qmfs->v[qmfs->v_index + 96 + k], qmf_c[64 + 2 * k]) + MUL_F(qmfs->v[qmfs->v_index + 128 + k], qmf_c[128 + 2 * k]) +
                            MUL_F(qmfs->v[qmfs->v_index + 224 + k], qmf_c[192 + 2 * k]) + MUL_F(qmfs->v[qmfs->v_index + 256 + k], qmf_c[256 + 2 * k]) +
                            MUL_F(qmfs->v[qmfs->v_index + 352 + k], qmf_c[320 + 2 * k]) + MUL_F(qmfs->v[qmfs->v_index + 384 + k], qmf_c[384 + 2 * k]) +
                            MUL_F(qmfs->v[qmfs->v_index + 480 + k], qmf_c[448 + 2 * k]) + MUL_F(qmfs->v[qmfs->v_index + 512 + k], qmf_c[512 + 2 * k]) +
                            MUL_F(qmfs->v[qmfs->v_index + 608 + k], qmf_c[576 + 2 * k]);
        }

        /* update ringbuffer index */
        qmfs->v_index -= 64;
        if(qmfs->v_index < 0) qmfs->v_index = (640 - 64);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void sbr_qmf_synthesis_64(sbr_info_t* sbr, qmfs_info_t* qmfs, complex_t* X[64], int32_t* output) {
//    ALIGN real_t x1[64], x2[64];
#ifndef SBR_LOW_POWER
    int32_t in_real1[32], in_imag1[32], out_real1[32], out_imag1[32];
    int32_t in_real2[32], in_imag2[32], out_real2[32], out_imag2[32];
#endif
    complex_t* pX;
    int32_t *  pring_buffer_1, *pring_buffer_3;
//    real_t * ptemp_1, * ptemp_2;
#ifdef PREFER_POINTERS
    // These pointers are used if target platform has autoinc address generators
    int32_t *      pring_buffer_2, *pring_buffer_4;
    int32_t *      pring_buffer_5, *pring_buffer_6;
    int32_t *      pring_buffer_7, *pring_buffer_8;
    int32_t *      pring_buffer_9, *pring_buffer_10;
    const int32_t *pqmf_c_1, *pqmf_c_2, *pqmf_c_3, *pqmf_c_4;
    const int32_t *pqmf_c_5, *pqmf_c_6, *pqmf_c_7, *pqmf_c_8;
    const int32_t *pqmf_c_9, *pqmf_c_10;
#endif // #ifdef PREFER_POINTERS
#ifndef FIXED_POINT
    int32_t scale = 1.f / 64.f;
#endif
    int32_t n, k, out = 0;
    uint8_t l;

    /* qmf subsample l */
    for(l = 0; l < sbr->numTimeSlotsRate; l++) {
        /* shift buffer v */
        /* buffer is not shifted, we use double ringbuffer */
        // memmove(qmfs->v + 128, qmfs->v, (1280-128)*sizeof(int32_t));

        /* calculate 128 samples */
#ifndef FIXED_POINT
        pX = X[l];
        in_imag1[31] = scale * QMF_RE(pX[1]);
        in_real1[0] = scale * QMF_RE(pX[0]);
        in_imag2[31] = scale * QMF_IM(pX[63 - 1]);
        in_real2[0] = scale * QMF_IM(pX[63 - 0]);
        for(k = 1; k < 31; k++) {
            in_imag1[31 - k] = scale * QMF_RE(pX[2 * k + 1]);
            in_real1[k] = scale * QMF_RE(pX[2 * k]);
            in_imag2[31 - k] = scale * QMF_IM(pX[63 - (2 * k + 1)]);
            in_real2[k] = scale * QMF_IM(pX[63 - (2 * k)]);
        }
        in_imag1[0] = scale * QMF_RE(pX[63]);
        in_real1[31] = scale * QMF_RE(pX[62]);
        in_imag2[0] = scale * QMF_IM(pX[63 - 63]);
        in_real2[31] = scale * QMF_IM(pX[63 - 62]);
#else
        pX = X[l];
        in_imag1[31] = QMF_RE(pX[1]) >> 1;
        in_real1[0] = QMF_RE(pX[0]) >> 1;
        in_imag2[31] = QMF_IM(pX[62]) >> 1;
        in_real2[0] = QMF_IM(pX[63]) >> 1;
        for(k = 1; k < 31; k++) {
            in_imag1[31 - k] = QMF_RE(pX[2 * k + 1]) >> 1;
            in_real1[k] = QMF_RE(pX[2 * k]) >> 1;
            in_imag2[31 - k] = QMF_IM(pX[63 - (2 * k + 1)]) >> 1;
            in_real2[k] = QMF_IM(pX[63 - (2 * k)]) >> 1;
        }
        in_imag1[0] = QMF_RE(pX[63]) >> 1;
        in_real1[31] = QMF_RE(pX[62]) >> 1;
        in_imag2[0] = QMF_IM(pX[0]) >> 1;
        in_real2[31] = QMF_IM(pX[1]) >> 1;
#endif
        // dct4_kernel is DCT_IV without reordering which is done before and after FFT
        dct4_kernel(in_real1, in_imag1, out_real1, out_imag1);
        dct4_kernel(in_real2, in_imag2, out_real2, out_imag2);
        pring_buffer_1 = qmfs->v + qmfs->v_index;
        pring_buffer_3 = pring_buffer_1 + 1280;
#ifdef PREFER_POINTERS
        pring_buffer_2 = pring_buffer_1 + 127;
        pring_buffer_4 = pring_buffer_1 + (1280 + 127);
#endif // #ifdef PREFER_POINTERS
//        ptemp_1 = x1;
//        ptemp_2 = x2;
#ifdef PREFER_POINTERS
        for(n = 0; n < 32; n++) {
            // int32_t x1 = *ptemp_1++;
            // int32_t x2 = *ptemp_2++;
            //  pring_buffer_3 and pring_buffer_4 are needed only for double ring buffer
            *pring_buffer_1++ = *pring_buffer_3++ = out_real2[n] - out_real1[n];
            *pring_buffer_2-- = *pring_buffer_4-- = out_real2[n] + out_real1[n];
            // x1 = *ptemp_1++;
            // x2 = *ptemp_2++;
            *pring_buffer_1++ = *pring_buffer_3++ = out_imag2[31 - n] + out_imag1[31 - n];
            *pring_buffer_2-- = *pring_buffer_4-- = out_imag2[31 - n] - out_imag1[31 - n];
        }
#else // #ifdef PREFER_POINTERS
        for(n = 0; n < 32; n++) {
            // pring_buffer_3 and pring_buffer_4 are needed only for double ring buffer
            pring_buffer_1[2 * n] = pring_buffer_3[2 * n] = out_real2[n] - out_real1[n];
            pring_buffer_1[127 - 2 * n] = pring_buffer_3[127 - 2 * n] = out_real2[n] + out_real1[n];
            pring_buffer_1[2 * n + 1] = pring_buffer_3[2 * n + 1] = out_imag2[31 - n] + out_imag1[31 - n];
            pring_buffer_1[127 - (2 * n + 1)] = pring_buffer_3[127 - (2 * n + 1)] = out_imag2[31 - n] - out_imag1[31 - n];
        }
#endif // #ifdef PREFER_POINTERS
        pring_buffer_1 = qmfs->v + qmfs->v_index;
#ifdef PREFER_POINTERS
        pring_buffer_2 = pring_buffer_1 + 192;
        pring_buffer_3 = pring_buffer_1 + 256;
        pring_buffer_4 = pring_buffer_1 + (256 + 192);
        pring_buffer_5 = pring_buffer_1 + 512;
        pring_buffer_6 = pring_buffer_1 + (512 + 192);
        pring_buffer_7 = pring_buffer_1 + 768;
        pring_buffer_8 = pring_buffer_1 + (768 + 192);
        pring_buffer_9 = pring_buffer_1 + 1024;
        pring_buffer_10 = pring_buffer_1 + (1024 + 192);
        pqmf_c_1 = qmf_c;
        pqmf_c_2 = qmf_c + 64;
        pqmf_c_3 = qmf_c + 128;
        pqmf_c_4 = qmf_c + 192;
        pqmf_c_5 = qmf_c + 256;
        pqmf_c_6 = qmf_c + 320;
        pqmf_c_7 = qmf_c + 384;
        pqmf_c_8 = qmf_c + 448;
        pqmf_c_9 = qmf_c + 512;
        pqmf_c_10 = qmf_c + 576;
#endif // #ifdef PREFER_POINTERS
        /* calculate 64 output samples and window */
        for(k = 0; k < 64; k++) {
#ifdef PREFER_POINTERS
            output[out++] = MUL_F(*pring_buffer_1++, *pqmf_c_1++) + MUL_F(*pring_buffer_2++, *pqmf_c_2++) + MUL_F(*pring_buffer_3++, *pqmf_c_3++) + MUL_F(*pring_buffer_4++, *pqmf_c_4++) +
                            MUL_F(*pring_buffer_5++, *pqmf_c_5++) + MUL_F(*pring_buffer_6++, *pqmf_c_6++) + MUL_F(*pring_buffer_7++, *pqmf_c_7++) + MUL_F(*pring_buffer_8++, *pqmf_c_8++) +
                            MUL_F(*pring_buffer_9++, *pqmf_c_9++) + MUL_F(*pring_buffer_10++, *pqmf_c_10++);
#else  // #ifdef PREFER_POINTERS
            output[out++] = MUL_F(pring_buffer_1[k + 0], qmf_c[k + 0]) + MUL_F(pring_buffer_1[k + 192], qmf_c[k + 64]) + MUL_F(pring_buffer_1[k + 256], qmf_c[k + 128]) +
                            MUL_F(pring_buffer_1[k + (256 + 192)], qmf_c[k + 192]) + MUL_F(pring_buffer_1[k + 512], qmf_c[k + 256]) + MUL_F(pring_buffer_1[k + (512 + 192)], qmf_c[k + 320]) +
                            MUL_F(pring_buffer_1[k + 768], qmf_c[k + 384]) + MUL_F(pring_buffer_1[k + (768 + 192)], qmf_c[k + 448]) + MUL_F(pring_buffer_1[k + 1024], qmf_c[k + 512]) +
                            MUL_F(pring_buffer_1[k + (1024 + 192)], qmf_c[k + 576]);
#endif // #ifdef PREFER_POINTERS
        }
        /* update ringbuffer index */
        qmfs->v_index -= 128;
        if(qmfs->v_index < 0) qmfs->v_index = (1280 - 128);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void sbr_reset(sbr_info_t* sbr) {
    //    printf(ANSI_ESC_YELLOW "sbr_reset" ANSI_ESC_WHITE "\n");
    /* if these are different from the previous frame: Reset = 1 */
    if((sbr->bs_start_freq != sbr->bs_start_freq_prev) || (sbr->bs_stop_freq != sbr->bs_stop_freq_prev) || (sbr->bs_freq_scale != sbr->bs_freq_scale_prev) ||
       (sbr->bs_alter_scale != sbr->bs_alter_scale_prev) || (sbr->bs_xover_band != sbr->bs_xover_band_prev) || (sbr->bs_noise_bands != sbr->bs_noise_bands_prev)) {
        sbr->Reset = 1;
    }
    else { sbr->Reset = 0; }
    sbr->bs_start_freq_prev = sbr->bs_start_freq;
    sbr->bs_stop_freq_prev = sbr->bs_stop_freq;
    sbr->bs_freq_scale_prev = sbr->bs_freq_scale;
    sbr->bs_alter_scale_prev = sbr->bs_alter_scale;
    sbr->bs_xover_band_prev = sbr->bs_xover_band;
    sbr->bs_noise_bands_prev = sbr->bs_noise_bands;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t calc_sbr_tables(sbr_info_t* sbr, uint8_t start_freq, uint8_t stop_freq, uint8_t samplerate_mode, uint8_t freq_scale, uint8_t alter_scale, uint8_t xover_band) {
    uint8_t result = 0;
    uint8_t k2;

    /* calculate the Master Frequency Table */
    sbr->k0 = qmf_start_channel(start_freq, samplerate_mode, sbr->sample_rate);
    k2 = qmf_stop_channel(stop_freq, sbr->sample_rate, sbr->k0);
    /* check k0 and k2 */
    if(sbr->sample_rate >= 48000) {
        if((k2 - sbr->k0) > 32) result += 1;
    }
    else if(sbr->sample_rate <= 32000) {
        if((k2 - sbr->k0) > 48) result += 1;
    }
    else { /* (sbr->sample_rate == 44100) */
        if((k2 - sbr->k0) > 45) result += 1;
    }
    if(freq_scale == 0) { result += master_frequency_table_fs0(sbr, sbr->k0, k2, alter_scale); }
    else { result += master_frequency_table(sbr, sbr->k0, k2, freq_scale, alter_scale); }
    result += derived_frequency_table(sbr, xover_band, k2);
    result = (result > 0) ? 1 : 0;
    return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 2 */
UNUSED_FUNCTION static uint8_t sbr_extension_data(bitfile_t* ld, sbr_info_t* sbr, uint16_t cnt, uint8_t psResetFlag) {
    (void)psResetFlag;
    uint8_t  result = 0;
    uint16_t num_align_bits = 0;
    uint16_t num_sbr_bits1 = (uint16_t)faad_get_processed_bits(ld);
    uint16_t num_sbr_bits2;
    uint8_t  saved_start_freq, saved_samplerate_mode;
    uint8_t  saved_stop_freq, saved_freq_scale;
    uint8_t  saved_alter_scale, saved_xover_band;

#if(defined(PS_DEC))
    if(psResetFlag) sbr->psResetFlag = psResetFlag;
#endif
    {
        uint8_t bs_extension_type = (uint8_t)faad_getbits(ld, 4);
        if(bs_extension_type == EXT_SBR_DATA_CRC) { sbr->bs_sbr_crc_bits = (uint16_t)faad_getbits(ld, 10); }
    }
    /* save old header values, in case the new ones are corrupted */
    saved_start_freq = sbr->bs_start_freq;
    saved_samplerate_mode = sbr->bs_samplerate_mode;
    saved_stop_freq = sbr->bs_stop_freq;
    saved_freq_scale = sbr->bs_freq_scale;
    saved_alter_scale = sbr->bs_alter_scale;
    saved_xover_band = sbr->bs_xover_band;
    sbr->bs_header_flag = faad_get1bit(ld);
    if(sbr->bs_header_flag) sbr_header(ld, sbr);
    /* Reset? */
    sbr_reset(sbr);
    /* first frame should have a header */
    // if (!(sbr->frame == 0 && sbr->bs_header_flag == 0))
    if(sbr->header_count != 0) {
        if(sbr->Reset || (sbr->bs_header_flag && sbr->just_seeked)) {
            uint8_t rt = calc_sbr_tables(sbr, sbr->bs_start_freq, sbr->bs_stop_freq, sbr->bs_samplerate_mode, sbr->bs_freq_scale, sbr->bs_alter_scale, sbr->bs_xover_band);
            /* if an error occured with the new header values revert to the old ones */
            if(rt > 0) { result += calc_sbr_tables(sbr, saved_start_freq, saved_stop_freq, saved_samplerate_mode, saved_freq_scale, saved_alter_scale, saved_xover_band); }
        }
        if(result == 0) {
            result = sbr_data(ld, sbr);
            /* sbr_data() returning an error means that there was an error in envelope_time_border_vector().
               In this case the old time border vector is saved and all the previous data normally read after sbr_grid() is saved.  */
            /* to be on the safe side, calculate old sbr tables in case of error */
            if((result > 0) && (sbr->Reset || (sbr->bs_header_flag && sbr->just_seeked))) {
                result += calc_sbr_tables(sbr, saved_start_freq, saved_stop_freq, saved_samplerate_mode, saved_freq_scale, saved_alter_scale, saved_xover_band);
            }
            /* we should be able to safely set result to 0 now, but practise indicates this doesn't work well */
        }
    }
    else { result = 1; }
    num_sbr_bits2 = (uint16_t)faad_get_processed_bits(ld) - num_sbr_bits1;
    /* check if we read more bits then were available for sbr */
    if(8 * cnt < num_sbr_bits2) {
        faad_resetbits(ld, num_sbr_bits1 + 8 * cnt);
        num_sbr_bits2 = 8 * cnt;
#ifdef PS_DEC
        /* turn off PS for the unfortunate case that we randomly read some
         * PS data that looks correct */
        sbr->ps_used = 0;
#endif
        /* Make sure it doesn't decode SBR in this frame, or we'll get glitches */
        return 1;
    }
    {
        /* -4 does not apply, bs_extension_type is re-read in this function */
        num_align_bits = 8 * cnt /*- 4*/ - num_sbr_bits2;
        while(num_align_bits > 7) {
            faad_getbits(ld, 8);
            num_align_bits -= 8;
        }
        faad_getbits(ld, num_align_bits);
    }
    return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 3 */
static void sbr_header(bitfile_t* ld, sbr_info_t* sbr) {
    uint8_t bs_header_extra_1, bs_header_extra_2;

    sbr->header_count++;
    sbr->bs_amp_res = faad_get1bit(ld);
    /* bs_start_freq and bs_stop_freq must define a fequency band that does not exceed 48 channels */
    sbr->bs_start_freq = (uint8_t)faad_getbits(ld, 4);
    sbr->bs_stop_freq = (uint8_t)faad_getbits(ld, 4);
    sbr->bs_xover_band = (uint8_t)faad_getbits(ld, 3);
    faad_getbits(ld, 2);
    bs_header_extra_1 = (uint8_t)faad_get1bit(ld);
    bs_header_extra_2 = (uint8_t)faad_get1bit(ld);
    if(bs_header_extra_1) {
        sbr->bs_freq_scale = (uint8_t)faad_getbits(ld, 2);
        sbr->bs_alter_scale = (uint8_t)faad_get1bit(ld);
        sbr->bs_noise_bands = (uint8_t)faad_getbits(ld, 2);
    }
    else {
        /* Default values */
        sbr->bs_freq_scale = 2;
        sbr->bs_alter_scale = 1;
        sbr->bs_noise_bands = 2;
    }
    if(bs_header_extra_2) {
        sbr->bs_limiter_bands = (uint8_t)faad_getbits(ld, 2);
        sbr->bs_limiter_gains = (uint8_t)faad_getbits(ld, 2);
        sbr->bs_interpol_freq = (uint8_t)faad_get1bit(ld);
        sbr->bs_smoothing_mode = (uint8_t)faad_get1bit(ld);
    }
    else {
        /* Default values */
        sbr->bs_limiter_bands = 2;
        sbr->bs_limiter_gains = 2;
        sbr->bs_interpol_freq = 1;
        sbr->bs_smoothing_mode = 1;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 4 */
static uint8_t sbr_data(bitfile_t* ld, sbr_info_t* sbr) {
    uint8_t result;

    sbr->rate = (sbr->bs_samplerate_mode) ? 2 : 1;
    switch(sbr->id_aac) {
    case ID_SCE:
        if((result = sbr_single_channel_element(ld, sbr)) > 0) return result;
        break;
    case ID_CPE:
        if((result = sbr_channel_pair_element(ld, sbr)) > 0) return result;
        break;
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 5 */
static uint8_t sbr_single_channel_element(bitfile_t* ld, sbr_info_t* sbr) {
    uint8_t result;
    if(faad_get1bit(ld)) { faad_getbits(ld, 4); }
    if((result = sbr_grid(ld, sbr, 0)) > 0) return result;
    sbr_dtdf(ld, sbr, 0);
    invf_mode(ld, sbr, 0);
    sbr_envelope(ld, sbr, 0);
    sbr_noise(ld, sbr, 0);

#ifndef FIXED_POINT
    envelope_noise_dequantisation(sbr, 0);
#endif

    memset(sbr->bs_add_harmonic[0], 0, 64 * sizeof(uint8_t));
    sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld);
    if(sbr->bs_add_harmonic_flag[0]) sinusoidal_coding(ld, sbr, 0);
    sbr->bs_extended_data = faad_get1bit(ld);
    if(sbr->bs_extended_data) {
        uint16_t nr_bits_left;
#if(defined(PS_DEC) || defined(DRM_PS))
        uint8_t ps_ext_read = 0;
#endif
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4);
        if(cnt == 15) { cnt += (uint16_t)faad_getbits(ld, 8); }
        nr_bits_left = 8 * cnt;
        while(nr_bits_left > 7) {
            uint16_t tmp_nr_bits = 0;
            sbr->bs_extension_id = (uint8_t)faad_getbits(ld, 2);
            tmp_nr_bits += 2;

            /* allow only 1 PS extension element per extension data */
#if(defined(PS_DEC) || defined(DRM_PS))
    #if(defined(PS_DEC) && defined(DRM_PS))
            if(sbr->bs_extension_id == EXTENSION_ID_PS || sbr->bs_extension_id == DRM_PARAMETRIC_STEREO)
    #else
        #ifdef PS_DEC
            if(sbr->bs_extension_id == EXTENSION_ID_PS)
        #else
            #ifdef DRM_PS
            if(sbr->bs_extension_id == DRM_PARAMETRIC_STEREO)
            #endif
        #endif
    #endif
            {
                if(ps_ext_read == 0) { ps_ext_read = 1; }
                else {
                    /* to be safe make it 3, will switch to "default"
                     * in sbr_extension() */
    #ifdef DRM
                    return 1;
    #else
                    sbr->bs_extension_id = 3;
    #endif
                }
            }
#endif
            tmp_nr_bits += sbr_extension(ld, sbr, sbr->bs_extension_id, nr_bits_left);
            /* check if the data read is bigger than the number of available bits */
            if(tmp_nr_bits > nr_bits_left) return 1;
            nr_bits_left -= tmp_nr_bits;
        }
        /* Corrigendum */
        if(nr_bits_left > 0) { faad_getbits(ld, nr_bits_left); }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 6 */
static uint8_t sbr_channel_pair_element(bitfile_t* ld, sbr_info_t* sbr) {
    uint8_t n, result;
    if(faad_get1bit(ld)) {
        faad_getbits(ld, 4);
        faad_getbits(ld, 4);
    }
    sbr->bs_coupling = faad_get1bit(ld);

    if(sbr->bs_coupling) {
        if((result = sbr_grid(ld, sbr, 0)) > 0) return result;
        /* need to copy some data from left to right */
        sbr->bs_frame_class[1] = sbr->bs_frame_class[0];
        sbr->L_E[1] = sbr->L_E[0];
        sbr->L_Q[1] = sbr->L_Q[0];
        sbr->bs_pointer[1] = sbr->bs_pointer[0];
        for(n = 0; n <= sbr->L_E[0]; n++) {
            sbr->t_E[1][n] = sbr->t_E[0][n];
            sbr->f[1][n] = sbr->f[0][n];
        }
        for(n = 0; n <= sbr->L_Q[0]; n++) sbr->t_Q[1][n] = sbr->t_Q[0][n];
        sbr_dtdf(ld, sbr, 0);
        sbr_dtdf(ld, sbr, 1);
        invf_mode(ld, sbr, 0);
        /* more copying */
        for(n = 0; n < sbr->N_Q; n++) sbr->bs_invf_mode[1][n] = sbr->bs_invf_mode[0][n];
        sbr_envelope(ld, sbr, 0);
        sbr_noise(ld, sbr, 0);
        sbr_envelope(ld, sbr, 1);
        sbr_noise(ld, sbr, 1);
        memset(sbr->bs_add_harmonic[0], 0, 64 * sizeof(uint8_t));
        memset(sbr->bs_add_harmonic[1], 0, 64 * sizeof(uint8_t));

        sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld);
        if(sbr->bs_add_harmonic_flag[0]) sinusoidal_coding(ld, sbr, 0);

        sbr->bs_add_harmonic_flag[1] = faad_get1bit(ld);
        if(sbr->bs_add_harmonic_flag[1]) sinusoidal_coding(ld, sbr, 1);
    }
    else {
        uint8_t saved_t_E[6] = {0}, saved_t_Q[3] = {0};
        uint8_t saved_L_E = sbr->L_E[0];
        uint8_t saved_L_Q = sbr->L_Q[0];
        uint8_t saved_frame_class = sbr->bs_frame_class[0];

        for(n = 0; n < saved_L_E; n++) saved_t_E[n] = sbr->t_E[0][n];
        for(n = 0; n < saved_L_Q; n++) saved_t_Q[n] = sbr->t_Q[0][n];

        if((result = sbr_grid(ld, sbr, 0)) > 0) return result;
        if((result = sbr_grid(ld, sbr, 1)) > 0) {
            /* restore first channel data as well */
            sbr->bs_frame_class[0] = saved_frame_class;
            sbr->L_E[0] = saved_L_E;
            sbr->L_Q[0] = saved_L_Q;
            for(n = 0; n < 6; n++) sbr->t_E[0][n] = saved_t_E[n];
            for(n = 0; n < 3; n++) sbr->t_Q[0][n] = saved_t_Q[n];

            return result;
        }
        sbr_dtdf(ld, sbr, 0);
        sbr_dtdf(ld, sbr, 1);
        invf_mode(ld, sbr, 0);
        invf_mode(ld, sbr, 1);
        sbr_envelope(ld, sbr, 0);
        sbr_envelope(ld, sbr, 1);
        sbr_noise(ld, sbr, 0);
        sbr_noise(ld, sbr, 1);

        memset(sbr->bs_add_harmonic[0], 0, 64 * sizeof(uint8_t));
        memset(sbr->bs_add_harmonic[1], 0, 64 * sizeof(uint8_t));

        sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld);
        if(sbr->bs_add_harmonic_flag[0]) sinusoidal_coding(ld, sbr, 0);

        sbr->bs_add_harmonic_flag[1] = faad_get1bit(ld);
        if(sbr->bs_add_harmonic_flag[1]) sinusoidal_coding(ld, sbr, 1);
    }
#ifndef FIXED_POINT
    envelope_noise_dequantisation(sbr, 0);
    envelope_noise_dequantisation(sbr, 1);

    if(sbr->bs_coupling) unmap_envelope_noise(sbr);
#endif

    sbr->bs_extended_data = faad_get1bit(ld);
    if(sbr->bs_extended_data) {
        uint16_t nr_bits_left;
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4);
        if(cnt == 15) { cnt += (uint16_t)faad_getbits(ld, 8); }

        nr_bits_left = 8 * cnt;
        while(nr_bits_left > 7) {
            uint16_t tmp_nr_bits = 0;

            sbr->bs_extension_id = (uint8_t)faad_getbits(ld, 2);
            tmp_nr_bits += 2;
            tmp_nr_bits += sbr_extension(ld, sbr, sbr->bs_extension_id, nr_bits_left);

            /* check if the data read is bigger than the number of available bits */
            if(tmp_nr_bits > nr_bits_left) return 1;

            nr_bits_left -= tmp_nr_bits;
        }

        /* Corrigendum */
        if(nr_bits_left > 0) { faad_getbits(ld, nr_bits_left); }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* integer log[2](x): input range [0,10) */
static int8_t sbr_log2(const int8_t val) {
    int8_t log2tab[] = {0, 0, 1, 2, 2, 3, 3, 3, 3, 4};
    if(val < 10 && val >= 0) return log2tab[val];
    else
        return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 7 */
static uint8_t sbr_grid(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t i, env, rel, result;
    uint8_t bs_abs_bord, bs_abs_bord_1;
    uint8_t bs_num_env = 0;
    uint8_t saved_L_E = sbr->L_E[ch];
    uint8_t saved_L_Q = sbr->L_Q[ch];
    uint8_t saved_frame_class = sbr->bs_frame_class[ch];
    sbr->bs_frame_class[ch] = (uint8_t)faad_getbits(ld, 2);
    switch(sbr->bs_frame_class[ch]) {
    case FIXFIX:
        i = (uint8_t)faad_getbits(ld, 2);
        bs_num_env = min(1u << i, 5);
        i = (uint8_t)faad_get1bit(ld);
        for(env = 0; env < bs_num_env; env++) sbr->f[ch][env] = i;
        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = sbr->numTimeSlots;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;
    case FIXVAR:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2) + sbr->numTimeSlots;
        bs_num_env = (uint8_t)faad_getbits(ld, 2) + 1;
        for(rel = 0; rel < bs_num_env - 1; rel++) { sbr->bs_rel_bord[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2) + 2; }
        i = sbr_log2(bs_num_env + 1);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i);
        for(env = 0; env < bs_num_env; env++) { sbr->f[ch][bs_num_env - env - 1] = (uint8_t)faad_get1bit(ld); }
        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = bs_abs_bord;
        sbr->n_rel_lead[ch] = 0;
        sbr->n_rel_trail[ch] = bs_num_env - 1;
        break;
    case VARFIX:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2);
        bs_num_env = (uint8_t)faad_getbits(ld, 2) + 1;
        for(rel = 0; rel < bs_num_env - 1; rel++) { sbr->bs_rel_bord[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2) + 2; }
        i = sbr_log2(bs_num_env + 1);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i);
        for(env = 0; env < bs_num_env; env++) { sbr->f[ch][env] = (uint8_t)faad_get1bit(ld); }
        sbr->abs_bord_lead[ch] = bs_abs_bord;
        sbr->abs_bord_trail[ch] = sbr->numTimeSlots;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;
    case VARVAR:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2);
        bs_abs_bord_1 = (uint8_t)faad_getbits(ld, 2) + sbr->numTimeSlots;
        sbr->bs_num_rel_0[ch] = (uint8_t)faad_getbits(ld, 2);
        sbr->bs_num_rel_1[ch] = (uint8_t)faad_getbits(ld, 2);
        bs_num_env = min(5, sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 1);
        for(rel = 0; rel < sbr->bs_num_rel_0[ch]; rel++) { sbr->bs_rel_bord_0[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2) + 2; }
        for(rel = 0; rel < sbr->bs_num_rel_1[ch]; rel++) { sbr->bs_rel_bord_1[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2) + 2; }
        i = sbr_log2(sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 2);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i);
        for(env = 0; env < bs_num_env; env++) { sbr->f[ch][env] = (uint8_t)faad_get1bit(ld); }
        sbr->abs_bord_lead[ch] = bs_abs_bord;
        sbr->abs_bord_trail[ch] = bs_abs_bord_1;
        sbr->n_rel_lead[ch] = sbr->bs_num_rel_0[ch];
        sbr->n_rel_trail[ch] = sbr->bs_num_rel_1[ch];
        break;
    }
    if(sbr->bs_frame_class[ch] == VARVAR) sbr->L_E[ch] = min(bs_num_env, 5);
    else
        sbr->L_E[ch] = min(bs_num_env, 4);
    if(sbr->L_E[ch] <= 0) return 1;
    if(sbr->L_E[ch] > 1) sbr->L_Q[ch] = 2;
    else
        sbr->L_Q[ch] = 1;
    /* TODO: this code can probably be integrated into the code above! */
    if((result = envelope_time_border_vector(sbr, ch)) > 0) {
        sbr->bs_frame_class[ch] = saved_frame_class;
        sbr->L_E[ch] = saved_L_E;
        sbr->L_Q[ch] = saved_L_Q;
        return result;
    }
    noise_floor_time_border_vector(sbr, ch);
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 8 */
static void sbr_dtdf(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t i;

    for(i = 0; i < sbr->L_E[ch]; i++) { sbr->bs_df_env[ch][i] = faad_get1bit(ld); }
    for(i = 0; i < sbr->L_Q[ch]; i++) { sbr->bs_df_noise[ch][i] = faad_get1bit(ld); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 9 */
static void invf_mode(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t n;
    for(n = 0; n < sbr->N_Q; n++) { sbr->bs_invf_mode[ch][n] = (uint8_t)faad_getbits(ld, 2); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint16_t sbr_extension(bitfile_t* ld, sbr_info_t* sbr, uint8_t bs_extension_id, uint16_t num_bits_left) {
    (void)num_bits_left;
    uint8_t header;
    (void)header;
    uint16_t ret;
    (void)ret;

    switch(bs_extension_id) {
#ifdef PS_DEC
    case EXTENSION_ID_PS:
        if(!sbr->ps) { sbr->ps = ps_init(get_sr_index(sbr->sample_rate), sbr->numTimeSlotsRate); }
        if(sbr->psResetFlag) { sbr->ps->header_read = 0; }
        ret = ps_data(sbr->ps, ld, &header);
        /* enable PS if and only if: a header has been decoded */
        if(sbr->ps_used == 0 && header == 1) { sbr->ps_used = 1; }
        if(header == 1) { sbr->psResetFlag = 0; }
        return ret;
#endif
    default: sbr->bs_extension_data = (uint8_t)faad_getbits(ld, 6); return 6;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* table 12 */
static void sinusoidal_coding(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch) {
    uint8_t n;
    for(n = 0; n < sbr->N_high; n++) { sbr->bs_add_harmonic[ch][n] = faad_get1bit(ld); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* function constructs new time border vector first build into temp vector to be able to use previous vector on error */
uint8_t envelope_time_border_vector(sbr_info_t* sbr, uint8_t ch) {
    uint8_t l, border, temp;
    uint8_t t_E_temp[6] = {0};

    t_E_temp[0] = sbr->rate * sbr->abs_bord_lead[ch];
    t_E_temp[sbr->L_E[ch]] = sbr->rate * sbr->abs_bord_trail[ch];
    switch(sbr->bs_frame_class[ch]) {
    case FIXFIX:
        switch(sbr->L_E[ch]) {
        case 4:
            temp = (sbr->numTimeSlots / 4);
            t_E_temp[3] = sbr->rate * 3 * temp;
            t_E_temp[2] = sbr->rate * 2 * temp;
            t_E_temp[1] = sbr->rate * temp;
            break;
        case 2: t_E_temp[1] = sbr->rate * (sbr->numTimeSlots / 2); break;
        default: break;
        }
        break;
    case FIXVAR:
        if(sbr->L_E[ch] > 1) {
            int8_t i = sbr->L_E[ch];
            border = sbr->abs_bord_trail[ch];
            for(l = 0; l < (sbr->L_E[ch] - 1); l++) {
                if(border < sbr->bs_rel_bord[ch][l]) return 1;
                border -= sbr->bs_rel_bord[ch][l];
                t_E_temp[--i] = sbr->rate * border;
            }
        }
        break;
    case VARFIX:
        if(sbr->L_E[ch] > 1) {
            int8_t i = 1;
            border = sbr->abs_bord_lead[ch];
            for(l = 0; l < (sbr->L_E[ch] - 1); l++) {
                border += sbr->bs_rel_bord[ch][l];
                if(sbr->rate * border + sbr->tHFAdj > sbr->numTimeSlotsRate + sbr->tHFGen) return 1;
                t_E_temp[i++] = sbr->rate * border;
            }
        }
        break;
    case VARVAR:
        if(sbr->bs_num_rel_0[ch]) {
            int8_t i = 1;
            border = sbr->abs_bord_lead[ch];
            for(l = 0; l < sbr->bs_num_rel_0[ch]; l++) {
                border += sbr->bs_rel_bord_0[ch][l];
                if(sbr->rate * border + sbr->tHFAdj > sbr->numTimeSlotsRate + sbr->tHFGen) return 1;
                t_E_temp[i++] = sbr->rate * border;
            }
        }
        if(sbr->bs_num_rel_1[ch]) {
            int8_t i = sbr->L_E[ch];
            border = sbr->abs_bord_trail[ch];
            for(l = 0; l < sbr->bs_num_rel_1[ch]; l++) {
                if(border < sbr->bs_rel_bord_1[ch][l]) return 1;
                border -= sbr->bs_rel_bord_1[ch][l];
                t_E_temp[--i] = sbr->rate * border;
            }
        }
        break;
    }
    /* no error occured, we can safely use this t_E vector */
    for(l = 0; l < 6; l++) { sbr->t_E[ch][l] = t_E_temp[l]; }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void noise_floor_time_border_vector(sbr_info_t* sbr, uint8_t ch) {
    sbr->t_Q[ch][0] = sbr->t_E[ch][0];

    if(sbr->L_E[ch] == 1) {
        sbr->t_Q[ch][1] = sbr->t_E[ch][1];
        sbr->t_Q[ch][2] = 0;
    }
    else {
        uint8_t index = middleBorder(sbr, ch);
        sbr->t_Q[ch][1] = sbr->t_E[ch][index];
        sbr->t_Q[ch][2] = sbr->t_E[ch][sbr->L_E[ch]];
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t middleBorder(sbr_info_t* sbr, uint8_t ch) {
    int8_t retval = 0;

    switch(sbr->bs_frame_class[ch]) {
    case FIXFIX: retval = sbr->L_E[ch] / 2; break;
    case VARFIX:
        if(sbr->bs_pointer[ch] == 0) retval = 1;
        else if(sbr->bs_pointer[ch] == 1)
            retval = sbr->L_E[ch] - 1;
        else
            retval = sbr->bs_pointer[ch] - 1;
        break;
    case FIXVAR:
    case VARVAR:
        if(sbr->bs_pointer[ch] > 1) retval = sbr->L_E[ch] + 1 - sbr->bs_pointer[ch];
        else
            retval = sbr->L_E[ch] - 1;
        break;
    }
    return (retval > 0) ? retval : 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.1 */
int8_t GASpecificConfig(bitfile_t* ld, program_config_t* pce_out) {
    program_config_t pce;
    /* 1024 or 960 */
    m_mp4ASC->frameLengthFlag = faad_get1bit(ld);
#ifndef ALLOW_SMALL_FRAMELENGTH
    if(m_mp4ASC->frameLengthFlag == 1) return -3;
#endif
    m_mp4ASC->dependsOnCoreCoder = faad_get1bit(ld);
    if(m_mp4ASC->dependsOnCoreCoder == 1) { m_mp4ASC->coreCoderDelay = (uint16_t)faad_getbits(ld, 14); }
    m_mp4ASC->extensionFlag = faad_get1bit(ld);
    if(m_mp4ASC->channelsConfiguration == 0) {
        if(program_config_t_element(&pce, ld)) return -3;
        // m_mp4ASC->channelsConfiguration = pce.channels;
        if(pce_out != NULL) memcpy(pce_out, &pce, sizeof(program_config_t));
        /*
        if (pce.num_valid_cc_elements)
            return -3;
        */
    }
#ifdef ERROR_RESILIENCE
    if(m_mp4ASC->extensionFlag == 1) {
        /* Error resilience not supported yet */
        if(m_mp4ASC->objectTypeIndex >= ER_OBJECT_START) {
            m_mp4ASC->aacSectionDataResilienceFlag = faad_get1bit(ld);
            m_mp4ASC->aacScalefactorDataResilienceFlag = faad_get1bit(ld);
            m_mp4ASC->aacSpectralDataResilienceFlag = faad_get1bit(ld);
        }
        /* 1 bit: extensionFlag3 */
        faad_getbits(ld, 1);
    }
#endif
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.2 */
/* An MPEG-4 Audio decoder is only required to follow the Program Configuration Element in GASpecificConfig(). The decoder shall ignore any Program
   Configuration Elements that may occur in raw data blocks. PCEs transmitted in raw data blocks cannot be used to convey decoder configuration
   information. */
static uint8_t program_config_t_element(program_config_t* pce, bitfile_t* ld) {
    uint8_t i;

    memset(pce, 0, sizeof(program_config_t));
    pce->channels = 0;
    pce->element_instance_tag = (uint8_t)faad_getbits(ld, 4);
    pce->object_type = (uint8_t)faad_getbits(ld, 2);
    pce->sf_index = (uint8_t)faad_getbits(ld, 4);
    pce->num_front_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_side_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_back_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_lfe_channel_elements = (uint8_t)faad_getbits(ld, 2);
    pce->num_assoc_data_elements = (uint8_t)faad_getbits(ld, 3);
    pce->num_valid_cc_elements = (uint8_t)faad_getbits(ld, 4);
    pce->mono_mixdown_present = faad_get1bit(ld);
    if(pce->mono_mixdown_present == 1) { pce->mono_mixdown_element_number = (uint8_t)faad_getbits(ld, 4); }
    pce->stereo_mixdown_present = faad_get1bit(ld);
    if(pce->stereo_mixdown_present == 1) { pce->stereo_mixdown_element_number = (uint8_t)faad_getbits(ld, 4); }
    pce->matrix_mixdown_idx_present = faad_get1bit(ld);
    if(pce->matrix_mixdown_idx_present == 1) {
        pce->matrix_mixdown_idx = (uint8_t)faad_getbits(ld, 2);
        pce->pseudo_surround_enable = faad_get1bit(ld);
    }
    for(i = 0; i < pce->num_front_channel_elements; i++) {
        pce->front_element_is_cpe[i] = faad_get1bit(ld);
        pce->front_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if(pce->front_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels += 2;
            pce->channels += 2;
        }
        else {
            pce->sce_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels++;
            pce->channels++;
        }
    }
    for(i = 0; i < pce->num_side_channel_elements; i++) {
        pce->side_element_is_cpe[i] = faad_get1bit(ld);
        pce->side_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if(pce->side_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels += 2;
            pce->channels += 2;
        }
        else {
            pce->sce_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels++;
            pce->channels++;
        }
    }
    for(i = 0; i < pce->num_back_channel_elements; i++) {
        pce->back_element_is_cpe[i] = faad_get1bit(ld);
        pce->back_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if(pce->back_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->channels += 2;
            pce->num_back_channels += 2;
        }
        else {
            pce->sce_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->num_back_channels++;
            pce->channels++;
        }
    }
    for(i = 0; i < pce->num_lfe_channel_elements; i++) {
        pce->lfe_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        pce->sce_channel[pce->lfe_element_tag_select[i]] = pce->channels;
        pce->num_lfe_channels++;
        pce->channels++;
    }
    for(i = 0; i < pce->num_assoc_data_elements; i++) pce->assoc_data_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
    for(i = 0; i < pce->num_valid_cc_elements; i++) {
        pce->cc_element_is_ind_sw[i] = faad_get1bit(ld);
        pce->valid_cc_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
    }
    faad_byte_align(ld);
    pce->comment_field_bytes = (uint8_t)faad_getbits(ld, 8);
    for(i = 0; i < pce->comment_field_bytes; i++) { pce->comment_field_data[i] = (uint8_t)faad_getbits(ld, 8); }
    pce->comment_field_data[i] = 0;
    if(pce->channels > MAX_CHANNELS) return 22;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void decode_sce_lfe(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, uint8_t id_syn_ele) {

    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;

    /* One or two channels are used; exact number will be known after single_lfe_channel_element */
    if (channels + 2 > MAX_CHANNELS){
        hInfo->error = 12;
        return;
    }
    if (hDecoder->fr_ch_ele + 1 > MAX_SYNTAX_ELEMENTS) {
        hInfo->error = 13;
        return;
    }

    /* for SCE hDecoder->element_output_channels[] is not set here because this can become 2 when some form of Parametric Stereo coding is used */

    if (hDecoder->element_id[hDecoder->fr_ch_ele] != INVALID_ELEMENT_ID && hDecoder->element_id[hDecoder->fr_ch_ele] != id_syn_ele) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }

    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;

    /* decode the element */
    hInfo->error = single_lfe_channel_element(hDecoder, ld, channels, &tag);

    /* map output channels position to internal data channels */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2) {
        /* this might be faulty when pce_set is true */
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels+1] = channels+1;
    } else {
        if (hDecoder->pce_set) {
            if (hDecoder->pce.channels > MAX_CHANNELS) {
                hInfo->error = 22;
                return;
            }
            hDecoder->internal_channel[hDecoder->pce.sce_channel[tag]] = channels;
        }
        else {
            hDecoder->internal_channel[channels] = channels;
        }
    }

    hDecoder->fr_channels += hDecoder->element_output_channels[hDecoder->fr_ch_ele];
    hDecoder->fr_ch_ele++;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void decode_cpe(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, uint8_t id_syn_ele) {
    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;
    if(channels + 2 > MAX_CHANNELS) {
        hInfo->error = 12;
        return;
    }
    if(hDecoder->fr_ch_ele + 1 > MAX_SYNTAX_ELEMENTS) {
        hInfo->error = 13;
        return;
    }
    if (hDecoder->pce_set && (hDecoder->pce.channels > MAX_CHANNELS)){
        hInfo->error = 22;
        return;
    }
    /* for CPE the number of output channels is always 2 */
    if(hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0) {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = 2;
    }
    else if(hDecoder->element_output_channels[hDecoder->fr_ch_ele] != 2) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }
    if(hDecoder->element_id[hDecoder->fr_ch_ele] != INVALID_ELEMENT_ID && hDecoder->element_id[hDecoder->fr_ch_ele] != id_syn_ele) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }
    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;
    /* decode the element */
    hInfo->error = channel_pair_element(hDecoder, ld, channels, &tag);
    /* map output channel position to internal data channels */
    if(hDecoder->pce_set) {
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag]] = channels;
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag] + 1] = channels + 1;
    }
    else {
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels + 1] = channels + 1;
    }
    hDecoder->fr_channels += 2;
    hDecoder->fr_ch_ele++;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void raw_data_block(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, program_config_t* pce, drc_info_t* drc) {
    uint8_t id_syn_ele;
    uint8_t ele_this_frame = 0;

    hDecoder->fr_channels = 0;
    hDecoder->fr_ch_ele = 0;
    hDecoder->first_syn_ele = 25;
    hDecoder->has_lfe = 0;
#ifdef ERROR_RESILIENCE
    if(hDecoder->object_type < ER_OBJECT_START) {
#endif
        /* Table 4.4.3: raw_data_block() */
        while((id_syn_ele = (uint8_t)faad_getbits(ld, LEN_SE_ID)) != ID_END) {
            switch(id_syn_ele) {
            case ID_SCE:
                ele_this_frame++;
                if(hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);
                if(hInfo->error > 0) return;
                break;
            case ID_CPE:
                ele_this_frame++;
                if(hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                decode_cpe(hDecoder, hInfo, ld, id_syn_ele);
                if(hInfo->error > 0) return;
                break;
            case ID_LFE:
                ele_this_frame++;
                hDecoder->has_lfe++;
                decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);

                if(hInfo->error > 0) return;
                break;
            case ID_CCE: /* not implemented yet, but skip the bits */
                ele_this_frame++;
                hInfo->error = 6;

                if(hInfo->error > 0) return;
                break;
            case ID_DSE:
                ele_this_frame++;
                data_stream_element(hDecoder, ld);
                break;
            case ID_PCE:
                if(ele_this_frame != 0) {
                    hInfo->error = 31;
                    return;
                }
                ele_this_frame++;
                /* 14496-4: 5.6.4.1.2.1.3: */
                /* program_config_turation_element()'s in access units shall be ignored */
                program_config_t_element(pce, ld);
                // if ((hInfo->error = program_config_t_element(pce, ld)) > 0)
                //     return;
                // hDecoder->pce_set = 1;
                break;
            case ID_FIL:
                ele_this_frame++;
                /* one sbr_info_t*_t describes a channel_element not a channel! */
                /* if we encounter SBR data here: error */
                /* SBR data will be read directly in the SCE/LFE/CPE element */
                if((hInfo->error = fill_element(hDecoder, ld, drc, INVALID_SBR_ELEMENT)) > 0) return;
                break;
            }
        }
#ifdef ERROR_RESILIENCE
    }
    else {
        /* Table 262: er_raw_data_block() */
        switch(hDecoder->channelConfiguration) {
        case 1:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            break;
        case 2:
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            break;
        case 3:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            break;
        case 4:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            break;
        case 5:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            break;
        case 6:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
            if(hInfo->error > 0) return;
            break;
        case 7: /* 8 channels */
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if(hInfo->error > 0) return;
            decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
            if(hInfo->error > 0) return;
            break;
        default: hInfo->error = 7; return;
        }
    }
#endif
    /* new in corrigendum 14496-3:2002 */
    { faad_byte_align(ld); }
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.4 and */
/* Table 4.4.9 */
static uint8_t single_lfe_channel_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, uint8_t channel, uint8_t* tag) {
    uint8_t retval = 0;
    uint8_t ret = 0;
    memset(m_sce, 0, 1 * sizeof(element_t));
    memset(m_spec_data, 0, 1024 * sizeof(int16_t));

    ic_stream_t* ics = &(m_sce->ics1);
    m_sce->element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG);
    *tag = m_sce->element_instance_tag;
    m_sce->channel = channel;
    m_sce->paired_channel = -1;
    retval = individual_channel_stream(hDecoder, m_sce, ld, ics, 0, m_spec_data);
    if(retval > 0) {
        ret = retval;
        goto exit;
    }
    /* IS not allowed in single channel */
    if(ics->is_used) {
        ret = 32;
        goto exit;
    }
#ifdef SBR_DEC
    /* check if next bitstream element is a fill element */
    /* if so, read it now so SBR decoding can be done in case of a file with SBR */
    if(faad_showbits(ld, LEN_SE_ID) == ID_FIL) {
        faad_flushbits(ld, LEN_SE_ID);
        /* one sbr_info_t*_t describes a channel_element not a channel! */
        if((retval = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0) {
            ret = retval;
            goto exit;
        }
    }
#endif
    /* noiseless coding is done, spectral reconstruction is done now */
    retval = reconstruct_single_channel(hDecoder, ics, m_sce, m_spec_data);
    if(retval > 0) {
        ret = retval;
        goto exit;
    }
    ret = 0;
    goto exit;

exit:
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.5 */
static uint8_t channel_pair_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, uint8_t channels, uint8_t* tag) {
    memset(m_cpe, 0, 1 * sizeof(element_t));
    memset(m_spec_data1, 0, 1024 * sizeof(int16_t));
    memset(m_spec_data2, 0, 1024 * sizeof(int16_t));

    ic_stream_t* ics1 = &(m_cpe->ics1);
    ic_stream_t* ics2 = &(m_cpe->ics2);
    uint8_t      result;
    uint8_t      ret;

    m_cpe->channel = channels;
    m_cpe->paired_channel = channels + 1;
    m_cpe->element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG);
    *tag = m_cpe->element_instance_tag;
    if((m_cpe->common_window = faad_get1bit(ld)) & 1) {
        /* both channels have common ics information */
        if((result = ics_info(hDecoder, ics1, ld, m_cpe->common_window)) > 0) {
            ret = result;
            goto exit;
        }
        ics1->ms_mask_present = (uint8_t)faad_getbits(ld, 2);
        if(ics1->ms_mask_present == 3) {
            /* bitstream error */
            ret = 32;
            goto exit;
        }
        if(ics1->ms_mask_present == 1) {
            uint8_t g, sfb;
            for(g = 0; g < ics1->num_window_groups; g++) {
                for(sfb = 0; sfb < ics1->max_sfb; sfb++) { ics1->ms_used[g][sfb] = faad_get1bit(ld); }
            }
        }
#ifdef ERROR_RESILIENCE
        if((hDecoder->object_type >= ER_OBJECT_START) && (ics1->predictor_data_present)) {
            if((
    #ifdef LTP_DEC
                   ics1->ltp.data_present =
    #endif
                       faad_get1bit(ld)) &
               1) {
    #ifdef LTP_DEC
                if((result = ltp_data(hDecoder, ics1, &(ics1->ltp), ld)) > 0) {
                    ret = result;
                    goto exit;
                }
    #else
                ret = 26;
                goto exit;
    #endif
            }
        }
#endif
        memcpy(ics2, ics1, sizeof(ic_stream_t));
    }
    else { ics1->ms_mask_present = 0; }
    if((result = individual_channel_stream(hDecoder, m_cpe, ld, ics1, 0, m_spec_data1)) > 0) {
        ret = result;
        goto exit;
    }
#ifdef ERROR_RESILIENCE
    if(m_cpe->common_window && (hDecoder->object_type >= ER_OBJECT_START) && (ics1->predictor_data_present)) {
        if((
    #ifdef LTP_DEC
               ics1->ltp2.data_present =
    #endif
                   faad_get1bit(ld)) &
           1) {
    #ifdef LTP_DEC
            if((result = ltp_data(hDecoder, ics1, &(ics1->ltp2), ld)) > 0) {
                ret = result;
                goto exit;
            }
    #else
            ret = 26;
            goto exit;
    #endif
        }
    }
#endif
    if((result = individual_channel_stream(hDecoder, m_cpe, ld, ics2, 0, m_spec_data2)) > 0) {
        ret = result;
        goto exit;
    }
#ifdef SBR_DEC
    /* check if next bitstream element is a fill element if so, read it now so SBR decoding can be done in case of a file with SBR */
    if(faad_showbits(ld, LEN_SE_ID) == ID_FIL) {
        faad_flushbits(ld, LEN_SE_ID);
        /* one sbr_info_t*_t describes a channel_element not a channel! */
        if((result = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0) {
            ret = result;
            goto exit;
        }
    }
#endif
    /* noiseless coding is done, spectral reconstruction is done now */
    if((result = reconstruct_channel_pair(hDecoder, ics1, ics2, m_cpe, m_spec_data1, m_spec_data2)) > 0) {
        ret = result;
        goto exit;
    }
    ret = 0;
    goto exit;

exit:
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.6 */
static uint8_t ics_info(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, uint8_t common_window) {
    uint8_t retval = 0;
    uint8_t ics_reserved_bit;

    ics_reserved_bit = faad_get1bit(ld);
    if (ics_reserved_bit != 0) return 32;
    ics->window_sequence = (uint8_t)faad_getbits(ld, 2);
    ics->window_shape = faad_get1bit(ld);

#ifdef LD_DEC
    /* No block switching in LD */
    if ((hDecoder->object_type == LD) && (ics->window_sequence != ONLY_LONG_SEQUENCE))
        return 32;
#endif

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 4);
        ics->scale_factor_grouping = (uint8_t)faad_getbits(ld, 7);
    }
    else {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 6);
    }

    /* get the grouping information */
    if ((retval = window_grouping_info(hDecoder, ics)) > 0)
        return retval;


    /* should be an error */
    /* check the range of max_sfb */
    if (ics->max_sfb > ics->num_swb)
        return 16;

    if (ics->window_sequence != EIGHT_SHORT_SEQUENCE){
        if((ics->predictor_data_present = faad_get1bit(ld)) & 1) {
            if (hDecoder->object_type == MAIN) {/* MPEG2 style AAC predictor */
                uint8_t sfb;
                uint8_t predictor_reset, predictor_reset_group_number, prediction_used;
                uint8_t limit = min(ics->max_sfb, max_pred_sfb(hDecoder->sf_index));

                predictor_reset = faad_get1bit(ld);
                if (predictor_reset) {
                    predictor_reset_group_number =
                        (uint8_t)faad_getbits(ld, 5);
                }
                else {
                    predictor_reset_group_number = 0;
                }

                for (sfb = 0; sfb < limit; sfb++){
                    prediction_used = faad_get1bit(ld);
#ifdef MAIN_DEC
                    ics->pred.prediction_used[sfb] = prediction_used;
#endif
                }
#ifdef MAIN_DEC
                ics->pred.limit = limit;
                ics->pred.predictor_reset = predictor_reset;
                ics->pred.predictor_reset_group_number = predictor_reset_group_number;
#else
                (void)predictor_reset_group_number;
                (void)prediction_used;
#endif
            }
#ifdef LTP_DEC
            else { /* Long Term Prediction */
                if (hDecoder->object_type < ER_OBJECT_START)
                {
                    if ((ics->ltp.data_present = faad_get1bit(ld)) & 1) {
                        if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0) {
                            return retval;
                        }
                    }
                    if (common_window) {
                        if ((ics->ltp2.data_present = faad_get1bit(ld)) & 1) {
                            if ((retval = ltp_data(hDecoder, ics, &(ics->ltp2), ld)) > 0) {
                                return retval;
                            }
                        }
                    }
                }
#ifdef ERROR_RESILIENCE
                if (!common_window && (hDecoder->object_type >= ER_OBJECT_START))
                {
                    if ((ics->ltp.data_present = faad_get1bit(ld)) & 1) {
                        if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0) {
                            return retval;
                        }
                    }
                }
#endif  /* ERROR_RESILIENCE */
            }
#else  /* LTP_DEC */
            (void)common_window;
#endif  /* LTP_DEC */
        }
    }

    return retval;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.7 */
static uint8_t pulse_data(ic_stream_t* ics, pulse_info_t* pul, bitfile_t* ld) {
    uint8_t i;

    pul->number_pulse = (uint8_t)faad_getbits(ld, 2);
    pul->pulse_start_sfb = (uint8_t)faad_getbits(ld, 6);
    /* check the range of pulse_start_sfb */
    if(pul->pulse_start_sfb > ics->num_swb) return 16;
    for(i = 0; i < pul->number_pulse + 1; i++) {
        pul->pulse_offset[i] = (uint8_t)faad_getbits(ld, 5);
        pul->pulse_amp[i] = (uint8_t)faad_getbits(ld, 4);
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.10 */
static uint16_t data_stream_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld) {
    (void)ld;
    (void)hDecoder;
    uint8_t  byte_aligned;
    uint16_t i, count;

    /* element_instance_tag = */ faad_getbits(ld, LEN_TAG);
    byte_aligned = faad_get1bit(ld);
    count = (uint16_t)faad_getbits(ld, 8);
    if(count == 255) { count += (uint16_t)faad_getbits(ld, 8); }
    if(byte_aligned) faad_byte_align(ld);
    for(i = 0; i < count; i++) { faad_getbits(ld, LEN_BYTE); }
    return count;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.11 */
static uint8_t fill_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, drc_info_t* drc, uint8_t sbr_ele) {
    (void)hDecoder;
    (void)sbr_ele;
    uint16_t count;
#ifdef SBR_DEC
    uint8_t bs_extension_type;
#endif
    count = (uint16_t)faad_getbits(ld, 4);
    if(count == 15) { count += (uint16_t)faad_getbits(ld, 8) - 1; }
    if(count > 0) {
#ifdef SBR_DEC
        bs_extension_type = (uint8_t)faad_showbits(ld, 4);
        if((bs_extension_type == EXT_SBR_DATA) || (bs_extension_type == EXT_SBR_DATA_CRC)) {
            if(sbr_ele == INVALID_SBR_ELEMENT) return 24;
            if(!hDecoder->sbr[sbr_ele]) {
                hDecoder->sbr[sbr_ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[sbr_ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR);
            }
            if(!hDecoder->sbr[sbr_ele]) return 19;
            hDecoder->sbr_present_flag = 1;
            /* parse the SBR data */
            hDecoder->sbr[sbr_ele]->ret = sbr_extension_data(ld, hDecoder->sbr[sbr_ele], count, hDecoder->postSeekResetFlag);
    #if(defined(PS_DEC))
            if(hDecoder->sbr[sbr_ele]->ps_used) {
                hDecoder->ps_used[sbr_ele] = 1;
                /* set element independent flag to 1 as well */
                hDecoder->ps_used_global = 1;
            }
    #endif
        }
        else {
#endif
            while(count > 0) { count -= extension_payload(ld, drc, count); }
#ifdef SBR_DEC
        }
#endif
    }

    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t side_info(NeAACDecStruct_t* hDecoder, element_t* ele, bitfile_t* ld, ic_stream_t* ics, uint8_t scal_flag) {
    uint8_t result;

    ics->global_gain = (uint8_t)faad_getbits(ld, 8);
    if(!ele->common_window && !scal_flag) {
        if((result = ics_info(hDecoder, ics, ld, ele->common_window)) > 0) return result;
    }
    if((result = section_data(hDecoder, ics, ld)) > 0) return result;
    if((result = scale_factor_data(hDecoder, ics, ld)) > 0) return result;
    if(!scal_flag) {
        /**
         **  NOTE: It could be that pulse data is available in scalable AAC too,
         **        as said in Amendment 1, this could be only the case for ER AAC,
         **        though. (have to check this out later)
         **/
        /* get pulse data */
        if((ics->pulse_data_present = faad_get1bit(ld)) & 1) {
            if((result = pulse_data(ics, &(ics->pul), ld)) > 0) return result;
        }
        /* get tns data */
        if((ics->tns_data_present = faad_get1bit(ld)) & 1) {
#ifdef ERROR_RESILIENCE
            if(hDecoder->object_type < ER_OBJECT_START)
#endif
                tns_data(ics, &(ics->tns), ld);
        }
        /* get gain control data */
        if((ics->gain_control_data_present = faad_get1bit(ld)) & 1) { return 1; }
    }
#ifdef ERROR_RESILIENCE
    if(hDecoder->aacSpectralDataResilienceFlag) {
        ics->length_of_reordered_spectral_data = (uint16_t)faad_getbits(ld, 14);
        if(hDecoder->channelConfiguration == 2) {
            if(ics->length_of_reordered_spectral_data > 6144) ics->length_of_reordered_spectral_data = 6144;
        }
        else {
            if(ics->length_of_reordered_spectral_data > 12288) ics->length_of_reordered_spectral_data = 12288;
        }
        ics->length_of_longest_codeword = (uint8_t)faad_getbits(ld, 6);
        if(ics->length_of_longest_codeword >= 49) ics->length_of_longest_codeword = 49;
    }
    /* RVLC spectral data is put here */
    if(hDecoder->aacScalefactorDataResilienceFlag) {
        if((result = rvlc_decode_scale_factors(ics, ld)) > 0) return result;
    }
#endif
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.24 */
static uint8_t individual_channel_stream(NeAACDecStruct_t* hDecoder, element_t* ele, bitfile_t* ld, ic_stream_t* ics, uint8_t scal_flag, int16_t* spec_data) {
    uint8_t result;

    result = side_info(hDecoder, ele, ld, ics, scal_flag);
    if(result > 0) return result;
    if(hDecoder->object_type >= ER_OBJECT_START) {
        if(ics->tns_data_present) tns_data(ics, &(ics->tns), ld);
    }
#ifdef ERROR_RESILIENCE
    if(hDecoder->aacSpectralDataResilienceFlag) {
        /* error resilient spectral data decoding */
        if((result = reordered_spectral_data(hDecoder, ics, ld, spec_data)) > 0) { return result; }
    }
    else {
#endif
        /* decode the spectral data */
        if((result = spectral_data(hDecoder, ics, ld, spec_data)) > 0) { return result; }
#ifdef ERROR_RESILIENCE
    }
#endif
    /* pulse coding reconstruction */
    if(ics->pulse_data_present) {
        if(ics->window_sequence != EIGHT_SHORT_SEQUENCE) {
            if((result = pulse_decode(ics, spec_data, hDecoder->frameLength)) > 0) return result;
        }
        else { return 2; /* pulse coding not allowed for short blocks */ }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.25 */
static uint8_t section_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld) {
    (void)hDecoder;
    uint8_t g;
    uint8_t sect_esc_val, sect_bits;

    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) sect_bits = 3;
    else
        sect_bits = 5;
    sect_esc_val = (1u << sect_bits) - 1;
    for(g = 0; g < ics->num_window_groups; g++) {
        uint8_t k = 0;
        uint8_t i = 0;
        while(k < ics->max_sfb) {
#ifdef ERROR_RESILIENCE
            uint8_t vcb11 = 0;
#endif
            uint8_t  sfb;
            uint8_t  sect_len_incr;
            uint16_t sect_len = 0;
            uint8_t  sect_cb_bits = 4;
            /* if "faad_getbits" detects error and returns "0", "k" is never incremented and we cannot leave the while loop */
            if(ld->error != 0) return 14;
#ifdef ERROR_RESILIENCE
            if(hDecoder->aacSectionDataResilienceFlag) sect_cb_bits = 5;
#endif
            ics->sect_cb[g][i] = (uint8_t)faad_getbits(ld, sect_cb_bits);
            if(ics->sect_cb[g][i] == 12) return 32;
            if(ics->sect_cb[g][i] == NOISE_HCB) ics->noise_used = 1;
            if(ics->sect_cb[g][i] == INTENSITY_HCB2 || ics->sect_cb[g][i] == INTENSITY_HCB) ics->is_used = 1;
#ifdef ERROR_RESILIENCE
            if(hDecoder->aacSectionDataResilienceFlag) {
                if((ics->sect_cb[g][i] == 11) || ((ics->sect_cb[g][i] >= 16) && (ics->sect_cb[g][i] <= 32))) { vcb11 = 1; }
            }
            if(vcb11) { sect_len_incr = 1; }
            else {
#endif
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits);
#ifdef ERROR_RESILIENCE
            }
#endif
            while ((sect_len_incr == sect_esc_val) /* &&
                (k+sect_len < ics->max_sfb)*/)
            {
                sect_len += sect_len_incr;
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits);
            }
            sect_len += sect_len_incr;
            ics->sect_start[g][i] = k;
            ics->sect_end[g][i] = k + sect_len;
            if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
                if(k + sect_len > 8 * 15) return 15;
                if(i >= 8 * 15) return 15;
            }
            else {
                if(k + sect_len > MAX_SFB) return 15;
                if(i >= MAX_SFB) return 15;
            }
            for(sfb = k; sfb < k + sect_len; sfb++) { ics->sfb_cb[g][sfb] = ics->sect_cb[g][i]; }
            k += sect_len;
            i++;
        }
        ics->num_sec[g] = i;
        /* the sum of all sect_len_incr elements for a given window group shall equal max_sfb */
        if(k != ics->max_sfb) { return 32; }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * decode_scale_factors() decodes the scalefactors from the bitstream. All scalefactors (and also the stereo positions and pns energies) are
 transmitted using Huffman coded DPCM relative to the previous active scalefactor (respectively previous stereo position or previous pns energy, see
 subclause 4.6.2 and 4.6.3). The first active scalefactor is differentially coded relative to the global gain. */
static uint8_t decode_scale_factors(ic_stream_t* ics, bitfile_t* ld) {
    uint8_t g, sfb;
    int16_t t;

    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t scale_factor_max = 255;
#ifdef FIXED_POINT
    /* TODO: consider rolling out to regular build. */
    #ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    /* The value is inexact, adjusted to current fuzzer findings. */
    scale_factor_max = 165;
    #endif // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#endif     // FIXED_POINT
#ifndef DRM
    int8_t  noise_pcm_flag = 1;
    int16_t noise_energy = ics->global_gain - 90;
#endif

    for(g = 0; g < ics->num_window_groups; g++) {
        for(sfb = 0; sfb < ics->max_sfb; sfb++) {
            switch(ics->sfb_cb[g][sfb]) {
                case ZERO_HCB: /* zero book */
                    ics->scale_factors[g][sfb] = 0;
// #define SF_PRINT
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif
                    break;
                case INTENSITY_HCB: /* intensity books */
                case INTENSITY_HCB2:

                    /* decode intensity position */
                    t = huffman_scale_factor(ld);
                    is_position += (t - 60);
                    ics->scale_factors[g][sfb] = is_position;
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif

                    break;
                case NOISE_HCB: /* noise books */

#ifndef DRM
                    /* decode noise energy */
                    if(noise_pcm_flag) {
                        noise_pcm_flag = 0;
                        t = (int16_t)faad_getbits(ld, 9) - 256;
                    }
                    else {
                        t = huffman_scale_factor(ld);
                        t -= 60;
                    }
                    noise_energy += t;
                    ics->scale_factors[g][sfb] = noise_energy;
    #ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
    #endif
#else
                    /* PNS not allowed in DRM */
                    return 29;
#endif

                    break;
                default: /* spectral books */

                    /* ics->scale_factors[g][sfb] must be between 0 and 255 */

                    ics->scale_factors[g][sfb] = 0;

                    /* decode scale factor */
                    t = huffman_scale_factor(ld);
                    scale_factor += (t - 60);
                    if(scale_factor < 0 || scale_factor > 255) return 4;
                    ics->scale_factors[g][sfb] = min(scale_factor, scale_factor_max);
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif

                    break;
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.26 */
static uint8_t scale_factor_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld) {
    (void)hDecoder;
    uint8_t ret = 0;

#ifdef ERROR_RESILIENCE
    if(!hDecoder->aacScalefactorDataResilienceFlag) {
#endif
        ret = decode_scale_factors(ics, ld);
#ifdef ERROR_RESILIENCE
    }
    else {
        /* In ER AAC the parameters for RVLC are seperated from the actual
           data that holds the scale_factors.
           Strangely enough, 2 parameters for HCR are put inbetween them.
        */
        ret = rvlc_scale_factor_data(ics, ld);
    }
#endif
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.27 */
static void tns_data(ic_stream_t* ics, tns_info_t* tns, bitfile_t* ld) {
    uint8_t w, filt, i, start_coef_bits, coef_bits;
    uint8_t n_filt_bits = 2;
    uint8_t length_bits = 6;
    uint8_t order_bits = 5;

    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        n_filt_bits = 1;
        length_bits = 4;
        order_bits = 3;
    }
    for(w = 0; w < ics->num_windows; w++) {
        tns->n_filt[w] = (uint8_t)faad_getbits(ld, n_filt_bits);
        if(tns->n_filt[w]) {
            if((tns->coef_res[w] = faad_get1bit(ld)) & 1) { start_coef_bits = 4; }
            else { start_coef_bits = 3; }
        }
        for(filt = 0; filt < tns->n_filt[w]; filt++) {
            tns->length[w][filt] = (uint8_t)faad_getbits(ld, length_bits);
            tns->order[w][filt] = (uint8_t)faad_getbits(ld, order_bits);
            if(tns->order[w][filt]) {
                tns->direction[w][filt] = faad_get1bit(ld);
                tns->coef_compress[w][filt] = faad_get1bit(ld);
                coef_bits = start_coef_bits - tns->coef_compress[w][filt];
                for(i = 0; i < tns->order[w][filt]; i++) { tns->coef[w][filt][i] = (uint8_t)faad_getbits(ld, coef_bits); }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.28 */
static uint8_t ltp_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, ltp_info_t* ltp, bitfile_t* ld) {
    (void)ltp_data;
    uint8_t sfb, w;

    ltp->lag = 0;
#ifdef LD_DEC
    if(hDecoder->object_type == LD) {
        ltp->lag_update = (uint8_t)faad_getbits(ld, 1);
        if(ltp->lag_update) { ltp->lag = (uint16_t)faad_getbits(ld, 10); }
    }
    else {
#endif
        ltp->lag = (uint16_t)faad_getbits(ld, 11);
#ifdef LD_DEC
    }
#endif
    /* Check length of lag */
    if(ltp->lag > (hDecoder->frameLength << 1)) return 18;
    ltp->coef = (uint8_t)faad_getbits(ld, 3);
    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        for(w = 0; w < ics->num_windows; w++) {
            if((ltp->short_used[w] = faad_get1bit(ld)) & 1) {
                ltp->short_lag_present[w] = faad_get1bit(ld);
                if(ltp->short_lag_present[w]) { ltp->short_lag[w] = (uint8_t)faad_getbits(ld, 4); }
            }
        }
    }
    else {
        ltp->last_band = (ics->max_sfb < MAX_LTP_SFB ? ics->max_sfb : MAX_LTP_SFB);
        for(sfb = 0; sfb < ltp->last_band; sfb++) { ltp->long_used[sfb] = faad_get1bit(ld); }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.29 */
static uint8_t spectral_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, int16_t* spectral_data) {
    int8_t   i;
    uint8_t  g;
    uint16_t inc, k, p = 0;
    uint8_t  groups = 0;
    uint8_t  sect_cb;
    uint8_t  result;
    uint16_t nshort = hDecoder->frameLength / 8;

    for(g = 0; g < ics->num_window_groups; g++) {
        p = groups * nshort;
        for(i = 0; i < ics->num_sec[g]; i++) {
            sect_cb = ics->sect_cb[g][i];
            inc = (sect_cb >= FIRST_PAIR_HCB) ? 2 : 4;
            switch(sect_cb) {
            case ZERO_HCB:
            case NOISE_HCB:
            case INTENSITY_HCB:
            case INTENSITY_HCB2: p += (ics->sect_sfb_offset[g][ics->sect_end[g][i]] - ics->sect_sfb_offset[g][ics->sect_start[g][i]]); break;
            default:
                for(k = ics->sect_sfb_offset[g][ics->sect_start[g][i]]; k < ics->sect_sfb_offset[g][ics->sect_end[g][i]]; k += inc) {
                    if((result = huffman_spectral_data(sect_cb, ld, &spectral_data[p])) > 0) return result;
                    p += inc;
                }
                break;
            }
        }
        groups += ics->window_group_length[g];
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.30 */
static uint16_t extension_payload(bitfile_t* ld, drc_info_t* drc, uint16_t count) {
    uint16_t i, n, dataElementLength;
    uint8_t  dataElementLengthPart;
    uint8_t  align = 4, data_element_version, loopCounter;
    uint8_t  extension_type = (uint8_t)faad_getbits(ld, 4);

    switch(extension_type) {
    case EXT_DYNAMIC_RANGE:
        drc->present = 1;
        n = dynamic_range_info(ld, drc);
        return n;
    case EXT_FILL_DATA:
        /* fill_nibble = */ faad_getbits(ld, 4); /* must be '0000' */
        for(i = 0; i < count - 1; i++) {         /* fill_byte[i] = */
            faad_getbits(ld, 8);                 /* must be '10100101' */
        }
        return count;
    case EXT_DATA_ELEMENT:
        data_element_version = (uint8_t)faad_getbits(ld, 4);
        switch(data_element_version) {
        case ANC_DATA:
            loopCounter = 0;
            dataElementLength = 0;
            do {
                dataElementLengthPart = (uint8_t)faad_getbits(ld, 8);
                dataElementLength += dataElementLengthPart;
                loopCounter++;
            } while(dataElementLengthPart == 255);
            for(i = 0; i < dataElementLength; i++) {
                /* data_element_byte[i] = */ faad_getbits(ld, 8);
                return (dataElementLength + loopCounter + 1);
            }
            [[fallthrough]];
        default: align = 0; [[fallthrough]];
        }
    case EXT_FIL:
    default:
        faad_getbits(ld, align);
        for(i = 0; i < count - 1; i++) { /* other_bits[i] = */
            faad_getbits(ld, 8);
        }
        return count;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.31 */
static uint8_t dynamic_range_info(bitfile_t* ld, drc_info_t* drc) {
    uint8_t i, n = 1;
    uint8_t band_incr;

    drc->num_bands = 1;
    if(faad_get1bit(ld) & 1) {
        drc->pce_instance_tag = (uint8_t)faad_getbits(ld, 4);
        /* drc->drc_tag_reserved_bits = */ faad_getbits(ld, 4);
        n++;
    }
    drc->excluded_chns_present = faad_get1bit(ld);
    if(drc->excluded_chns_present == 1) { n += excluded_channels(ld, drc); }
    if(faad_get1bit(ld) & 1) {
        band_incr = (uint8_t)faad_getbits(ld, 4);
        /* drc->drc_bands_reserved_bits = */ faad_getbits(ld, 4);
        n++;
        drc->num_bands += band_incr;
        for(i = 0; i < drc->num_bands; i++) {
            drc->band_top[i] = (uint8_t)faad_getbits(ld, 8);
            n++;
        }
    }
    if(faad_get1bit(ld) & 1) {
        drc->prog_ref_level = (uint8_t)faad_getbits(ld, 7);
        /* drc->prog_ref_level_reserved_bits = */ faad_get1bit(ld);
        n++;
    }
    for(i = 0; i < drc->num_bands; i++) {
        drc->dyn_rng_sgn[i] = faad_get1bit(ld);
        drc->dyn_rng_ctl[i] = (uint8_t)faad_getbits(ld, 7);
        n++;
    }
    return n;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 4.4.32 */
static uint8_t excluded_channels(bitfile_t* ld, drc_info_t* drc) {
    uint8_t i, n = 0;
    uint8_t num_excl_chan = 7;

    for(i = 0; i < 7; i++) { drc->exclude_mask[i] = faad_get1bit(ld); }
    n++;
    while((drc->additional_excluded_chns[n - 1] = faad_get1bit(ld)) == 1) {
        if(i >= MAX_CHANNELS - num_excl_chan - 7) return n;
        for(i = num_excl_chan; i < num_excl_chan + 7; i++) { drc->exclude_mask[i] = faad_get1bit(ld); }
        n++;
        num_excl_chan += 7;
    }
    return n;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Annex A: Audio Interchange Formats */
/* Table 1.A.2 */
void get_adif_header_t(adif_header_t* adif, bitfile_t* ld) {
    uint8_t i;

    /* adif_id[0] = */ faad_getbits(ld, 8);
    /* adif_id[1] = */ faad_getbits(ld, 8);
    /* adif_id[2] = */ faad_getbits(ld, 8);
    /* adif_id[3] = */ faad_getbits(ld, 8);
    adif->copyright_id_present = faad_get1bit(ld);
    if(adif->copyright_id_present) {
        for(i = 0; i < 72 / 8; i++) { adif->copyright_id[i] = (int8_t)faad_getbits(ld, 8); }
        adif->copyright_id[i] = 0;
    }
    adif->original_copy = faad_get1bit(ld);
    adif->home = faad_get1bit(ld);
    adif->bitstream_type = faad_get1bit(ld);
    adif->bitrate = faad_getbits(ld, 23);
    adif->num_program_config_t_elements = (uint8_t)faad_getbits(ld, 4);
    for(i = 0; i < adif->num_program_config_t_elements + 1; i++) {
        if(adif->bitstream_type == 0) { adif->adif_buffer_fullness = faad_getbits(ld, 20); }
        else { adif->adif_buffer_fullness = 0; }
        program_config_t_element(&adif->pce[i], ld);
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 1.A.5 */
uint8_t adts_frame(adts_header_t* adts, bitfile_t* ld) {
    /* faad_byte_align(ld); */
    if(adts_fixed_header(adts, ld)) return 5;
    adts_variable_header(adts, ld);
    adts_error_check(adts, ld);
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 1.A.6 */
static uint8_t adts_fixed_header(adts_header_t* adts, bitfile_t* ld) {
    uint16_t i;
    uint8_t  sync_err = 1;

    /* try to recover from sync errors */
    for(i = 0; i < 768; i++) {
        adts->syncword = (uint16_t)faad_showbits(ld, 12);
        if(adts->syncword != 0xFFF) { faad_getbits(ld, 8); }
        else {
            sync_err = 0;
            faad_getbits(ld, 12);
            break;
        }
    }
    if(sync_err) return 5;
    adts->id = faad_get1bit(ld);
    adts->layer = (uint8_t)faad_getbits(ld, 2);
    adts->protection_absent = faad_get1bit(ld);
    adts->profile = (uint8_t)faad_getbits(ld, 2);
    adts->sf_index = (uint8_t)faad_getbits(ld, 4);
    adts->private_bit = faad_get1bit(ld);
    adts->channel_configuration = (uint8_t)faad_getbits(ld, 3);
    adts->original = faad_get1bit(ld);
    adts->home = faad_get1bit(ld);
    if(adts->old_format == 1) {
        /* Removed in corrigendum 14496-3:2002 */
        if(adts->id == 0) { adts->emphasis = (uint8_t)faad_getbits(ld, 2); }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 1.A.7 */
static void adts_variable_header(adts_header_t* adts, bitfile_t* ld) {
    adts->copyright_identification_bit = faad_get1bit(ld);
    adts->copyright_identification_start = faad_get1bit(ld);
    adts->aac_frame_length = (uint16_t)faad_getbits(ld, 13);
    adts->adts_buffer_fullness = (uint16_t)faad_getbits(ld, 11);
    adts->no_raw_data_blocks_in_frame = (uint8_t)faad_getbits(ld, 2);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Table 1.A.8 */
static void adts_error_check(adts_header_t* adts, bitfile_t* ld) {
    if(adts->protection_absent == 0) { adts->crc_check = (uint16_t)faad_getbits(ld, 16); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* LATM parsing functions */
static uint32_t latm_get_value(bitfile_t* ld) {
    uint32_t l, value;
    uint8_t  bytesForValue;
    bytesForValue = (uint8_t)faad_getbits(ld, 2);
    value = 0;
    for(l = 0; l < bytesForValue; l++) value = (value << 8) | (uint8_t)faad_getbits(ld, 8);
    return value;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t latmParsePayload(latm_header_t* latm, bitfile_t* ld) {
    // assuming there's only one program with a single layer and 1 subFrame,
    // allStreamsSametimeframing is set,
    uint32_t framelen;
    uint8_t  tmp;

    // this should be the payload length field for the current configuration
    framelen = 0;
    if(latm->framelen_type == 0) {
        do {
            tmp = (uint8_t)faad_getbits(ld, 8);
            framelen += tmp;
        } while(tmp == 0xff);
    }
    else if(latm->framelen_type == 1)
        framelen = latm->frameLength;
    return framelen;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t latmAudioMuxElement(latm_header_t* latm, bitfile_t* ld) {
    uint32_t         ascLen, asc_bits = 0;
    uint32_t         x1, y1, m, n, i;
    program_config_t pce;
    uint32_t         ret = 0;

    latm->useSameStreamMux = (uint8_t)faad_getbits(ld, 1);
    if(!latm->useSameStreamMux) {
        // parseSameStreamMuxConfig
        latm->version = (uint8_t)faad_getbits(ld, 1);
        if(latm->version) latm->versionA = (uint8_t)faad_getbits(ld, 1);
        if(latm->versionA) {
            // dunno the payload format for versionA
            // printf(ANSI_ESC_ORANGE "libfaad2 INFO; versionA not supported" ANSI_ESC_WHITE "\n");
            ret = 0;
            goto exit;
        }
        if(latm->version) // read taraBufferFullness
            latm_get_value(ld);
        latm->allStreamsSameTimeFraming = (uint8_t)faad_getbits(ld, 1);
        latm->numSubFrames = (uint8_t)faad_getbits(ld, 6) + 1;
        latm->numPrograms = (uint8_t)faad_getbits(ld, 4) + 1;
        latm->numLayers = faad_getbits(ld, 3) + 1;
        if(latm->numPrograms > 1 || !latm->allStreamsSameTimeFraming || latm->numSubFrames > 1 || latm->numLayers > 1) {
            // printf(ANSI_ESC_RED "libfaad2 INFO: Unsupported LATM configuration: %li programs/ %li subframes, %li layers, allstreams: %li" ANSI_ESC_WHITE "\n", latm->numPrograms, latm->numSubFrames,
            //       latm->numLayers, latm->allStreamsSameTimeFraming);
            ret = 0;
            goto exit;
        }
        ascLen = 0;
        if(latm->version) ascLen = latm_get_value(ld);
        x1 = faad_get_processed_bits(ld);

        if(AudioSpecificConfigFrombitfile(ld, &pce, 0, 1) < 0) {
            ret = 0;
            goto exit;
        }
        // horrid hack to unread the ASC bits and store them in latm->ASC
        // the correct code would rely on an ideal faad_ungetbits()
        y1 = faad_get_processed_bits(ld);
        if((y1 - x1) <= MAX_ASC_BYTES * 8) {
            faad_rewindbits(ld);
            m = x1;
            while(m > 0) {
                n = min(m, 32);
                faad_getbits(ld, n);
                m -= n;
            }
            i = 0;
            m = latm->ASCbits = y1 - x1;
            while(m > 0) {
                n = min(m, 8);
                latm->ASC[i++] = (uint8_t)faad_getbits(ld, n);
                m -= n;
            }
        }
        asc_bits = y1 - x1;
        if(ascLen > asc_bits) faad_getbits(ld, ascLen - asc_bits);
        latm->framelen_type = (uint8_t)faad_getbits(ld, 3);
        if(latm->framelen_type == 0) {
            latm->frameLength = 0;
            faad_getbits(ld, 8); // buffer fullness for frame_len_type==0, useless
        }
        else if(latm->framelen_type == 1) {
            latm->frameLength = faad_getbits(ld, 9);
            if(latm->frameLength == 0) {
                // printf(ANSI_ESC_ORANGE "libfaad2 INFO: Invalid frameLength: 0" ANSI_ESC_WHITE "\n");
                ret = 0;
                goto exit;
            }
            latm->frameLength = (latm->frameLength + 20) * 8;
        }
        else { // hellish CELP or HCVX stuff, discard
            // printf(ANSI_ESC_ORANGE "libfaad2 INFO: Unsupported CELP/HCVX framelentype: %li" ANSI_ESC_WHITE "\n", latm->framelen_type);
            ret = 0;
            goto exit;
        }
        latm->otherDataLenBits = 0;
        if(faad_getbits(ld, 1)) { // other data present
            int32_t esc, tmp;
            if(latm->version) latm->otherDataLenBits = latm_get_value(ld);
            else
                do {
                    esc = faad_getbits(ld, 1);
                    tmp = faad_getbits(ld, 8);
                    latm->otherDataLenBits = (latm->otherDataLenBits << 8) + tmp;
                } while(esc);
        }
        if(faad_getbits(ld, 1)) // crc
            faad_getbits(ld, 8);
        latm->inited = 1;
    }
    // read payload
    if(latm->inited) {
        ret = latmParsePayload(latm, ld);
        goto exit;
    }
    else {
        ret = 0;
        goto exit;
    }

exit:
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t faad_latm_frame(latm_header_t* latm, bitfile_t* ld) {
    uint16_t len;
    uint32_t initpos, endpos, firstpos, ret;

    firstpos = faad_get_processed_bits(ld);
    (void)firstpos;
    while(ld->bytes_left) {
        faad_byte_align(ld);
        if(faad_showbits(ld, 11) != 0x2B7) {
            faad_getbits(ld, 8);
            continue;
        }
        faad_getbits(ld, 11);
        len = faad_getbits(ld, 13);
        if(!len) continue;
        initpos = faad_get_processed_bits(ld);
        ret = latmAudioMuxElement(latm, ld);
        endpos = faad_get_processed_bits(ld);
        if(ret > 0) return (len * 8) - (endpos - initpos);
        // faad_getbits(ld, initpos-endpos); //go back to initpos, but is valid a getbits(-N) ?
    }
    return 0xFFFFFFFF;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*
  - determine the number of windows in a window_sequence named num_windows
  - determine the number of window_groups named num_window_groups
  - determine the number of windows in each group named window_group_length[g]
  - determine the total number of scalefactor window bands named num_swb for the actual window type
  - determine swb_offset[swb], the offset of the first coefficient in scalefactor window band named swb of the window actually used
  - determine sect_sfb_offset[g][section],the offset of the first coefficient in section named section. This offset depends on window_sequence and
    scale_factor_grouping and is needed to decode the spectral_data().
*/
uint8_t window_grouping_info(NeAACDecStruct_t* hDecoder, ic_stream_t* ics) {
    uint8_t i, g;
    uint8_t sf_index = hDecoder->sf_index;

    switch(ics->window_sequence) {
    case ONLY_LONG_SEQUENCE:
    case LONG_START_SEQUENCE:
    case LONG_STOP_SEQUENCE:
        ics->num_windows = 1;
        ics->num_window_groups = 1;
        ics->window_group_length[ics->num_window_groups - 1] = 1;
#ifdef LD_DEC
        if(hDecoder->object_type == LD) {
            if(hDecoder->frameLength == 512) ics->num_swb = num_swb_512_window[sf_index];
            else /* if (hDecoder->frameLength == 480) */
                ics->num_swb = num_swb_480_window[sf_index];
        }
        else {
#endif
            if(hDecoder->frameLength == 1024) ics->num_swb = num_swb_1024_window[sf_index];
            else /* if (hDecoder->frameLength == 960) */
                ics->num_swb = num_swb_960_window[sf_index];
#ifdef LD_DEC
        }
#endif
        if(ics->max_sfb > ics->num_swb) { return 32; }
        /* preparation of sect_sfb_offset for long blocks also copy the last value! */
#ifdef LD_DEC
        if(hDecoder->object_type == LD) {
            if(hDecoder->frameLength == 512) {
                for(i = 0; i < ics->num_swb; i++) {
                    ics->sect_sfb_offset[0][i] = swb_offset_512_window[sf_index][i];
                    ics->swb_offset[i] = swb_offset_512_window[sf_index][i];
                }
            }
            else /* if (hDecoder->frameLength == 480) */ {
                for(i = 0; i < ics->num_swb; i++) {
                    ics->sect_sfb_offset[0][i] = swb_offset_480_window[sf_index][i];
                    ics->swb_offset[i] = swb_offset_480_window[sf_index][i];
                }
            }
            ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset_max = hDecoder->frameLength;
        }
        else {
#endif
            for(i = 0; i < ics->num_swb; i++) {
                ics->sect_sfb_offset[0][i] = swb_offset_1024_window[sf_index][i];
                ics->swb_offset[i] = swb_offset_1024_window[sf_index][i];
            }
            ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset_max = hDecoder->frameLength;
#ifdef LD_DEC
        }
#endif
        return 0;
    case EIGHT_SHORT_SEQUENCE:
        ics->num_windows = 8;
        ics->num_window_groups = 1;
        ics->window_group_length[ics->num_window_groups - 1] = 1;
        ics->num_swb = num_swb_128_window[sf_index];
        if(ics->max_sfb > ics->num_swb) { return 32; }
        for(i = 0; i < ics->num_swb; i++) ics->swb_offset[i] = swb_offset_128_window[sf_index][i];
        ics->swb_offset[ics->num_swb] = hDecoder->frameLength / 8;
        ics->swb_offset_max = hDecoder->frameLength / 8;
        for(i = 0; i < ics->num_windows - 1; i++) {
            if(bit_set(ics->scale_factor_grouping, 6 - i) == 0) {
                ics->num_window_groups += 1;
                ics->window_group_length[ics->num_window_groups - 1] = 1;
            }
            else { ics->window_group_length[ics->num_window_groups - 1] += 1; }
        }
        /* preparation of sect_sfb_offset for short blocks */
        for(g = 0; g < ics->num_window_groups; g++) {
            uint16_t width;
            uint8_t  sect_sfb = 0;
            uint16_t offset = 0;
            for(i = 0; i < ics->num_swb; i++) {
                if(i + 1 == ics->num_swb) { width = (hDecoder->frameLength / 8) - swb_offset_128_window[sf_index][i]; }
                else { width = swb_offset_128_window[sf_index][i + 1] - swb_offset_128_window[sf_index][i]; }
                width *= ics->window_group_length[g];
                ics->sect_sfb_offset[g][sect_sfb++] = offset;
                offset += width;
            }
            ics->sect_sfb_offset[g][sect_sfb] = offset;
        }
        return 0;
    default: return 32;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int32_t iquant(int16_t q, const int32_t* tab, uint8_t* error) {
#ifdef FIXED_POINT
    /* For FIXED_POINT the iq_table is prescaled by 3 bits (iq_table[]/8) */
    /* BIG_IQ_TABLE allows you to use the full 8192 value table, if this is not
     * defined a 1026 value table and interpolation will be used
     */
    #ifndef BIG_IQ_TABLE
    static const int32_t errcorr[] = {REAL_CONST(0),         REAL_CONST(1.0 / 8.0), REAL_CONST(2.0 / 8.0), REAL_CONST(3.0 / 8.0), REAL_CONST(4.0 / 8.0),
                                     REAL_CONST(5.0 / 8.0), REAL_CONST(6.0 / 8.0), REAL_CONST(7.0 / 8.0), REAL_CONST(0)};
    int32_t              x1, x2;
    #endif
    int16_t sgn = 1;

    if(q < 0) {
        q = -q;
        sgn = -1;
    }

    if(q < IQ_TABLE_SIZE) {
    // #define IQUANT_PRINT
    #ifdef IQUANT_PRINT
        // printf("0x%.8X\n", sgn * tab[q]);
        printf("%d\n", sgn * tab[q]);
    #endif
        return sgn * tab[q];
    }

    #ifndef BIG_IQ_TABLE
    if(q >= 8192) {
        *error = 17;
        return 0;
    }

    /* linear interpolation */
    x1 = tab[q >> 3];
    x2 = tab[(q >> 3) + 1];
    return sgn * 16 * (MUL_R(errcorr[q & 7], (x2 - x1)) + x1);
    #else
    *error = 17;
    return 0;
    #endif

#else
    if(q < 0) {
        /* tab contains a value for all possible q [0,8192] */
        if(-q < IQ_TABLE_SIZE) return -tab[-q];

        *error = 17;
        return 0;
    }
    else {
        /* tab contains a value for all possible q [0,8192] */
        if(q < IQ_TABLE_SIZE) return tab[q];

        *error = 17;
        return 0;
    }
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* quant_to_spec: perform dequantisation and scaling and in case of short block it also does the deinterleaving */
/* For ONLY_LONG_SEQUENCE windows (num_window_groups = 1, window_group_length[0] = 1) the spectral data is in ascending spectral order.
  For the EIGHT_SHORT_SEQUENCE window, the spectral order depends on the grouping in the following manner:
  - Groups are ordered sequentially
  - Within a group, a scalefactor band consists of the spectral data of all grouped SHORT_WINDOWs for the associated scalefactor window band. To
  clarify via example, the length of a group is in the range of one to eight SHORT_WINDOWs.
  - If there are eight groups each with length one (num_window_groups = 8, window_group_length[0..7] = 1), the result is a sequence of eight spectra,
  each in ascending spectral order.
  - If there is only one group with length eight (num_window_groups = 1, window_group_length[0] = 8), the result is that spectral data of all eight
  SHORT_WINDOWs is interleaved by scalefactor window bands.
  - Within a scalefactor window band, the coefficients are in ascending spectral order.
*/
static uint8_t quant_to_spec(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, int16_t* quant_data, int32_t* spec_data, uint16_t frame_len) {

    static const int32_t pow2_table[] = {
        COEF_CONST(1.0), COEF_CONST(1.1892071150027210667174999705605), /* 2^0.25 */
        COEF_CONST(1.4142135623730950488016887242097),                  /* 2^0.5 */
        COEF_CONST(1.6817928305074290860622509524664)                   /* 2^0.75 */
    };
    const int32_t* tab = iq_table;

    uint8_t  g, sfb, win;
    uint16_t width, bin, k, gindex;
    uint8_t  error = 0; /* Init error flag */
#ifndef FIXED_POINT
    int32_t scf;
#else
    int32_t sat_shift_mask = 0;
#endif

    k = 0;
    gindex = 0;

    /* In this case quant_to_spec is no-op and spec_data remains undefined.
     * Without peeking into AAC specification, there is no strong evidence if
     * such streams are invalid -> just calm down MSAN. */
    if(ics->num_swb == 0) memset(spec_data, 0, frame_len * sizeof(int32_t));

    for(g = 0; g < ics->num_window_groups; g++) {
        uint16_t j = 0;
        uint16_t gincrease = 0;
        uint16_t win_inc = ics->swb_offset[ics->num_swb];

        for(sfb = 0; sfb < ics->num_swb; sfb++) {
            int32_t  exp, frac;
            uint16_t wa = gindex + j;
            int16_t  scale_factor = ics->scale_factors[g][sfb];

            width = ics->swb_offset[sfb + 1] - ics->swb_offset[sfb];

#ifdef FIXED_POINT
            scale_factor -= 100;
            /* IMDCT pre-scaling */
            if(hDecoder->object_type == LD) { scale_factor -= 24 /*9*/; }
            else {
                if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) scale_factor -= 16 /*7*/;
                else scale_factor -= 28 /*10*/;
            }
            if(scale_factor > 120) scale_factor = 120; /* => exp <= 30 */
#else
            (void)hDecoder;
#endif
            /* scale_factor for IS or PNS, has different meaning; fill with almost zeroes */
            if(is_intensity(ics, g, sfb) || is_noise(ics, g, sfb)) { scale_factor = 0; }

            /* scale_factor must be between 0 and 255 */
            exp = (scale_factor /* - 100 */) >> 2;
            /* frac must always be > 0 */
            frac = (scale_factor /* - 100 */) & 3;

#ifndef FIXED_POINT
            scf = pow2sf_tab[exp /*+25*/] * pow2_table[frac];
#else
            if(exp > 0) sat_shift_mask = SAT_SHIFT_MASK(exp);
#endif

            for(win = 0; win < ics->window_group_length[g]; win++) {
                for(bin = 0; bin < width; bin += 4) {
                    uint16_t wb = wa + bin;
#ifndef FIXED_POINT
                    spec_data[wb + 0] = iquant(quant_data[k + 0], tab, &error) * scf;
                    spec_data[wb + 1] = iquant(quant_data[k + 1], tab, &error) * scf;
                    spec_data[wb + 2] = iquant(quant_data[k + 2], tab, &error) * scf;
                    spec_data[wb + 3] = iquant(quant_data[k + 3], tab, &error) * scf;
#else
                    int32_t iq0 = iquant(quant_data[k + 0], tab, &error);
                    int32_t iq1 = iquant(quant_data[k + 1], tab, &error);
                    int32_t iq2 = iquant(quant_data[k + 2], tab, &error);
                    int32_t iq3 = iquant(quant_data[k + 3], tab, &error);

                    if(exp == -32) {
                        spec_data[wb + 0] = 0;
                        spec_data[wb + 1] = 0;
                        spec_data[wb + 2] = 0;
                        spec_data[wb + 3] = 0;
                    }
                    else if(exp <= 0) {
                        spec_data[wb + 0] = iq0 >> -exp;
                        spec_data[wb + 1] = iq1 >> -exp;
                        spec_data[wb + 2] = iq2 >> -exp;
                        spec_data[wb + 3] = iq3 >> -exp;
                    }
                    else { /* exp > 0 */ spec_data[wb + 0] = SAT_SHIFT(iq0, exp, sat_shift_mask);
                        spec_data[wb + 1] = SAT_SHIFT(iq1, exp, sat_shift_mask);
                        spec_data[wb + 2] = SAT_SHIFT(iq2, exp, sat_shift_mask);
                        spec_data[wb + 3] = SAT_SHIFT(iq3, exp, sat_shift_mask);
                    }
                    if(frac != 0) {
                        spec_data[wb + 0] = MUL_C(spec_data[wb + 0], pow2_table[frac]);
                        spec_data[wb + 1] = MUL_C(spec_data[wb + 1], pow2_table[frac]);
                        spec_data[wb + 2] = MUL_C(spec_data[wb + 2], pow2_table[frac]);
                        spec_data[wb + 3] = MUL_C(spec_data[wb + 3], pow2_table[frac]);
                    }

    // #define SCFS_PRINT
    #ifdef SCFS_PRINT
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 0]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 1]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 2]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 3]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+0]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+1]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+2]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+3]);
    #endif
#endif
                    gincrease += 4;
                    k += 4;
                }
                wa += win_inc;
            }
            j += width;
        }
        gindex += gincrease;
    }
    return error;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void faad_free(void* b) {free(b); b = NULL;};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t allocate_single_channel(NeAACDecStruct_t* hDecoder, uint8_t channel, uint8_t output_channels) {
    (void)output_channels;
    int32_t mul = 1;

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] != NULL) {
            faad_free(hDecoder->pred_stat[channel]);
            hDecoder->pred_stat[channel] = NULL;
        }

        hDecoder->pred_stat[channel] = (pred_state_t*)faad_malloc(hDecoder->frameLength * sizeof(pred_state_t));
        reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
    }
#endif

#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
        /* allocate the state only when needed */
        if(hDecoder->lt_pred_stat[channel] != NULL) {
            faad_free(hDecoder->lt_pred_stat[channel]);
            hDecoder->lt_pred_stat[channel] = NULL;
        }
        hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
        memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
    }
#endif

    if(hDecoder->time_out[channel] != NULL) {
        faad_free(hDecoder->time_out[channel]);
        hDecoder->time_out[channel] = NULL;
    }
    {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (int32_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->time_out[channel], 0, mul * hDecoder->frameLength * sizeof(int32_t));
    }
#if(defined(PS_DEC))
    if(output_channels == 2) {
        if(hDecoder->time_out[channel + 1] != NULL) {
            faad_free(hDecoder->time_out[channel + 1]);
            hDecoder->time_out[channel + 1] = NULL;
        }

        hDecoder->time_out[channel + 1] = (int32_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->time_out[channel + 1], 0, mul * hDecoder->frameLength * sizeof(int32_t));
    }
#endif
    if(hDecoder->fb_intermed[channel] != NULL) {
        faad_free(hDecoder->fb_intermed[channel]);
        hDecoder->fb_intermed[channel] = NULL;
    }
    hDecoder->fb_intermed[channel] = (int32_t*)faad_malloc(hDecoder->frameLength * sizeof(int32_t));
    memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength * sizeof(int32_t));
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t allocate_channel_pair(NeAACDecStruct_t* hDecoder, uint8_t channel, uint8_t paired_channel) {
    int32_t mul = 1;

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] == NULL) {
            hDecoder->pred_stat[channel] = (pred_state_t*)faad_malloc(hDecoder->frameLength * sizeof(pred_state_t));
            reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
        }
        if (hDecoder->pred_stat[paired_channel] == NULL) {
            hDecoder->pred_stat[paired_channel] = (pred_state_t*)faad_malloc(hDecoder->frameLength * sizeof(pred_state_t));
            reset_all_predictors(hDecoder->pred_stat[paired_channel], hDecoder->frameLength);
        }
    }
#endif

#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
        /* allocate the state only when needed */
        if(hDecoder->lt_pred_stat[channel] == NULL) {
            hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
        }
        if(hDecoder->lt_pred_stat[paired_channel] == NULL) {
            hDecoder->lt_pred_stat[paired_channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[paired_channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
        }
    }
#endif
    if(hDecoder->time_out[channel] == NULL) {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (int32_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->time_out[channel], 0, mul * hDecoder->frameLength * sizeof(int32_t));
    }
    if(hDecoder->time_out[paired_channel] == NULL) {
        hDecoder->time_out[paired_channel] = (int32_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->time_out[paired_channel], 0, mul * hDecoder->frameLength * sizeof(int32_t));
    }

    if(hDecoder->fb_intermed[channel] == NULL) {
        hDecoder->fb_intermed[channel] = (int32_t*)faad_malloc(hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength * sizeof(int32_t));
    }
    if(hDecoder->fb_intermed[paired_channel] == NULL) {
        hDecoder->fb_intermed[paired_channel] = (int32_t*)faad_malloc(hDecoder->frameLength * sizeof(int32_t));
        memset(hDecoder->fb_intermed[paired_channel], 0, hDecoder->frameLength * sizeof(int32_t));
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t reconstruct_single_channel(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, element_t* sce, int16_t* spec_data) {
    uint8_t retval;
    int32_t output_channels;

    /* always allocate 2 channels, PS can always "suddenly" turn up */

    if(hDecoder->ps_used[hDecoder->fr_ch_ele]) output_channels = 2;
    else
        output_channels = 1;
    if(hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0) {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = output_channels;
    }
    else if(hDecoder->element_output_channels[hDecoder->fr_ch_ele] != output_channels) {
        /* element inconsistency, this only happens if PS is actually found but not in the first frame this means that there is only 1 bitstream
         * element! */
        /* reset the allocation */
        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 0;
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = output_channels;
        // return 21;
    }
    if(hDecoder->element_alloced[hDecoder->fr_ch_ele] == 0) {
        retval = allocate_single_channel(hDecoder, sce->channel, output_channels);
        if(retval > 0) return retval;

        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 1;
    }
    /* sanity check, CVE-2018-20199, CVE-2018-20360 */
    if(!hDecoder->time_out[sce->channel]) return 15;
    if(output_channels > 1 && !hDecoder->time_out[sce->channel + 1]) return 15;
    if(!hDecoder->fb_intermed[sce->channel]) return 15;
    /* dequantisation and scaling */
    //   int32_t* m_spec_coef = (int32_t*)faad_malloc(1024 * sizeof(int32_t));
    retval = quant_to_spec(hDecoder, ics, spec_data, m_spec_coef, hDecoder->frameLength);
    if(retval > 0) {
        return retval;
    }
    /* pns decoding */
    pns_decode(ics, NULL, m_spec_coef, NULL, hDecoder->frameLength, 0, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2));

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN){
		if (!hDecoder->pred_stat[sce->channel])
			return 33;

        /* intra channel prediction */
        ic_prediction(ics, m_spec_coef, hDecoder->pred_stat[sce->channel], hDecoder->frameLength,
            hDecoder->sf_index);

        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics, hDecoder->pred_stat[sce->channel]);
    }
#endif

#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
    #ifdef LD_DEC
        if(hDecoder->object_type == LD) {
            if(ics->ltp.data_present) {
                if(ics->ltp.lag_update) hDecoder->ltp_lag[sce->channel] = ics->ltp.lag;
            }
            ics->ltp.lag = hDecoder->ltp_lag[sce->channel];
        }
    #endif
        /* long term prediction */
        lt_prediction(ics, &(ics->ltp), m_spec_coef, hDecoder->lt_pred_stat[sce->channel], hDecoder->fb, ics->window_shape, hDecoder->window_shape_prev[sce->channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
    }
#endif
    /* tns decoding */
    tns_decode_frame(ics, &(ics->tns), hDecoder->sf_index, hDecoder->object_type, m_spec_coef, hDecoder->frameLength);
    /* filter bank */
    ifilter_bank(hDecoder->fb, ics->window_sequence, ics->window_shape, hDecoder->window_shape_prev[sce->channel], m_spec_coef, hDecoder->time_out[sce->channel], hDecoder->fb_intermed[sce->channel],
                 hDecoder->object_type, hDecoder->frameLength);

    /* save window shape for next frame */
    hDecoder->window_shape_prev[sce->channel] = ics->window_shape;
#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
        lt_update_state(hDecoder->lt_pred_stat[sce->channel], hDecoder->time_out[sce->channel], hDecoder->fb_intermed[sce->channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif
#ifdef SBR_DEC
    if(((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        int32_t ele = hDecoder->fr_ch_ele;
        int32_t ch = sce->channel;
        /* following case can happen when forceUpSampling == 1 */
        if(hDecoder->sbr[ele] == NULL) { hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR); }
        if(!hDecoder->sbr[ele]) return 19;
        if(sce->ics1.window_sequence == EIGHT_SHORT_SEQUENCE) hDecoder->sbr[ele]->maxAACLine = 8 * min(sce->ics1.swb_offset[max(sce->ics1.max_sfb - 1, 0)], sce->ics1.swb_offset_max);
        else
            hDecoder->sbr[ele]->maxAACLine = min(sce->ics1.swb_offset[max(sce->ics1.max_sfb - 1, 0)], sce->ics1.swb_offset_max);
            /* check if any of the PS tools is used */
    #if(defined(PS_DEC))
        if(hDecoder->ps_used[ele] == 0) {
    #endif
            retval = sbrDecodeSingleFrame(hDecoder->sbr[ele], hDecoder->time_out[ch], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR); hDecoder->isPS = 0;
    #if(defined(PS_DEC))
        }
        else { retval = sbrDecodeSingleFramePS(hDecoder->sbr[ele], hDecoder->time_out[ch], hDecoder->time_out[ch + 1], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR); hDecoder->isPS = 1;}
    #endif
        if(retval > 0) return retval;
    }
    else if(((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) { return 23; }
#endif
    /* copy L to R when no PS is used */
#if(defined(PS_DEC))
    if((hDecoder->ps_used[hDecoder->fr_ch_ele] == 0) && (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2)) {
        int32_t ele = hDecoder->fr_ch_ele;
        int32_t ch = sce->channel;
        int32_t frame_size = (hDecoder->sbr_alloced[ele]) ? 2 : 1;
        frame_size *= hDecoder->frameLength * sizeof(int32_t);
        memcpy(hDecoder->time_out[ch + 1], hDecoder->time_out[ch], frame_size);
    }
#endif
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t reconstruct_channel_pair(NeAACDecStruct_t* hDecoder, ic_stream_t* ics1, ic_stream_t* ics2, element_t* cpe, int16_t* spec_data1, int16_t* spec_data2) {
    uint8_t retval;

    if(hDecoder->element_alloced[hDecoder->fr_ch_ele] != 2) {
        retval = allocate_channel_pair(hDecoder, cpe->channel, (uint8_t)cpe->paired_channel);
        if(retval > 0) {
            return retval;
        }
        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 2;
    }
    /* sanity check, CVE-2018-20199, CVE-2018-20360 */
    if(!hDecoder->time_out[cpe->channel] || !hDecoder->time_out[cpe->paired_channel]) {
         return 15;
    }
    if(!hDecoder->fb_intermed[cpe->channel] || !hDecoder->fb_intermed[cpe->paired_channel]) {
        return 15;
    }
    /* dequantisation and scaling */
    retval = quant_to_spec(hDecoder, ics1, spec_data1, m_spec_coef1, hDecoder->frameLength);
    if(retval > 0) {
        return retval;
    }
    retval = quant_to_spec(hDecoder, ics2, spec_data2, m_spec_coef2, hDecoder->frameLength);
    if(retval > 0) {
        return retval;
    }
    /* pns decoding */
    if(ics1->ms_mask_present) { pns_decode(ics1, ics2, m_spec_coef1, m_spec_coef2, hDecoder->frameLength, 1, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2)); }
    else { pns_decode(ics1, ics2, m_spec_coef1, m_spec_coef2, hDecoder->frameLength, 0, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2)); }
    /* mid/side decoding */
    ms_decode(ics1, ics2, m_spec_coef1, m_spec_coef2, hDecoder->frameLength);
    /* intensity stereo decoding */
    is_decode(ics1, ics2, m_spec_coef1, m_spec_coef2, hDecoder->frameLength);

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* intra channel prediction */
        ic_prediction(ics1, m_spec_coef1, hDecoder->pred_stat[cpe->channel], hDecoder->frameLength,
            hDecoder->sf_index);
        ic_prediction(ics2, m_spec_coef2, hDecoder->pred_stat[cpe->paired_channel], hDecoder->frameLength,
            hDecoder->sf_index);

        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics1, hDecoder->pred_stat[cpe->channel]);
        pns_reset_pred_state(ics2, hDecoder->pred_stat[cpe->paired_channel]);
    }
#endif

#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
        ltp_info_t* ltp1 = &(ics1->ltp);
        ltp_info_t* ltp2 = (cpe->common_window) ? &(ics2->ltp2) : &(ics2->ltp);
    #ifdef LD_DEC
        if(hDecoder->object_type == LD) {
            if(ltp1->data_present) {
                if(ltp1->lag_update) hDecoder->ltp_lag[cpe->channel] = ltp1->lag;
            }
            ltp1->lag = hDecoder->ltp_lag[cpe->channel];
            if(ltp2->data_present) {
                if(ltp2->lag_update) hDecoder->ltp_lag[cpe->paired_channel] = ltp2->lag;
            }
            ltp2->lag = hDecoder->ltp_lag[cpe->paired_channel];
        }
    #endif
        /* long term prediction */
        lt_prediction(ics1, ltp1, m_spec_coef1, hDecoder->lt_pred_stat[cpe->channel], hDecoder->fb, ics1->window_shape, hDecoder->window_shape_prev[cpe->channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
        lt_prediction(ics2, ltp2, m_spec_coef2, hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->fb, ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
    }
#endif
    /* tns decoding */
    tns_decode_frame(ics1, &(ics1->tns), hDecoder->sf_index, hDecoder->object_type, m_spec_coef1, hDecoder->frameLength);
    tns_decode_frame(ics2, &(ics2->tns), hDecoder->sf_index, hDecoder->object_type, m_spec_coef2, hDecoder->frameLength);
    /* filter bank */
    ifilter_bank(hDecoder->fb, ics1->window_sequence, ics1->window_shape, hDecoder->window_shape_prev[cpe->channel], m_spec_coef1, hDecoder->time_out[cpe->channel],
                 hDecoder->fb_intermed[cpe->channel], hDecoder->object_type, hDecoder->frameLength);
    ifilter_bank(hDecoder->fb, ics2->window_sequence, ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel], m_spec_coef2, hDecoder->time_out[cpe->paired_channel],
                 hDecoder->fb_intermed[cpe->paired_channel], hDecoder->object_type, hDecoder->frameLength);
    /* save window shape for next frame */
    hDecoder->window_shape_prev[cpe->channel] = ics1->window_shape;
    hDecoder->window_shape_prev[cpe->paired_channel] = ics2->window_shape;
#ifdef LTP_DEC
    if(is_ltp_ot(hDecoder->object_type)) {
        lt_update_state(hDecoder->lt_pred_stat[cpe->channel], hDecoder->time_out[cpe->channel], hDecoder->fb_intermed[cpe->channel], hDecoder->frameLength, hDecoder->object_type);
        lt_update_state(hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->time_out[cpe->paired_channel], hDecoder->fb_intermed[cpe->paired_channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif
#ifdef SBR_DEC
    if(((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        int32_t ele = hDecoder->fr_ch_ele;
        int32_t ch0 = cpe->channel;
        int32_t ch1 = cpe->paired_channel;
        /* following case can happen when forceUpSampling == 1 */
        if(hDecoder->sbr[ele] == NULL) { hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR); }
        if(!hDecoder->sbr[ele]) {
            return 19;
        }
        if(cpe->ics1.window_sequence == EIGHT_SHORT_SEQUENCE) hDecoder->sbr[ele]->maxAACLine = 8 * min(cpe->ics1.swb_offset[max(cpe->ics1.max_sfb - 1, 0)], cpe->ics1.swb_offset_max);
        else
            hDecoder->sbr[ele]->maxAACLine = min(cpe->ics1.swb_offset[max(cpe->ics1.max_sfb - 1, 0)], cpe->ics1.swb_offset_max);
        retval = sbrDecodeCoupleFrame(hDecoder->sbr[ele], hDecoder->time_out[ch0], hDecoder->time_out[ch1], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR);
        if(retval > 0) {
            return retval;
        }
    }
    else if(((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        return 23;
    }
#endif
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* circumvent memory alignment errors on ARM */
static uint32_t getdword(void* mem) {
    uint32_t tmp;
    ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[3];
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2];
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1];
    ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0];
    return tmp;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* reads only n bytes from the stream instead of the standard 4 */
static uint32_t getdword_n(void* mem, int32_t n) {
    uint32_t tmp = 0;
    switch(n) {
    case 3: ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2]; [[fallthrough]];
    case 2: ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1]; [[fallthrough]];
    case 1: ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0]; [[fallthrough]];
    default: break;
    }
    return tmp;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t faad_showbits(bitfile_t* ld, uint32_t bits) {
    if(bits <= ld->bits_left) {
        // return (ld->bufa >> (ld->bits_left - bits)) & bitmask[bits];
        return (ld->bufa << (32 - ld->bits_left)) >> (32 - bits);
    }
    bits -= ld->bits_left;
    // return ((ld->bufa & bitmask[ld->bits_left]) << bits) | (ld->bufb >> (32 - bits));
    return ((ld->bufa & ((1u << ld->bits_left) - 1)) << bits) | (ld->bufb >> (32 - bits));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
UNUSED_FUNCTION static void DCT3_4_unscaled(int32_t* y, int32_t* x) {
    int32_t f0, f1, f2, f3, f4, f5, f6, f7, f8;

    f0 = MUL_F(x[2], FRAC_CONST(0.7071067811865476));
    f1 = x[0] - f0;
    f2 = x[0] + f0;
    f3 = x[1] + x[3];
    f4 = MUL_C(x[1], COEF_CONST(1.3065629648763766));
    f5 = MUL_F(f3, FRAC_CONST(-0.9238795325112866));
    f6 = MUL_F(x[3], FRAC_CONST(-0.5411961001461967));
    f7 = f4 + f5;
    f8 = f6 - f5;
    y[3] = f2 - f8;
    y[0] = f2 + f8;
    y[2] = f1 - f7;
    y[1] = f1 + f7;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t showbits_hcr(bits_t_t* ld, uint8_t bits) {
    uint32_t mask;
    int8_t tail;
    if (bits == 0) return 0;
    if (ld->len == 0) return 0;
    tail = ld->len - bits;
    mask = 0xFFFFFFFF >> (32 - bits);
    if (ld->len <= 32){
        /* huffman_spectral_data_2 might request more than available (tail < 0),
           pad with zeroes then. */
        if (tail >= 0) return (ld->bufa >> tail) & mask; /* tail is 0..31 */
        else           return (ld->bufa << -tail) & mask; /* -tail is 1..31 */
    } else {
        /* tail is 1..63 */
        if (tail < 32)
            return ((ld->bufb << (32 - tail)) | (ld->bufa >> tail)) & mask;
        else
            return (ld->bufb >> (tail - 32)) & mask;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* return next n bits (right adjusted) */
static uint32_t faad_getbits(bitfile_t* ld, uint32_t n) {
    uint32_t ret;
    if(n == 0) return 0;
    ret = faad_showbits(ld, n);
    faad_flushbits(ld, n);
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void faad_flushbits_rev(bitfile_t* ld, uint32_t bits) {
    /* do nothing if error */
    if(ld->error != 0) return;
    if(bits < ld->bits_left) { ld->bits_left -= bits; }
    else {
        uint32_t tmp;
        ld->bufa = ld->bufb;
        tmp = getdword(ld->start);
        ld->bufb = tmp;
        ld->start--;
        ld->bits_left += (32 - bits);
        if(ld->bytes_left < 4) {
            ld->error = 1;
            ld->bytes_left = 0;
        }
        else { ld->bytes_left -= 4; }
        //        if (ld->bytes_left == 0)
        //            ld->no_more_reading = 1;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t faad_getbits_rev(bitfile_t* ld, uint32_t n) {
    uint32_t ret;

    if(n == 0) return 0;
    ret = faad_showbits_rev(ld, n);
    faad_flushbits_rev(ld, n);
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void reset_pred_state(pred_state_t *state)
{
    state->r[0]   = 0;
    state->r[1]   = 0;
    state->COR[0] = 0;
    state->COR[1] = 0;
    state->VAR[0] = 0x3F80;
    state->VAR[1] = 0x3F80;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void reset_all_predictors(pred_state_t *state, uint16_t frame_len){
    uint16_t i;
    for (i = 0; i < frame_len; i++)
        reset_pred_state(&state[i]);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* intra channel prediction */
static void ic_prediction(ic_stream_t *ics, int32_t *spec, pred_state_t *state, uint16_t frame_len, uint8_t sf_index){
    uint8_t sfb;
    uint16_t bin;

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        reset_all_predictors(state, frame_len);
    }
    else {
        for (sfb = 0; sfb < max_pred_sfb(sf_index); sfb++){
            uint16_t low  = ics->swb_offset[sfb];
            uint16_t high = min(ics->swb_offset[sfb+1], ics->swb_offset_max);

            for (bin = low; bin < high; bin++){
                ic_predict(&state[bin], spec[bin], &spec[bin],
                    (ics->predictor_data_present && ics->pred.prediction_used[sfb]));
            }
        }

        if (ics->predictor_data_present) {
            if (ics->pred.predictor_reset){
                for (bin = ics->pred.predictor_reset_group_number - 1;
                     bin < frame_len; bin += 30)
                {
                    reset_pred_state(&state[bin]);
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t float_to_bits(float f32) {
    uint32_t u32;
    memcpy(&u32, &f32, 4);
    return u32;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static float bits_to_float(uint32_t u32) {
    float f32;
    memcpy(&f32, &u32, 4);
    return f32;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static int16_t quant_pred(float x){
    return (int16_t)(float_to_bits(x) >> 16);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static float inv_quant_pred(int16_t q){
    uint16_t u16 = (uint16_t)q;
    return bits_to_float((uint32_t)u16 << 16);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static float flt_round(float_t pf) {
    int32_t  flg;
    uint32_t tmp, tmp1, tmp2;

    tmp = float_to_bits(pf);
    flg = tmp & (uint32_t)0x00008000;
    tmp &= (uint32_t)0xffff0000;
    tmp1 = tmp;
    /* round 1/2 lsb toward infinity */
    if(flg) {
        tmp &= (uint32_t)0xff800000; /* extract exponent and sign */
        tmp |= (uint32_t)0x00010000; /* insert 1 lsb */
        tmp2 = tmp;                  /* add 1 lsb and elided one */
        tmp &= (uint32_t)0xff800000; /* extract exponent and sign */

        return bits_to_float(tmp1) + bits_to_float(tmp2) - bits_to_float(tmp);
    }
    else { return bits_to_float(tmp); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void ic_predict(pred_state_t *state, int32_t input, int32_t *output, uint8_t pred){

#ifdef FIXED_POINT
    // main codepath is simply not ready for FIXED_POINT, better not to run it at all.
    if (pred)
        *output = input;
#else
    uint16_t tmp;
    int16_t i, j;
    int32_t dr1;
	float predictedvalue;
    int32_t e0, e1;
    int32_t k1, k2;

    int32_t r[2];
    int32_t COR[2];
    int32_t VAR[2];

    r[0] = inv_quant_pred(state->r[0]);
    r[1] = inv_quant_pred(state->r[1]);
    COR[0] = inv_quant_pred(state->COR[0]);
    COR[1] = inv_quant_pred(state->COR[1]);
    VAR[0] = inv_quant_pred(state->VAR[0]);
    VAR[1] = inv_quant_pred(state->VAR[1]);

    tmp = state->VAR[0];
    j = (tmp >> 7);
    i = tmp & 0x7f;
    if (j >= 128) {
        j -= 128;
        k1 = COR[0] * exp_table[j] * mnt_table[i];
    }
    else {
        k1 = REAL_CONST(0);
    }


    if (pred) {
        tmp = state->VAR[1];
        j = (tmp >> 7);
        i = tmp & 0x7f;
        if (j >= 128)
        {
            j -= 128;
            k2 = COR[1] * exp_table[j] * mnt_table[i];
        } else {
            k2 = REAL_CONST(0);
        }

        predictedvalue = k1*r[0] + k2*r[1];
        predictedvalue = flt_round(predictedvalue);
        *output = input + predictedvalue;
    }

    /* calculate new state data */
    e0 = *output;
    e1 = e0 - k1*r[0];
    dr1 = k1*e0;

    VAR[0] = ALPHA*VAR[0] + 0.5f * (r[0]*r[0] + e0*e0);
    COR[0] = ALPHA*COR[0] + r[0]*e0;
    VAR[1] = ALPHA*VAR[1] + 0.5f * (r[1]*r[1] + e1*e1);
    COR[1] = ALPHA*COR[1] + r[1]*e1;
#define A  REAL_CONST(0.953125)
    r[1] = A * (r[0]-dr1);
    r[0] = A * e0;

    state->r[0] = quant_pred(r[0]);
    state->r[1] = quant_pred(r[1]);
    state->COR[0] = quant_pred(COR[0]);
    state->COR[1] = quant_pred(COR[1]);
    state->VAR[0] = quant_pred(VAR[0]);
    state->VAR[1] = quant_pred(VAR[1]);
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void pns_reset_pred_state(ic_stream_t *ics, pred_state_t *state){
    uint8_t sfb, g, b;
    uint16_t i, offs, offs2;

    /* prediction only for long blocks */
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
        return;

    for (g = 0; g < ics->num_window_groups; g++) {
        for (b = 0; b < ics->window_group_length[g]; b++) {
            for (sfb = 0; sfb < ics->max_sfb; sfb++){
                if (is_noise(ics, g, sfb)) {
                    offs = ics->swb_offset[sfb];
                    offs2 = min(ics->swb_offset[sfb+1], ics->swb_offset_max);

                    for (i = offs; i < offs2; i++)
                        reset_pred_state(&state[i]);
                }
            }
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void DST4_32(int32_t* y, int32_t* x) {
    // printf(ANSI_ESC_YELLOW "DST4_32" ANSI_ESC_WHITE "\n");
    int32_t* f = (int32_t*)faad_malloc(336 * sizeof(int32_t));
    f[0] = x[0] - x[1];
    f[1] = x[2] - x[1];
    f[2] = x[2] - x[3];
    f[3] = x[4] - x[3];
    f[4] = x[4] - x[5];
    f[5] = x[6] - x[5];
    f[6] = x[6] - x[7];
    f[7] = x[8] - x[7];
    f[8] = x[8] - x[9];
    f[9] = x[10] - x[9];
    f[10] = x[10] - x[11];
    f[11] = x[12] - x[11];
    f[12] = x[12] - x[13];
    f[13] = x[14] - x[13];
    f[14] = x[14] - x[15];
    f[15] = x[16] - x[15];
    f[16] = x[16] - x[17];
    f[17] = x[18] - x[17];
    f[18] = x[18] - x[19];
    f[19] = x[20] - x[19];
    f[20] = x[20] - x[21];
    f[21] = x[22] - x[21];
    f[22] = x[22] - x[23];
    f[23] = x[24] - x[23];
    f[24] = x[24] - x[25];
    f[25] = x[26] - x[25];
    f[26] = x[26] - x[27];
    f[27] = x[28] - x[27];
    f[28] = x[28] - x[29];
    f[29] = x[30] - x[29];
    f[30] = x[30] - x[31];
    f[31] = MUL_F(FRAC_CONST(0.7071067811865476), f[15]);
    f[32] = x[0] - f[31];
    f[33] = x[0] + f[31];
    f[34] = f[7] + f[23];
    f[35] = MUL_C(COEF_CONST(1.3065629648763766), f[7]);
    f[36] = MUL_F(FRAC_CONST(-0.9238795325112866), f[34]);
    f[37] = MUL_F(FRAC_CONST(-0.5411961001461967), f[23]);
    f[38] = f[35] + f[36];
    f[39] = f[37] - f[36];
    f[40] = f[33] - f[39];
    f[41] = f[33] + f[39];
    f[42] = f[32] - f[38];
    f[43] = f[32] + f[38];
    f[44] = f[11] - f[19];
    f[45] = f[11] + f[19];
    f[46] = MUL_F(FRAC_CONST(0.7071067811865476), f[45]);
    f[47] = f[3] - f[46];
    f[48] = f[3] + f[46];
    f[49] = MUL_F(FRAC_CONST(0.7071067811865476), f[44]);
    f[50] = f[49] - f[27];
    f[51] = f[49] + f[27];
    f[52] = f[51] + f[48];
    f[53] = MUL_F(FRAC_CONST(-0.7856949583871021), f[51]);
    f[54] = MUL_F(FRAC_CONST(0.9807852804032304), f[52]);
    f[55] = MUL_C(COEF_CONST(1.1758756024193588), f[48]);
    f[56] = f[53] + f[54];
    f[57] = f[55] - f[54];
    f[58] = f[50] + f[47];
    f[59] = MUL_F(FRAC_CONST(-0.2758993792829430), f[50]);
    f[60] = MUL_F(FRAC_CONST(0.8314696123025452), f[58]);
    f[61] = MUL_C(COEF_CONST(1.3870398453221475), f[47]);
    f[62] = f[59] + f[60];
    f[63] = f[61] - f[60];
    f[64] = f[41] - f[56];
    f[65] = f[41] + f[56];
    f[66] = f[43] - f[62];
    f[67] = f[43] + f[62];
    f[68] = f[42] - f[63];
    f[69] = f[42] + f[63];
    f[70] = f[40] - f[57];
    f[71] = f[40] + f[57];
    f[72] = f[5] - f[9];
    f[73] = f[5] + f[9];
    f[74] = f[13] - f[17];
    f[75] = f[13] + f[17];
    f[76] = f[21] - f[25];
    f[77] = f[21] + f[25];
    f[78] = MUL_F(FRAC_CONST(0.7071067811865476), f[75]);
    f[79] = f[1] - f[78];
    f[80] = f[1] + f[78];
    f[81] = f[73] + f[77];
    f[82] = MUL_C(COEF_CONST(1.3065629648763766), f[73]);
    f[83] = MUL_F(FRAC_CONST(-0.9238795325112866), f[81]);
    f[84] = MUL_F(FRAC_CONST(-0.5411961001461967), f[77]);
    f[85] = f[82] + f[83];
    f[86] = f[84] - f[83];
    f[87] = f[80] - f[86];
    f[88] = f[80] + f[86];
    f[89] = f[79] - f[85];
    f[90] = f[79] + f[85];
    f[91] = MUL_F(FRAC_CONST(0.7071067811865476), f[74]);
    f[92] = f[29] - f[91];
    f[93] = f[29] + f[91];
    f[94] = f[76] + f[72];
    f[95] = MUL_C(COEF_CONST(1.3065629648763766), f[76]);
    f[96] = MUL_F(FRAC_CONST(-0.9238795325112866), f[94]);
    f[97] = MUL_F(FRAC_CONST(-0.5411961001461967), f[72]);
    f[98] = f[95] + f[96];
    f[99] = f[97] - f[96];
    f[100] = f[93] - f[99];
    f[101] = f[93] + f[99];
    f[102] = f[92] - f[98];
    f[103] = f[92] + f[98];
    f[104] = f[101] + f[88];
    f[105] = MUL_F(FRAC_CONST(-0.8971675863426361), f[101]);
    f[106] = MUL_F(FRAC_CONST(0.9951847266721968), f[104]);
    f[107] = MUL_C(COEF_CONST(1.0932018670017576), f[88]);
    f[108] = f[105] + f[106];
    f[109] = f[107] - f[106];
    f[110] = f[90] - f[103];
    f[111] = MUL_F(FRAC_CONST(-0.6666556584777466), f[103]);
    f[112] = MUL_F(FRAC_CONST(0.9569403357322089), f[110]);
    f[113] = MUL_C(COEF_CONST(1.2472250129866713), f[90]);
    f[114] = f[112] - f[111];
    f[115] = f[113] - f[112];
    f[116] = f[102] + f[89];
    f[117] = MUL_F(FRAC_CONST(-0.4105245275223571), f[102]);
    f[118] = MUL_F(FRAC_CONST(0.8819212643483549), f[116]);
    f[119] = MUL_C(COEF_CONST(1.3533180011743529), f[89]);
    f[120] = f[117] + f[118];
    f[121] = f[119] - f[118];
    f[122] = f[87] - f[100];
    f[123] = MUL_F(FRAC_CONST(-0.1386171691990915), f[100]);
    f[124] = MUL_F(FRAC_CONST(0.7730104533627370), f[122]);
    f[125] = MUL_C(COEF_CONST(1.4074037375263826), f[87]);
    f[126] = f[124] - f[123];
    f[127] = f[125] - f[124];
    f[128] = f[65] - f[108];
    f[129] = f[65] + f[108];
    f[130] = f[67] - f[114];
    f[131] = f[67] + f[114];
    f[132] = f[69] - f[120];
    f[133] = f[69] + f[120];
    f[134] = f[71] - f[126];
    f[135] = f[71] + f[126];
    f[136] = f[70] - f[127];
    f[137] = f[70] + f[127];
    f[138] = f[68] - f[121];
    f[139] = f[68] + f[121];
    f[140] = f[66] - f[115];
    f[141] = f[66] + f[115];
    f[142] = f[64] - f[109];
    f[143] = f[64] + f[109];
    f[144] = f[0] + f[30];
    f[145] = MUL_C(COEF_CONST(1.0478631305325901), f[0]);
    f[146] = MUL_F(FRAC_CONST(-0.9987954562051724), f[144]);
    f[147] = MUL_F(FRAC_CONST(-0.9497277818777548), f[30]);
    f[148] = f[145] + f[146];
    f[149] = f[147] - f[146];
    f[150] = f[4] + f[26];
    f[151] = MUL_F(FRAC_CONST(1.2130114330978077), f[4]);
    f[152] = MUL_F(FRAC_CONST(-0.9700312531945440), f[150]);
    f[153] = MUL_F(FRAC_CONST(-0.7270510732912803), f[26]);
    f[154] = f[151] + f[152];
    f[155] = f[153] - f[152];
    f[156] = f[8] + f[22];
    f[157] = MUL_C(COEF_CONST(1.3315443865537255), f[8]);
    f[158] = MUL_F(FRAC_CONST(-0.9039892931234433), f[156]);
    f[159] = MUL_F(FRAC_CONST(-0.4764341996931612), f[22]);
    f[160] = f[157] + f[158];
    f[161] = f[159] - f[158];
    f[162] = f[12] + f[18];
    f[163] = MUL_C(COEF_CONST(1.3989068359730781), f[12]);
    f[164] = MUL_F(FRAC_CONST(-0.8032075314806453), f[162]);
    f[165] = MUL_F(FRAC_CONST(-0.2075082269882124), f[18]);
    f[166] = f[163] + f[164];
    f[167] = f[165] - f[164];
    f[168] = f[16] + f[14];
    f[169] = MUL_C(COEF_CONST(1.4125100802019777), f[16]);
    f[170] = MUL_F(FRAC_CONST(-0.6715589548470187), f[168]);
    f[171] = MUL_F(FRAC_CONST(0.0693921705079402), f[14]);
    f[172] = f[169] + f[170];
    f[173] = f[171] - f[170];
    f[174] = f[20] + f[10];
    f[175] = MUL_C(COEF_CONST(1.3718313541934939), f[20]);
    f[176] = MUL_F(FRAC_CONST(-0.5141027441932219), f[174]);
    f[177] = MUL_F(FRAC_CONST(0.3436258658070501), f[10]);
    f[178] = f[175] + f[176];
    f[179] = f[177] - f[176];
    f[180] = f[24] + f[6];
    f[181] = MUL_C(COEF_CONST(1.2784339185752409), f[24]);
    f[182] = MUL_F(FRAC_CONST(-0.3368898533922200), f[180]);
    f[183] = MUL_F(FRAC_CONST(0.6046542117908008), f[6]);
    f[184] = f[181] + f[182];
    f[185] = f[183] - f[182];
    f[186] = f[28] + f[2];
    f[187] = MUL_C(COEF_CONST(1.1359069844201433), f[28]);
    f[188] = MUL_F(FRAC_CONST(-0.1467304744553624), f[186]);
    f[189] = MUL_F(FRAC_CONST(0.8424460355094185), f[2]);
    f[190] = f[187] + f[188];
    f[191] = f[189] - f[188];
    f[192] = f[149] - f[173];
    f[193] = f[149] + f[173];
    f[194] = f[148] - f[172];
    f[195] = f[148] + f[172];
    f[196] = f[155] - f[179];
    f[197] = f[155] + f[179];
    f[198] = f[154] - f[178];
    f[199] = f[154] + f[178];
    f[200] = f[161] - f[185];
    f[201] = f[161] + f[185];
    f[202] = f[160] - f[184];
    f[203] = f[160] + f[184];
    f[204] = f[167] - f[191];
    f[205] = f[167] + f[191];
    f[206] = f[166] - f[190];
    f[207] = f[166] + f[190];
    f[208] = f[192] + f[194];
    f[209] = MUL_C(COEF_CONST(1.1758756024193588), f[192]);
    f[210] = MUL_F(FRAC_CONST(-0.9807852804032304), f[208]);
    f[211] = MUL_F(FRAC_CONST(-0.7856949583871021), f[194]);
    f[212] = f[209] + f[210];
    f[213] = f[211] - f[210];
    f[214] = f[196] + f[198];
    f[215] = MUL_C(COEF_CONST(1.3870398453221475), f[196]);
    f[216] = MUL_F(FRAC_CONST(-0.5555702330196022), f[214]);
    f[217] = MUL_F(FRAC_CONST(0.2758993792829431), f[198]);
    f[218] = f[215] + f[216];
    f[219] = f[217] - f[216];
    f[220] = f[200] + f[202];
    f[221] = MUL_F(FRAC_CONST(0.7856949583871022), f[200]);
    f[222] = MUL_F(FRAC_CONST(0.1950903220161283), f[220]);
    f[223] = MUL_C(COEF_CONST(1.1758756024193586), f[202]);
    f[224] = f[221] + f[222];
    f[225] = f[223] - f[222];
    f[226] = f[204] + f[206];
    f[227] = MUL_F(FRAC_CONST(-0.2758993792829430), f[204]);
    f[228] = MUL_F(FRAC_CONST(0.8314696123025452), f[226]);
    f[229] = MUL_C(COEF_CONST(1.3870398453221475), f[206]);
    f[230] = f[227] + f[228];
    f[231] = f[229] - f[228];
    f[232] = f[193] - f[201];
    f[233] = f[193] + f[201];
    f[234] = f[195] - f[203];
    f[235] = f[195] + f[203];
    f[236] = f[197] - f[205];
    f[237] = f[197] + f[205];
    f[238] = f[199] - f[207];
    f[239] = f[199] + f[207];
    f[240] = f[213] - f[225];
    f[241] = f[213] + f[225];
    f[242] = f[212] - f[224];
    f[243] = f[212] + f[224];
    f[244] = f[219] - f[231];
    f[245] = f[219] + f[231];
    f[246] = f[218] - f[230];
    f[247] = f[218] + f[230];
    f[248] = f[232] + f[234];
    f[249] = MUL_C(COEF_CONST(1.3065629648763766), f[232]);
    f[250] = MUL_F(FRAC_CONST(-0.9238795325112866), f[248]);
    f[251] = MUL_F(FRAC_CONST(-0.5411961001461967), f[234]);
    f[252] = f[249] + f[250];
    f[253] = f[251] - f[250];
    f[254] = f[236] + f[238];
    f[255] = MUL_F(FRAC_CONST(0.5411961001461969), f[236]);
    f[256] = MUL_F(FRAC_CONST(0.3826834323650898), f[254]);
    f[257] = MUL_C(COEF_CONST(1.3065629648763766), f[238]);
    f[258] = f[255] + f[256];
    f[259] = f[257] - f[256];
    f[260] = f[240] + f[242];
    f[261] = MUL_C(COEF_CONST(1.3065629648763766), f[240]);
    f[262] = MUL_F(FRAC_CONST(-0.9238795325112866), f[260]);
    f[263] = MUL_F(FRAC_CONST(-0.5411961001461967), f[242]);
    f[264] = f[261] + f[262];
    f[265] = f[263] - f[262];
    f[266] = f[244] + f[246];
    f[267] = MUL_F(FRAC_CONST(0.5411961001461969), f[244]);
    f[268] = MUL_F(FRAC_CONST(0.3826834323650898), f[266]);
    f[269] = MUL_C(COEF_CONST(1.3065629648763766), f[246]);
    f[270] = f[267] + f[268];
    f[271] = f[269] - f[268];
    f[272] = f[233] - f[237];
    f[273] = f[233] + f[237];
    f[274] = f[235] - f[239];
    f[275] = f[235] + f[239];
    f[276] = f[253] - f[259];
    f[277] = f[253] + f[259];
    f[278] = f[252] - f[258];
    f[279] = f[252] + f[258];
    f[280] = f[241] - f[245];
    f[281] = f[241] + f[245];
    f[282] = f[243] - f[247];
    f[283] = f[243] + f[247];
    f[284] = f[265] - f[271];
    f[285] = f[265] + f[271];
    f[286] = f[264] - f[270];
    f[287] = f[264] + f[270];
    f[288] = f[272] - f[274];
    f[289] = f[272] + f[274];
    f[290] = MUL_F(FRAC_CONST(0.7071067811865474), f[288]);
    f[291] = MUL_F(FRAC_CONST(0.7071067811865474), f[289]);
    f[292] = f[276] - f[278];
    f[293] = f[276] + f[278];
    f[294] = MUL_F(FRAC_CONST(0.7071067811865474), f[292]);
    f[295] = MUL_F(FRAC_CONST(0.7071067811865474), f[293]);
    f[296] = f[280] - f[282];
    f[297] = f[280] + f[282];
    f[298] = MUL_F(FRAC_CONST(0.7071067811865474), f[296]);
    f[299] = MUL_F(FRAC_CONST(0.7071067811865474), f[297]);
    f[300] = f[284] - f[286];
    f[301] = f[284] + f[286];
    f[302] = MUL_F(FRAC_CONST(0.7071067811865474), f[300]);
    f[303] = MUL_F(FRAC_CONST(0.7071067811865474), f[301]);
    f[304] = f[129] - f[273];
    f[305] = f[129] + f[273];
    f[306] = f[131] - f[281];
    f[307] = f[131] + f[281];
    f[308] = f[133] - f[285];
    f[309] = f[133] + f[285];
    f[310] = f[135] - f[277];
    f[311] = f[135] + f[277];
    f[312] = f[137] - f[295];
    f[313] = f[137] + f[295];
    f[314] = f[139] - f[303];
    f[315] = f[139] + f[303];
    f[316] = f[141] - f[299];
    f[317] = f[141] + f[299];
    f[318] = f[143] - f[291];
    f[319] = f[143] + f[291];
    f[320] = f[142] - f[290];
    f[321] = f[142] + f[290];
    f[322] = f[140] - f[298];
    f[323] = f[140] + f[298];
    f[324] = f[138] - f[302];
    f[325] = f[138] + f[302];
    f[326] = f[136] - f[294];
    f[327] = f[136] + f[294];
    f[328] = f[134] - f[279];
    f[329] = f[134] + f[279];
    f[330] = f[132] - f[287];
    f[331] = f[132] + f[287];
    f[332] = f[130] - f[283];
    f[333] = f[130] + f[283];
    f[334] = f[128] - f[275];
    f[335] = f[128] + f[275];
    y[31] = MUL_F(FRAC_CONST(0.5001506360206510), f[305]);
    y[30] = MUL_F(FRAC_CONST(0.5013584524464084), f[307]);
    y[29] = MUL_F(FRAC_CONST(0.5037887256810443), f[309]);
    y[28] = MUL_F(FRAC_CONST(0.5074711720725553), f[311]);
    y[27] = MUL_F(FRAC_CONST(0.5124514794082247), f[313]);
    y[26] = MUL_F(FRAC_CONST(0.5187927131053328), f[315]);
    y[25] = MUL_F(FRAC_CONST(0.5265773151542700), f[317]);
    y[24] = MUL_F(FRAC_CONST(0.5359098169079920), f[319]);
    y[23] = MUL_F(FRAC_CONST(0.5469204379855088), f[321]);
    y[22] = MUL_F(FRAC_CONST(0.5597698129470802), f[323]);
    y[21] = MUL_F(FRAC_CONST(0.5746551840326600), f[325]);
    y[20] = MUL_F(FRAC_CONST(0.5918185358574165), f[327]);
    y[19] = MUL_F(FRAC_CONST(0.6115573478825099), f[329]);
    y[18] = MUL_F(FRAC_CONST(0.6342389366884031), f[331]);
    y[17] = MUL_F(FRAC_CONST(0.6603198078137061), f[333]);
    y[16] = MUL_F(FRAC_CONST(0.6903721282002123), f[335]);
    y[15] = MUL_F(FRAC_CONST(0.7251205223771985), f[334]);
    y[14] = MUL_F(FRAC_CONST(0.7654941649730891), f[332]);
    y[13] = MUL_F(FRAC_CONST(0.8127020908144905), f[330]);
    y[12] = MUL_F(FRAC_CONST(0.8683447152233481), f[328]);
    y[11] = MUL_F(FRAC_CONST(0.9345835970364075), f[326]);
    y[10] = MUL_C(COEF_CONST(1.0144082649970547), f[324]);
    y[9] = MUL_C(COEF_CONST(1.1120716205797176), f[322]);
    y[8] = MUL_C(COEF_CONST(1.2338327379765710), f[320]);
    y[7] = MUL_C(COEF_CONST(1.3892939586328277), f[318]);
    y[6] = MUL_C(COEF_CONST(1.5939722833856311), f[316]);
    y[5] = MUL_C(COEF_CONST(1.8746759800084078), f[314]);
    y[4] = MUL_C(COEF_CONST(2.2820500680051619), f[312]);
    y[3] = MUL_C(COEF_CONST(2.9246284281582162), f[310]);
    y[2] = MUL_C(COEF_CONST(4.0846110781292477), f[308]);
    y[1] = MUL_C(COEF_CONST(6.7967507116736332), f[306]);
    y[0] = MUL_R(REAL_CONST(20.3738781672314530), f[304]);
    if(f) {
        free(f);
        f = NULL;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void DCT4_32(int32_t* y, int32_t* x) {
    // printf(ANSI_ESC_YELLOW "dct4_32" ANSI_ESC_WHITE "\n");
    int32_t* f = (int32_t*)faad_malloc(397 * sizeof(int32_t));
    f[0] = x[15] - x[16];
    f[1] = x[15] + x[16];
    f[2] = MUL_F(FRAC_CONST(0.7071067811865476), f[1]);
    f[3] = MUL_F(FRAC_CONST(0.7071067811865476), f[0]);
    f[4] = x[8] - x[23];
    f[5] = x[8] + x[23];
    f[6] = MUL_F(FRAC_CONST(0.7071067811865476), f[5]);
    f[7] = MUL_F(FRAC_CONST(0.7071067811865476), f[4]);
    f[8] = x[12] - x[19];
    f[9] = x[12] + x[19];
    f[10] = MUL_F(FRAC_CONST(0.7071067811865476), f[9]);
    f[11] = MUL_F(FRAC_CONST(0.7071067811865476), f[8]);
    f[12] = x[11] - x[20];
    f[13] = x[11] + x[20];
    f[14] = MUL_F(FRAC_CONST(0.7071067811865476), f[13]);
    f[15] = MUL_F(FRAC_CONST(0.7071067811865476), f[12]);
    f[16] = x[14] - x[17];
    f[17] = x[14] + x[17];
    f[18] = MUL_F(FRAC_CONST(0.7071067811865476), f[17]);
    f[19] = MUL_F(FRAC_CONST(0.7071067811865476), f[16]);
    f[20] = x[9] - x[22];
    f[21] = x[9] + x[22];
    f[22] = MUL_F(FRAC_CONST(0.7071067811865476), f[21]);
    f[23] = MUL_F(FRAC_CONST(0.7071067811865476), f[20]);
    f[24] = x[13] - x[18];
    f[25] = x[13] + x[18];
    f[26] = MUL_F(FRAC_CONST(0.7071067811865476), f[25]);
    f[27] = MUL_F(FRAC_CONST(0.7071067811865476), f[24]);
    f[28] = x[10] - x[21];
    f[29] = x[10] + x[21];
    f[30] = MUL_F(FRAC_CONST(0.7071067811865476), f[29]);
    f[31] = MUL_F(FRAC_CONST(0.7071067811865476), f[28]);
    f[32] = x[0] - f[2];
    f[33] = x[0] + f[2];
    f[34] = x[31] - f[3];
    f[35] = x[31] + f[3];
    f[36] = x[7] - f[6];
    f[37] = x[7] + f[6];
    f[38] = x[24] - f[7];
    f[39] = x[24] + f[7];
    f[40] = x[3] - f[10];
    f[41] = x[3] + f[10];
    f[42] = x[28] - f[11];
    f[43] = x[28] + f[11];
    f[44] = x[4] - f[14];
    f[45] = x[4] + f[14];
    f[46] = x[27] - f[15];
    f[47] = x[27] + f[15];
    f[48] = x[1] - f[18];
    f[49] = x[1] + f[18];
    f[50] = x[30] - f[19];
    f[51] = x[30] + f[19];
    f[52] = x[6] - f[22];
    f[53] = x[6] + f[22];
    f[54] = x[25] - f[23];
    f[55] = x[25] + f[23];
    f[56] = x[2] - f[26];
    f[57] = x[2] + f[26];
    f[58] = x[29] - f[27];
    f[59] = x[29] + f[27];
    f[60] = x[5] - f[30];
    f[61] = x[5] + f[30];
    f[62] = x[26] - f[31];
    f[63] = x[26] + f[31];
    f[64] = f[39] + f[37];
    f[65] = MUL_F(FRAC_CONST(-0.5411961001461969), f[39]);
    f[66] = MUL_F(FRAC_CONST(0.9238795325112867), f[64]);
    f[67] = MUL_C(COEF_CONST(1.3065629648763766), f[37]);
    f[68] = f[65] + f[66];
    f[69] = f[67] - f[66];
    f[70] = f[38] + f[36];
    f[71] = MUL_C(COEF_CONST(1.3065629648763770), f[38]);
    f[72] = MUL_F(FRAC_CONST(-0.3826834323650904), f[70]);
    f[73] = MUL_F(FRAC_CONST(0.5411961001461961), f[36]);
    f[74] = f[71] + f[72];
    f[75] = f[73] - f[72];
    f[76] = f[47] + f[45];
    f[77] = MUL_F(FRAC_CONST(-0.5411961001461969), f[47]);
    f[78] = MUL_F(FRAC_CONST(0.9238795325112867), f[76]);
    f[79] = MUL_C(COEF_CONST(1.3065629648763766), f[45]);
    f[80] = f[77] + f[78];
    f[81] = f[79] - f[78];
    f[82] = f[46] + f[44];
    f[83] = MUL_C(COEF_CONST(1.3065629648763770), f[46]);
    f[84] = MUL_F(FRAC_CONST(-0.3826834323650904), f[82]);
    f[85] = MUL_F(FRAC_CONST(0.5411961001461961), f[44]);
    f[86] = f[83] + f[84];
    f[87] = f[85] - f[84];
    f[88] = f[55] + f[53];
    f[89] = MUL_F(FRAC_CONST(-0.5411961001461969), f[55]);
    f[90] = MUL_F(FRAC_CONST(0.9238795325112867), f[88]);
    f[91] = MUL_C(COEF_CONST(1.3065629648763766), f[53]);
    f[92] = f[89] + f[90];
    f[93] = f[91] - f[90];
    f[94] = f[54] + f[52];
    f[95] = MUL_C(COEF_CONST(1.3065629648763770), f[54]);
    f[96] = MUL_F(FRAC_CONST(-0.3826834323650904), f[94]);
    f[97] = MUL_F(FRAC_CONST(0.5411961001461961), f[52]);
    f[98] = f[95] + f[96];
    f[99] = f[97] - f[96];
    f[100] = f[63] + f[61];
    f[101] = MUL_F(FRAC_CONST(-0.5411961001461969), f[63]);
    f[102] = MUL_F(FRAC_CONST(0.9238795325112867), f[100]);
    f[103] = MUL_C(COEF_CONST(1.3065629648763766), f[61]);
    f[104] = f[101] + f[102];
    f[105] = f[103] - f[102];
    f[106] = f[62] + f[60];
    f[107] = MUL_C(COEF_CONST(1.3065629648763770), f[62]);
    f[108] = MUL_F(FRAC_CONST(-0.3826834323650904), f[106]);
    f[109] = MUL_F(FRAC_CONST(0.5411961001461961), f[60]);
    f[110] = f[107] + f[108];
    f[111] = f[109] - f[108];
    f[112] = f[33] - f[68];
    f[113] = f[33] + f[68];
    f[114] = f[35] - f[69];
    f[115] = f[35] + f[69];
    f[116] = f[32] - f[74];
    f[117] = f[32] + f[74];
    f[118] = f[34] - f[75];
    f[119] = f[34] + f[75];
    f[120] = f[41] - f[80];
    f[121] = f[41] + f[80];
    f[122] = f[43] - f[81];
    f[123] = f[43] + f[81];
    f[124] = f[40] - f[86];
    f[125] = f[40] + f[86];
    f[126] = f[42] - f[87];
    f[127] = f[42] + f[87];
    f[128] = f[49] - f[92];
    f[129] = f[49] + f[92];
    f[130] = f[51] - f[93];
    f[131] = f[51] + f[93];
    f[132] = f[48] - f[98];
    f[133] = f[48] + f[98];
    f[134] = f[50] - f[99];
    f[135] = f[50] + f[99];
    f[136] = f[57] - f[104];
    f[137] = f[57] + f[104];
    f[138] = f[59] - f[105];
    f[139] = f[59] + f[105];
    f[140] = f[56] - f[110];
    f[141] = f[56] + f[110];
    f[142] = f[58] - f[111];
    f[143] = f[58] + f[111];
    f[144] = f[123] + f[121];
    f[145] = MUL_F(FRAC_CONST(-0.7856949583871021), f[123]);
    f[146] = MUL_F(FRAC_CONST(0.9807852804032304), f[144]);
    f[147] = MUL_C(COEF_CONST(1.1758756024193588), f[121]);
    f[148] = f[145] + f[146];
    f[149] = f[147] - f[146];
    f[150] = f[127] + f[125];
    f[151] = MUL_F(FRAC_CONST(0.2758993792829431), f[127]);
    f[152] = MUL_F(FRAC_CONST(0.5555702330196022), f[150]);
    f[153] = MUL_C(COEF_CONST(1.3870398453221475), f[125]);
    f[154] = f[151] + f[152];
    f[155] = f[153] - f[152];
    f[156] = f[122] + f[120];
    f[157] = MUL_C(COEF_CONST(1.1758756024193591), f[122]);
    f[158] = MUL_F(FRAC_CONST(-0.1950903220161287), f[156]);
    f[159] = MUL_F(FRAC_CONST(0.7856949583871016), f[120]);
    f[160] = f[157] + f[158];
    f[161] = f[159] - f[158];
    f[162] = f[126] + f[124];
    f[163] = MUL_C(COEF_CONST(1.3870398453221473), f[126]);
    f[164] = MUL_F(FRAC_CONST(-0.8314696123025455), f[162]);
    f[165] = MUL_F(FRAC_CONST(-0.2758993792829436), f[124]);
    f[166] = f[163] + f[164];
    f[167] = f[165] - f[164];
    f[168] = f[139] + f[137];
    f[169] = MUL_F(FRAC_CONST(-0.7856949583871021), f[139]);
    f[170] = MUL_F(FRAC_CONST(0.9807852804032304), f[168]);
    f[171] = MUL_C(COEF_CONST(1.1758756024193588), f[137]);
    f[172] = f[169] + f[170];
    f[173] = f[171] - f[170];
    f[174] = f[143] + f[141];
    f[175] = MUL_F(FRAC_CONST(0.2758993792829431), f[143]);
    f[176] = MUL_F(FRAC_CONST(0.5555702330196022), f[174]);
    f[177] = MUL_C(COEF_CONST(1.3870398453221475), f[141]);
    f[178] = f[175] + f[176];
    f[179] = f[177] - f[176];
    f[180] = f[138] + f[136];
    f[181] = MUL_C(COEF_CONST(1.1758756024193591), f[138]);
    f[182] = MUL_F(FRAC_CONST(-0.1950903220161287), f[180]);
    f[183] = MUL_F(FRAC_CONST(0.7856949583871016), f[136]);
    f[184] = f[181] + f[182];
    f[185] = f[183] - f[182];
    f[186] = f[142] + f[140];
    f[187] = MUL_C(COEF_CONST(1.3870398453221473), f[142]);
    f[188] = MUL_F(FRAC_CONST(-0.8314696123025455), f[186]);
    f[189] = MUL_F(FRAC_CONST(-0.2758993792829436), f[140]);
    f[190] = f[187] + f[188];
    f[191] = f[189] - f[188];
    f[192] = f[113] - f[148];
    f[193] = f[113] + f[148];
    f[194] = f[115] - f[149];
    f[195] = f[115] + f[149];
    f[196] = f[117] - f[154];
    f[197] = f[117] + f[154];
    f[198] = f[119] - f[155];
    f[199] = f[119] + f[155];
    f[200] = f[112] - f[160];
    f[201] = f[112] + f[160];
    f[202] = f[114] - f[161];
    f[203] = f[114] + f[161];
    f[204] = f[116] - f[166];
    f[205] = f[116] + f[166];
    f[206] = f[118] - f[167];
    f[207] = f[118] + f[167];
    f[208] = f[129] - f[172];
    f[209] = f[129] + f[172];
    f[210] = f[131] - f[173];
    f[211] = f[131] + f[173];
    f[212] = f[133] - f[178];
    f[213] = f[133] + f[178];
    f[214] = f[135] - f[179];
    f[215] = f[135] + f[179];
    f[216] = f[128] - f[184];
    f[217] = f[128] + f[184];
    f[218] = f[130] - f[185];
    f[219] = f[130] + f[185];
    f[220] = f[132] - f[190];
    f[221] = f[132] + f[190];
    f[222] = f[134] - f[191];
    f[223] = f[134] + f[191];
    f[224] = f[211] + f[209];
    f[225] = MUL_F(FRAC_CONST(-0.8971675863426361), f[211]);
    f[226] = MUL_F(FRAC_CONST(0.9951847266721968), f[224]);
    f[227] = MUL_C(COEF_CONST(1.0932018670017576), f[209]);
    f[228] = f[225] + f[226];
    f[229] = f[227] - f[226];
    f[230] = f[215] + f[213];
    f[231] = MUL_F(FRAC_CONST(-0.4105245275223571), f[215]);
    f[232] = MUL_F(FRAC_CONST(0.8819212643483549), f[230]);
    f[233] = MUL_C(COEF_CONST(1.3533180011743529), f[213]);
    f[234] = f[231] + f[232];
    f[235] = f[233] - f[232];
    f[236] = f[219] + f[217];
    f[237] = MUL_F(FRAC_CONST(0.1386171691990915), f[219]);
    f[238] = MUL_F(FRAC_CONST(0.6343932841636455), f[236]);
    f[239] = MUL_C(COEF_CONST(1.4074037375263826), f[217]);
    f[240] = f[237] + f[238];
    f[241] = f[239] - f[238];
    f[242] = f[223] + f[221];
    f[243] = MUL_F(FRAC_CONST(0.6666556584777466), f[223]);
    f[244] = MUL_F(FRAC_CONST(0.2902846772544623), f[242]);
    f[245] = MUL_C(COEF_CONST(1.2472250129866711), f[221]);
    f[246] = f[243] + f[244];
    f[247] = f[245] - f[244];
    f[248] = f[210] + f[208];
    f[249] = MUL_C(COEF_CONST(1.0932018670017574), f[210]);
    f[250] = MUL_F(FRAC_CONST(-0.0980171403295605), f[248]);
    f[251] = MUL_F(FRAC_CONST(0.8971675863426364), f[208]);
    f[252] = f[249] + f[250];
    f[253] = f[251] - f[250];
    f[254] = f[214] + f[212];
    f[255] = MUL_C(COEF_CONST(1.3533180011743529), f[214]);
    f[256] = MUL_F(FRAC_CONST(-0.4713967368259979), f[254]);
    f[257] = MUL_F(FRAC_CONST(0.4105245275223569), f[212]);
    f[258] = f[255] + f[256];
    f[259] = f[257] - f[256];
    f[260] = f[218] + f[216];
    f[261] = MUL_C(COEF_CONST(1.4074037375263826), f[218]);
    f[262] = MUL_F(FRAC_CONST(-0.7730104533627369), f[260]);
    f[263] = MUL_F(FRAC_CONST(-0.1386171691990913), f[216]);
    f[264] = f[261] + f[262];
    f[265] = f[263] - f[262];
    f[266] = f[222] + f[220];
    f[267] = MUL_C(COEF_CONST(1.2472250129866711), f[222]);
    f[268] = MUL_F(FRAC_CONST(-0.9569403357322089), f[266]);
    f[269] = MUL_F(FRAC_CONST(-0.6666556584777469), f[220]);
    f[270] = f[267] + f[268];
    f[271] = f[269] - f[268];
    f[272] = f[193] - f[228];
    f[273] = f[193] + f[228];
    f[274] = f[195] - f[229];
    f[275] = f[195] + f[229];
    f[276] = f[197] - f[234];
    f[277] = f[197] + f[234];
    f[278] = f[199] - f[235];
    f[279] = f[199] + f[235];
    f[280] = f[201] - f[240];
    f[281] = f[201] + f[240];
    f[282] = f[203] - f[241];
    f[283] = f[203] + f[241];
    f[284] = f[205] - f[246];
    f[285] = f[205] + f[246];
    f[286] = f[207] - f[247];
    f[287] = f[207] + f[247];
    f[288] = f[192] - f[252];
    f[289] = f[192] + f[252];
    f[290] = f[194] - f[253];
    f[291] = f[194] + f[253];
    f[292] = f[196] - f[258];
    f[293] = f[196] + f[258];
    f[294] = f[198] - f[259];
    f[295] = f[198] + f[259];
    f[296] = f[200] - f[264];
    f[297] = f[200] + f[264];
    f[298] = f[202] - f[265];
    f[299] = f[202] + f[265];
    f[300] = f[204] - f[270];
    f[301] = f[204] + f[270];
    f[302] = f[206] - f[271];
    f[303] = f[206] + f[271];
    f[304] = f[275] + f[273];
    f[305] = MUL_F(FRAC_CONST(-0.9751575901732920), f[275]);
    f[306] = MUL_F(FRAC_CONST(0.9996988186962043), f[304]);
    f[307] = MUL_C(COEF_CONST(1.0242400472191164), f[273]);
    y[0] = f[305] + f[306];
    y[31] = f[307] - f[306];
    f[310] = f[279] + f[277];
    f[311] = MUL_F(FRAC_CONST(-0.8700688593994936), f[279]);
    f[312] = MUL_F(FRAC_CONST(0.9924795345987100), f[310]);
    f[313] = MUL_C(COEF_CONST(1.1148902097979263), f[277]);
    y[2] = f[311] + f[312];
    y[29] = f[313] - f[312];
    f[316] = f[283] + f[281];
    f[317] = MUL_F(FRAC_CONST(-0.7566008898816587), f[283]);
    f[318] = MUL_F(FRAC_CONST(0.9757021300385286), f[316]);
    f[319] = MUL_C(COEF_CONST(1.1948033701953984), f[281]);
    y[4] = f[317] + f[318];
    y[27] = f[319] - f[318];
    f[322] = f[287] + f[285];
    f[323] = MUL_F(FRAC_CONST(-0.6358464401941451), f[287]);
    f[324] = MUL_F(FRAC_CONST(0.9495281805930367), f[322]);
    f[325] = MUL_C(COEF_CONST(1.2632099209919283), f[285]);
    y[6] = f[323] + f[324];
    y[25] = f[325] - f[324];
    f[328] = f[291] + f[289];
    f[329] = MUL_F(FRAC_CONST(-0.5089684416985408), f[291]);
    f[330] = MUL_F(FRAC_CONST(0.9142097557035307), f[328]);
    f[331] = MUL_C(COEF_CONST(1.3194510697085207), f[289]);
    y[8] = f[329] + f[330];
    y[23] = f[331] - f[330];
    f[334] = f[295] + f[293];
    f[335] = MUL_F(FRAC_CONST(-0.3771887988789273), f[295]);
    f[336] = MUL_F(FRAC_CONST(0.8700869911087114), f[334]);
    f[337] = MUL_C(COEF_CONST(1.3629851833384954), f[293]);
    y[10] = f[335] + f[336];
    y[21] = f[337] - f[336];
    f[340] = f[299] + f[297];
    f[341] = MUL_F(FRAC_CONST(-0.2417766217337384), f[299]);
    f[342] = MUL_F(FRAC_CONST(0.8175848131515837), f[340]);
    f[343] = MUL_C(COEF_CONST(1.3933930045694289), f[297]);
    y[12] = f[341] + f[342];
    y[19] = f[343] - f[342];
    f[346] = f[303] + f[301];
    f[347] = MUL_F(FRAC_CONST(-0.1040360035527077), f[303]);
    f[348] = MUL_F(FRAC_CONST(0.7572088465064845), f[346]);
    f[349] = MUL_C(COEF_CONST(1.4103816894602612), f[301]);
    y[14] = f[347] + f[348];
    y[17] = f[349] - f[348];
    f[352] = f[274] + f[272];
    f[353] = MUL_F(FRAC_CONST(0.0347065382144002), f[274]);
    f[354] = MUL_F(FRAC_CONST(0.6895405447370668), f[352]);
    f[355] = MUL_C(COEF_CONST(1.4137876276885337), f[272]);
    y[16] = f[353] + f[354];
    y[15] = f[355] - f[354];
    f[358] = f[278] + f[276];
    f[359] = MUL_F(FRAC_CONST(0.1731148370459795), f[278]);
    f[360] = MUL_F(FRAC_CONST(0.6152315905806268), f[358]);
    f[361] = MUL_C(COEF_CONST(1.4035780182072330), f[276]);
    y[18] = f[359] + f[360];
    y[13] = f[361] - f[360];
    f[364] = f[282] + f[280];
    f[365] = MUL_F(FRAC_CONST(0.3098559453626100), f[282]);
    f[366] = MUL_F(FRAC_CONST(0.5349976198870972), f[364]);
    f[367] = MUL_C(COEF_CONST(1.3798511851368043), f[280]);
    y[20] = f[365] + f[366];
    y[11] = f[367] - f[366];
    f[370] = f[286] + f[284];
    f[371] = MUL_F(FRAC_CONST(0.4436129715409088), f[286]);
    f[372] = MUL_F(FRAC_CONST(0.4496113296546065), f[370]);
    f[373] = MUL_C(COEF_CONST(1.3428356308501219), f[284]);
    y[22] = f[371] + f[372];
    y[9] = f[373] - f[372];
    f[376] = f[290] + f[288];
    f[377] = MUL_F(FRAC_CONST(0.5730977622997509), f[290]);
    f[378] = MUL_F(FRAC_CONST(0.3598950365349881), f[376]);
    f[379] = MUL_C(COEF_CONST(1.2928878353697271), f[288]);
    y[24] = f[377] + f[378];
    y[7] = f[379] - f[378];
    f[382] = f[294] + f[292];
    f[383] = MUL_F(FRAC_CONST(0.6970633083205415), f[294]);
    f[384] = MUL_F(FRAC_CONST(0.2667127574748984), f[382]);
    f[385] = MUL_C(COEF_CONST(1.2304888232703382), f[292]);
    y[26] = f[383] + f[384];
    y[5] = f[385] - f[384];
    f[388] = f[298] + f[296];
    f[389] = MUL_F(FRAC_CONST(0.8143157536286401), f[298]);
    f[390] = MUL_F(FRAC_CONST(0.1709618887603012), f[388]);
    f[391] = MUL_C(COEF_CONST(1.1562395311492424), f[296]);
    y[28] = f[389] + f[390];
    y[3] = f[391] - f[390];
    f[394] = f[302] + f[300];
    f[395] = MUL_F(FRAC_CONST(0.9237258930790228), f[302]);
    f[396] = MUL_F(FRAC_CONST(0.0735645635996674), f[394]);
    f[397] = MUL_C(COEF_CONST(1.0708550202783576), f[300]);
    y[30] = f[395] + f[396];
    y[1] = f[397] - f[396];
    if(f) {
        free(f);
        f = NULL;
    }
}

/* EOF */
