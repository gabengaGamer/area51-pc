#include "mss.h"
#include "mp3dec.h"
#include <memory.h>
#include <math.h>

static const float sin30 = 0.5f;
static const float cos30 = 0.866025403f;

static const float dct36t0 = 0.501909918f;
static const float dct36t1 = -0.250238171f;
static const float dct36t2 = -5.731396405f;

extern "C" void __cdecl x86_IMDCT_1x36(F32 *in, S32 sb, F32 *result, F32 *save, F32 *window)
{
    float temp[36];
    float v[18];
    
    for (int i = 17; i > 0; i--) 
    {
        in[i] += in[i-1];
    }
    
    for (int i = 17; i > 0; i -= 4) 
    {
        in[i] += in[i-2];
    }
    
    for (int i = 0; i < 18; i++) 
    {
        v[i] = 0.0f;
    }
    
    for (int i = 0; i < 18; i++) 
    {
        for (int j = 0; j < 18; j++) 
        {
            v[i] += in[j] * cos(PI / 36.0f * (i + 0.5f) * (j + 0.5f));
        }
    }
    
    for (int i = 0; i < 18; i++) 
    {
        temp[i] = v[i] * window[i];
        temp[i+18] = -v[17-i] * window[i+18];
    }
    
    for (int i = 0; i < 18; i++) 
    {
        result[i] = temp[i] + save[i];
        save[i] = temp[i+18];
    }
}

extern "C" void __cdecl x86_IMDCT_3x12(F32 *in, S32 sb, F32 *result, F32 *save)
{
    float output[36] = {0};
    float temp[18] = {0};
    
    for (int block = 0; block < 3; block++) 
    {
        float blockInput[6];
        for (int i = 0; i < 6; i++) 
        {
            blockInput[i] = in[block*6 + i];
        }
        
        blockInput[5] += blockInput[4];
        blockInput[4] += blockInput[3];
        blockInput[3] += blockInput[2];
        blockInput[2] += blockInput[1];
        blockInput[1] += blockInput[0];
        blockInput[5] += blockInput[3];
        blockInput[3] += blockInput[1];
        
        for (int i = 0; i < 6; i++) 
        {
            temp[i] = 0.0f;
            for (int j = 0; j < 6; j++) 
            {
                temp[i] += blockInput[j] * cos(PI / 12.0f * (i + 0.5f) * (j + 0.5f));
            }
        }       
        for (int i = 0; i < 6; i++) {
            output[block*6 + i + 6] += temp[i];
        }
    }
    
    if (sb & 1) 
    {
        for (int i = 0; i < 18; i++) 
        {
            result[i] = output[i] + save[i];
            result[i+18] = -output[i+18] - save[i+18];
        }
    } 
    else 
    {
        for (int i = 0; i < 36; i++) 
        {
            result[i] = output[i] + save[i];
        }
    }
    
    memcpy(save, output + 18, 18 * sizeof(float));
}

extern "C" void __cdecl x86_poly_filter(const F32 *src, const F32 *b, S32 phase, F32 *out1, F32 *out2)
{
    for (int i = 0; i < 16; i++) 
    {
        out1[i] = 0.0f;
        out2[i] = 0.0f;
    }
    
    for (int i = 0; i < 16; i++) 
    {
        for (int j = 0; j < 8; j++)
        {
            int idx = j*2 + (phase & 1);
            float val = src[i*2 + idx];
            
            out1[i] += val * b[j*4];
            out2[i] += val * b[j*4 + 16];
        }
    }
}

extern "C" void __cdecl x86_dewindow_and_write(F32 *u, F32 *dewindow, S32 start, S16 *samples, S32 output_step, S32 div)
{
    int index = start;
    
    float buffer[32];
    
    for (int i = 0; i < 16; i++) 
    {
        buffer[i] = 0.0f;
        
        for (int j = 0; j < 8; j++) 
        {
            buffer[i] += u[i*16 + j*2] * dewindow[j*4 + index];
            buffer[i] += u[i*16 + j*2 + 1] * dewindow[j*4 + index + 4];
        }
        
        int sample = (int)(buffer[i]);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        
        *samples = (S16)sample;
        samples = (S16*)((char*)samples + output_step);
    }
    
    if (div & 1) 
    {
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) 
        {
            sum += u[16*16 + j*8] * dewindow[j*8 + index];
            sum += u[16*16 + j*8 + 8] * dewindow[j*8 + index + 8];
        }
        
        int sample = (int)(sum);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        
        *samples = (S16)sample;
        samples = (S16*)((char*)samples + output_step);
        
        for (int i = 15; i > 0; i--) 
        {
            float sum = 0.0f;
            for (int j = 0; j < 8; j++) 
            {
                sum += u[(i-1)*16 + j*2 + 4] * dewindow[j*4 + index - 4];
                sum -= u[(i-1)*16 + j*2] * dewindow[j*4 + index];
            }
            
            int sample = (int)(sum);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            
            *samples = (S16)sample;
            samples = (S16*)((char*)samples + output_step);
        }
    } 
    else 
    {
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) 
        {
            sum += u[16*16 + j*8 + 4] * dewindow[j*8 + index + 4];
            sum += u[16*16 + j*8 + 12] * dewindow[j*8 + index + 12];
        }
        
        int sample = (int)(sum);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        
        *samples = (S16)sample;
        samples = (S16*)((char*)samples + output_step);
        
        for (int i = 15; i > 0; i--) 
        {
            float sum = 0.0f;
            for (int j = 0; j < 8; j++) 
            {
                sum += u[(i-1)*16 + j*2] * dewindow[j*4 + index];
                sum -= u[(i-1)*16 + j*2 + 4] * dewindow[j*4 + index - 4];
            }
            
            int sample = (int)(sum);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            
            *samples = (S16)sample;
            samples = (S16*)((char*)samples + output_step);
        }
    }
}