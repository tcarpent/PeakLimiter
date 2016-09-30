/*
Copyright (c) 2016, UMR STMS 9912 - Ircam-Centre Pompidou / CNRS / UPMC
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef __peaklimiter_h__
#define __peaklimiter_h__

enum {
  LIMITER_OK = 0,

  __error_codes_start = -100,

  LIMITER_INVALID_HANDLE,
  LIMITER_INVALID_PARAMETER,

  __error_codes_end
};

#define PEAKLIMITER_ATTACK_DEFAULT_MS      (20.0f)               /* default attack  time in ms */
#define PEAKLIMITER_RELEASE_DEFAULT_MS     (20.0f)              /* default release time in ms */


class PeakLimiter
{

public:
  int  m_attack;
  float         m_attackConst, m_releaseConst;
  float         m_attackMs, m_releaseMs, m_maxAttackMs;
  float         m_threshold;
  int  m_channels, m_maxChannels;
  int  m_sampleRate, m_maxSampleRate;
  float         m_fadedGain;
  float*        m_pMaxBuffer;
  float*        m_pMaxBufferSlow;
  float*        m_pDelayBuffer;
  int  m_maxBufferIndex, m_maxBufferSlowIndex, m_delayBufferIndex;
  int  m_sectionLen, m_nbrMaxBufferSection;
  int  m_maxBufferSectionIndex, m_maxBufferSectionCounter;
  float        m_smoothState;
  float         m_maxMaxBufferSlow, m_maxCurrentSection;
  int m_indexMaxBufferSlow, *m_pIndexMaxInSection;
    
public:

/******************************************************************************
* createLimiter                                                               *
* maxAttackMs:   maximum attack/lookahead time in milliseconds                *
* releaseMs:     release time in milliseconds (90% time constant)             *
* threshold:     limiting threshold                                           *
* maxChannels:   maximum number of channels                                   *
* maxSampleRate: maximum sampling rate in Hz                                  *
* returns:       limiter handle                                               *
******************************************************************************/
PeakLimiter(               float         maxAttackMs, 
                           float         releaseMs, 
                           float         threshold, 
                           int  maxChannels, 
                           int  maxSampleRate);
~PeakLimiter();
/******************************************************************************
* resetLimiter                                                                *
* limiter: limiter handle                                                     *
* returns: error code                                                         *
******************************************************************************/
int resetLimiter();

/******************************************************************************
* destroyLimiter                                                              *
* limiter: limiter handle                                                     *
* returns: error code                                                         *
******************************************************************************/
int destroyLimiter();

/******************************************************************************
* applyLimiter                                                                *
* limiter:  limiter handle                                                    *
* samplesIn:  input buffer containing interleaved samples                *
* samplesOut:  output buffer containing interleaved samples                *
* nSamples: number of samples per channel                                     *
* returns:  error code                                                        *
******************************************************************************/
int applyLimiter_E( 
                 const float*       samplesIn, 
                 float*       samplesOut, 
                 int nSamples);

/******************************************************************************
* applyLimiter                                                                *
* limiter:  limiter handle                                                    *
* samplesIn:  input buffer containing no interleaved samples                *
* samplesOut:  output buffer containing interleaved samples                *
* nSamples: number of samples per channel                                     *
* returns:  error code                                                        *
******************************************************************************/
int applyLimiter(
                 const float**       samplesIn, 
                 float**       samplesOut, 
                 int nSamples);

/******************************************************************************
* applyLimiter                                                                *
* limiter:  limiter handle                                                    *
* samples:  input/output buffer containing no interleaved samples                *
* nSamples: number of samples per channel                                     *
* returns:  error code                                                        *
******************************************************************************/
int applyLimiter_I(
                 float**       samples, 
                 int nSamples);

/******************************************************************************
* applyLimiter                                                                *
* limiter:  limiter handle                                                    *
* samples:  input/output buffer containing interleaved samples                *
* nSamples: number of samples per channel                                     *
* returns:  error code                                                        *
******************************************************************************/
int applyLimiter_E_I(
                 float*       samples, 
                 int nSamples);

/******************************************************************************
* getLimiterDelay                                                             *
* limiter: limiter handle                                                     *
* returns: exact delay caused by the limiter in samples                       *
******************************************************************************/
 int getLimiterDelay();

 int getLimiterSampleRate();

float getLimiterAttack();

float getLimiterRelease();

float getLimiterThreshold();

/******************************************************************************
* getLimiterMaxGainReduction                                                  *
* limiter: limiter handle                                                     *
* returns: maximum gain reduction in last processed block in dB               *
******************************************************************************/
float getLimiterMaxGainReduction();

/******************************************************************************
* setLimiterNChannels                                                         *
* limiter:   limiter handle                                                   *
* nChannels: number of channels ( <= maxChannels specified on create)         *
* returns:   error code                                                       *
******************************************************************************/
int setLimiterNChannels( int nChannels);

/******************************************************************************
* setLimiterSampleRate                                                        *
* limiter:    limiter handle                                                  *
* sampleRate: sampling rate in Hz ( <= maxSampleRate specified on create)     *
* returns:    error code                                                      *
******************************************************************************/
int setLimiterSampleRate( int sampleRate);

/******************************************************************************
* setLimiterAttack                                                            *
* limiter:    limiter handle                                                  *
* attackMs:   attack time in ms ( <= maxAttackMs specified on create)         *
* returns:    error code                                                      *
******************************************************************************/
int setLimiterAttack( float attackMs);

/******************************************************************************
* setLimiterRelease                                                           *
* limiter:    limiter handle                                                  *
* releaseMs:  release time in ms                                              *
* returns:    error code                                                      *
******************************************************************************/
int setLimiterRelease( float releaseMs);

/******************************************************************************
* setLimiterThreshold                                                         *
* limiter:    limiter handle                                                  *
* threshold:  limiter threshold                                               *
* returns:    error code                                                      *
******************************************************************************/
int setLimiterThreshold( float threshold);
};

#endif /* __peaklimiter_h__ */
