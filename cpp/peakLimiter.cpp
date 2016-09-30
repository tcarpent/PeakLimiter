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

#include "peakLimiter.h"

#ifndef max
#define max(a, b)   (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b)   (((a) < (b)) ? (a) : (b))
#endif

/* create limiter */
 PeakLimiter::PeakLimiter(
                           float         maxAttackMsIn,
                           float         releaseMsIn,
                           float         thresholdIn,
                           int  maxChannelsIn,
                           int  maxSampleRateIn
                           )
{

  /* calc m_attack time in samples */
  m_attack = (int)(maxAttackMsIn * maxSampleRateIn / 1000);

  if (m_attack < 1) /* m_attack time is too short */
	  m_attack = 1; 

  /* length of m_pMaxBuffer sections */
  m_sectionLen = (int)sqrt((float)m_attack+1);
  /* sqrt(m_attack+1) leads to the minimum 
     of the number of maximum operators:
     nMaxOp = m_sectionLen + (m_attack+1)/m_sectionLen */

  /* alloc limiter struct */

  m_nbrMaxBufferSection = (m_attack+1)/m_sectionLen;
  if (m_nbrMaxBufferSection*m_sectionLen < (m_attack+1))
    m_nbrMaxBufferSection++; /* create a full section for the last samples */

  /* alloc maximum and delay buffers */
  m_pMaxBuffer   = new float[m_nbrMaxBufferSection * m_sectionLen];
  m_pDelayBuffer   = new float[m_attack * maxChannelsIn];
  m_pMaxBufferSlow   = new float[m_nbrMaxBufferSection];
  m_pIndexMaxInSection = new  int[m_nbrMaxBufferSection];

  if ((m_pMaxBuffer==NULL) || (m_pDelayBuffer==NULL) || (m_pMaxBufferSlow==NULL)) {
    destroyLimiter();
    return;
  }

  /* init parameters & states */
  m_maxBufferIndex = 0;
  m_delayBufferIndex = 0;
  m_maxBufferSlowIndex = 0;
  m_maxBufferSectionIndex = 0;
  m_maxBufferSectionCounter = 0;
  m_maxMaxBufferSlow = 0;
  m_indexMaxBufferSlow = 0;
  m_maxCurrentSection = 0;

  m_attackMs      = maxAttackMsIn;
  m_maxAttackMs   = maxAttackMsIn;
  m_attackConst   = (float)pow(0.1, 1.0 / (m_attack + 1));
  m_releaseConst  = (float)pow(0.1, 1.0 / (m_releaseMs * maxSampleRateIn / 1000 + 1));
  m_threshold     = thresholdIn;
  m_channels      = maxChannelsIn;
  m_maxChannels   = maxChannelsIn;
  m_sampleRate    = maxSampleRateIn;
  m_maxSampleRate = maxSampleRateIn;
    

  m_fadedGain = 1.0f;
  m_smoothState = 1.0;
    
    memset(m_pMaxBuffer,0,sizeof(float)*m_nbrMaxBufferSection * m_sectionLen);
    memset(m_pDelayBuffer,0,sizeof(float)*m_attack * maxChannelsIn);
    memset(m_pMaxBufferSlow,0,sizeof(float)*m_nbrMaxBufferSection);
    memset(m_pIndexMaxInSection,0,sizeof( int)*m_nbrMaxBufferSection);
}

PeakLimiter::~PeakLimiter()
{
    destroyLimiter();
}

/* reset limiter */
int PeakLimiter::resetLimiter()
{
 
    m_maxBufferIndex = 0;
    m_delayBufferIndex = 0;
    m_maxBufferSlowIndex = 0;
    m_maxBufferSectionIndex = 0;
    m_maxBufferSectionCounter = 0;
    m_fadedGain = 1.0f;
    m_smoothState = 1.0;
    m_maxMaxBufferSlow = 0;
    m_indexMaxBufferSlow = 0;
    m_maxCurrentSection = 0;


    memset(m_pMaxBuffer,0,sizeof(float)*m_nbrMaxBufferSection * m_sectionLen);
    memset(m_pDelayBuffer,0,sizeof(float)*m_attack * m_maxChannels);
    memset(m_pMaxBufferSlow,0,sizeof(float)*m_nbrMaxBufferSection);
    memset(m_pIndexMaxInSection,0,sizeof(int)*m_nbrMaxBufferSection);
  
  return LIMITER_OK;
}


/* destroy limiter */
int PeakLimiter::destroyLimiter()
{
    if (m_pMaxBuffer)
    {
        delete [] m_pMaxBuffer;
        m_pMaxBuffer = NULL;
    }
    if (m_pDelayBuffer)
    {
     delete [] m_pDelayBuffer;
        m_pDelayBuffer = NULL;
    }
    if (m_pMaxBufferSlow)
    {
        delete [] m_pMaxBufferSlow;
        m_pMaxBufferSlow = NULL;
    }
    if (m_pIndexMaxInSection)
    {
        delete [] m_pIndexMaxInSection;
        m_pIndexMaxInSection = NULL;
    }
    
    return LIMITER_OK;
}

/* apply limiter */
int PeakLimiter::applyLimiter_E(const float *samplesIn,float *samplesOut, int nSamples)
{
	memcpy(samplesOut,samplesIn,nSamples*sizeof(float));
	return applyLimiter_E_I(samplesOut,nSamples);
}

/* apply limiter */
int PeakLimiter::applyLimiter_E_I(float *samples, int nSamples)
{
   int i, j;
    float tmp, gain, maximum;

    for (i = 0; i < nSamples; i++) {
        /* get maximum absolute sample value of all channels that are greater in absoulte value to m_threshold */
        m_pMaxBuffer[m_maxBufferIndex] = m_threshold;
        for (j = 0; j < m_channels; j++) {
            m_pMaxBuffer[m_maxBufferIndex] = max(m_pMaxBuffer[m_maxBufferIndex], (float)fabs(samples[i * m_channels + j]));
        }

        /* search maximum in the current section */
        if (m_pIndexMaxInSection[m_maxBufferSlowIndex] == m_maxBufferIndex) // if we have just changed the sample containg the old maximum value
        {
            // need to compute the maximum on the whole section 
            m_maxCurrentSection = m_pMaxBuffer[m_maxBufferSectionIndex];
            for (j = 1; j < m_sectionLen; j++) {
                if (m_pMaxBuffer[m_maxBufferSectionIndex + j] > m_maxCurrentSection)
                {
                    m_maxCurrentSection = m_pMaxBuffer[m_maxBufferSectionIndex + j];
                    m_pIndexMaxInSection[m_maxBufferSlowIndex] = m_maxBufferSectionIndex + j;
                }
            }
        }
        else // just need to compare the new value the cthe current maximum value
        {
            if (m_pMaxBuffer[m_maxBufferIndex] > m_maxCurrentSection)
            {
                m_maxCurrentSection = m_pMaxBuffer[m_maxBufferIndex];
                m_pIndexMaxInSection[m_maxBufferSlowIndex] = m_maxBufferIndex;
            }
        }

        // find maximum of slow (downsampled) max buffer
        maximum = m_maxMaxBufferSlow;
        if (m_maxCurrentSection > maximum)
        {
            maximum = m_maxCurrentSection;
        }

        m_maxBufferIndex++;
        m_maxBufferSectionCounter++;

        /* if m_pMaxBuffer section is finished, or end of m_pMaxBuffer is reached,
        store the maximum of this section and open up a new one */
        if ((m_maxBufferSectionCounter >= m_sectionLen) || (m_maxBufferIndex >= m_attack + 1)) {
            m_maxBufferSectionCounter = 0;

            tmp = m_pMaxBufferSlow[m_maxBufferSlowIndex] = m_maxCurrentSection;
            j = 0;
            if (m_indexMaxBufferSlow == m_maxBufferSlowIndex)
            {
                j = 1;
            }
            m_maxBufferSlowIndex++;
            if (m_maxBufferSlowIndex >= m_nbrMaxBufferSection)
            {
                m_maxBufferSlowIndex = 0;
            }
            if (m_indexMaxBufferSlow == m_maxBufferSlowIndex)
            {
                j = 1;
            }
            m_maxCurrentSection = m_pMaxBufferSlow[m_maxBufferSlowIndex];
            m_pMaxBufferSlow[m_maxBufferSlowIndex] = 0.0f;  /* zero out the value representing the new section */

            /* compute the maximum over all the section */
            if (j)
            {
                m_maxMaxBufferSlow = 0;
                for (j = 0; j < m_nbrMaxBufferSection; j++)
                {
                    if (m_pMaxBufferSlow[j] > m_maxMaxBufferSlow)
                    {
                        m_maxMaxBufferSlow = m_pMaxBufferSlow[j];
                        m_indexMaxBufferSlow = j;
                    }
                }
            }
            else
            {
                if (tmp > m_maxMaxBufferSlow)
                {
                    m_maxMaxBufferSlow = tmp;
                    m_indexMaxBufferSlow = m_maxBufferSlowIndex;
                }
            }

            m_maxBufferSectionIndex += m_sectionLen;
        }

        if (m_maxBufferIndex >= (m_attack + 1))
        {
            m_maxBufferIndex = 0;
            m_maxBufferSectionIndex = 0;
        }

        /* needed current gain */
        if (maximum > m_threshold)
        {
            gain = m_threshold / maximum;
        }
        else
        {
            gain = 1;
        }

        /*avoid overshoot */

        if (gain < m_smoothState) {
            m_fadedGain = min(m_fadedGain, (gain - 0.1f * (float)m_smoothState) * 1.11111111f);
        }
        else
        {
            m_fadedGain = gain;
        }


        /* smoothing gain */
        if (m_fadedGain < m_smoothState)
        {
            m_smoothState = m_attackConst * (m_smoothState - m_fadedGain) + m_fadedGain;  /* m_attack */
            /*avoid overshoot */
            if (gain > m_smoothState)
            {
                m_smoothState = gain;
            }
        }
        else
        {
            m_smoothState = m_releaseConst * (m_smoothState - m_fadedGain) + m_fadedGain; /* release */
        }

        /* fill delay line, apply gain */
        for (j = 0; j < m_channels; j++)
        {
            tmp = m_pDelayBuffer[m_delayBufferIndex * m_channels + j];
            m_pDelayBuffer[m_delayBufferIndex * m_channels + j] = samples[i * m_channels + j];

            tmp *= m_smoothState;
            if (tmp > m_threshold) tmp = m_threshold;
            if (tmp < -m_threshold) tmp = -m_threshold;

            samples[i * m_channels + j] = tmp;
        }

        m_delayBufferIndex++;
        if (m_delayBufferIndex >= m_attack)
            m_delayBufferIndex = 0;

    }

    return LIMITER_OK;
}

/* apply limiter */
int PeakLimiter::applyLimiter(const float **samplesIn,float **samplesOut, int nSamples)
{
	int ind;
	for(ind=0;ind<m_channels;ind++)
	{
		memcpy(samplesOut[ind],samplesIn[ind],nSamples*sizeof(float));
	}
	return applyLimiter_I(samplesOut,nSamples);
}


/* apply limiter */
int PeakLimiter::applyLimiter_I( float **samples,int nSamples)
{
   int i, j;
    float tmp, gain, maximum;
    
    for (i = 0; i < nSamples; i++) {
        /* get maximum absolute sample value of all channels that are greater in absoulte value to m_threshold */
        m_pMaxBuffer[m_maxBufferIndex] = m_threshold;
        for (j = 0; j < m_channels; j++) {
            m_pMaxBuffer[m_maxBufferIndex] = max(m_pMaxBuffer[m_maxBufferIndex], (float)fabs(samples[j][i]));
        }
                
        /* search maximum in the current section */
        if (m_pIndexMaxInSection[m_maxBufferSlowIndex] == m_maxBufferIndex) // if we have just changed the sample containg the old maximum value
        {
            // need to compute the maximum on the whole section 
            m_maxCurrentSection = m_pMaxBuffer[m_maxBufferSectionIndex];
            for (j = 1; j < m_sectionLen; j++) {
                if (m_pMaxBuffer[m_maxBufferSectionIndex + j] > m_maxCurrentSection)
                {
                    m_maxCurrentSection = m_pMaxBuffer[m_maxBufferSectionIndex + j];
                    m_pIndexMaxInSection[m_maxBufferSlowIndex] = m_maxBufferSectionIndex+j;
                }
            }
        }
        else // just need to compare the new value the cthe current maximum value
        {
            if (m_pMaxBuffer[m_maxBufferIndex] > m_maxCurrentSection)
            {
                m_maxCurrentSection = m_pMaxBuffer[m_maxBufferIndex];
                m_pIndexMaxInSection[m_maxBufferSlowIndex] = m_maxBufferIndex;
            }
        }
        
        // find maximum of slow (downsampled) max buffer
        maximum = m_maxMaxBufferSlow;
        if (m_maxCurrentSection > maximum)
        {
            maximum = m_maxCurrentSection;
        }
    
        m_maxBufferIndex++;
        m_maxBufferSectionCounter++;
        
        /* if m_pMaxBuffer section is finished, or end of m_pMaxBuffer is reached,
         store the maximum of this section and open up a new one */
        if ((m_maxBufferSectionCounter >= m_sectionLen)||(m_maxBufferIndex >= m_attack+1)) {
            m_maxBufferSectionCounter = 0;
            
            tmp = m_pMaxBufferSlow[m_maxBufferSlowIndex] = m_maxCurrentSection;
            j = 0;
            if (m_indexMaxBufferSlow == m_maxBufferSlowIndex)
            {
                j = 1;
            }
            m_maxBufferSlowIndex++;
            if (m_maxBufferSlowIndex >= m_nbrMaxBufferSection)
            {
                m_maxBufferSlowIndex = 0;
            }
            if (m_indexMaxBufferSlow == m_maxBufferSlowIndex)
            {
                j = 1;
            }
            m_maxCurrentSection = m_pMaxBufferSlow[m_maxBufferSlowIndex];
            m_pMaxBufferSlow[m_maxBufferSlowIndex] = 0.0f;  /* zero out the value representing the new section */

            /* compute the maximum over all the section */
            if (j)
            {
                m_maxMaxBufferSlow = 0;
                for (j = 0; j < m_nbrMaxBufferSection; j++)
                {
                    if (m_pMaxBufferSlow[j] > m_maxMaxBufferSlow)
                    {
                        m_maxMaxBufferSlow = m_pMaxBufferSlow[j];
                        m_indexMaxBufferSlow = j;
                    }
                }
            }
            else
            {
                if (tmp > m_maxMaxBufferSlow)
                {
                    m_maxMaxBufferSlow = tmp;
                    m_indexMaxBufferSlow = m_maxBufferSlowIndex;
                }
            }
            
            m_maxBufferSectionIndex += m_sectionLen;
        }
        
        if (m_maxBufferIndex >= (m_attack+1))
        {
            m_maxBufferIndex = 0;
            m_maxBufferSectionIndex = 0;
        }
        
        /* needed current gain */
        if (maximum > m_threshold)
        {
            gain = m_threshold / maximum;
        }
        else
        {
            gain = 1;
        }
        
        /*avoid overshoot */
 
        if (gain < m_smoothState) {
            m_fadedGain = min(m_fadedGain, (gain - 0.1f * (float)m_smoothState) * 1.11111111f);
        }
        else 
        {
            m_fadedGain = gain;
        }


        /* smoothing gain */
        if (m_fadedGain < m_smoothState)
        {
            m_smoothState = m_attackConst * (m_smoothState - m_fadedGain) + m_fadedGain;  /* m_attack */
            /*avoid overshoot */
            if (gain > m_smoothState)
            {
                m_smoothState = gain;
            }
        }
        else
        {
            m_smoothState = m_releaseConst * (m_smoothState - m_fadedGain) + m_fadedGain; /* release */
        }
        
        /* fill delay line, apply gain */
        for (j = 0; j < m_channels; j++)
        {
            tmp = m_pDelayBuffer[m_delayBufferIndex * m_channels + j];
            m_pDelayBuffer[m_delayBufferIndex * m_channels + j] = samples[j][i];
            
            tmp *= m_smoothState;
            if (tmp > m_threshold) tmp = m_threshold;
            if (tmp < -m_threshold) tmp = -m_threshold;
            
            samples[j][i] = tmp;
        }
        
        m_delayBufferIndex++;
        if (m_delayBufferIndex >= m_attack)
            m_delayBufferIndex = 0;
        
    }
    
    return LIMITER_OK;
    
}

/* get delay in samples */
int PeakLimiter::getLimiterDelay()
{
  return m_attack;
}

/* get m_attack in Ms */
float PeakLimiter::getLimiterAttack()
{
  return m_attackMs;
}

/* get delay in samples */
int PeakLimiter::getLimiterSampleRate()
{
	return m_sampleRate;
}

/* get delay in samples */
float PeakLimiter::getLimiterRelease()
{
	return m_releaseMs;
}
 
/* get maximum gain reduction of last processed block */
float PeakLimiter::getLimiterMaxGainReduction()
{
  return -20 * (float)log10(m_smoothState);
}

/* set number of channels */
int PeakLimiter::setLimiterNChannels(int nChannelsIn)
{
  if (nChannelsIn > m_maxChannels) return LIMITER_INVALID_PARAMETER;

  m_channels = nChannelsIn;
  resetLimiter();

  return LIMITER_OK;
}

/* set sampling rate */
int PeakLimiter::setLimiterSampleRate(int sampleRateIn)
{
  if (sampleRateIn > m_maxSampleRate) return LIMITER_INVALID_PARAMETER;

  /* update m_attack/release constants */
  m_attack = (int)(m_attackMs * sampleRateIn / 1000);

  if (m_attack < 1) /* m_attack time is too short */
    return LIMITER_INVALID_PARAMETER; 

  /* length of m_pMaxBuffer sections */
  m_sectionLen = (int)sqrt((float)m_attack+1);

  m_nbrMaxBufferSection    = (m_attack+1)/m_sectionLen;
  if (m_nbrMaxBufferSection*m_sectionLen < (m_attack+1))
    m_nbrMaxBufferSection++;
  m_attackConst   = (float)pow(0.1, 1.0 / (m_attack + 1));
  m_releaseConst  = (float)pow(0.1, 1.0 / (m_releaseMs * sampleRateIn / 1000 + 1));
  m_sampleRate    = sampleRateIn;

  /* reset */
  resetLimiter();

  return LIMITER_OK;
}

/* set m_attack time */
int PeakLimiter::setLimiterAttack(float attackMsIn)
{
  if (attackMsIn > m_maxAttackMs) return LIMITER_INVALID_PARAMETER;

  /* calculate attack time in samples */
  m_attack = (int)(attackMsIn * m_sampleRate / 1000);

  if (m_attack < 1) /* attack time is too short */
    m_attack=1;

  /* length of m_pMaxBuffer sections */
  m_sectionLen = (int)sqrt((float)m_attack+1);

  m_nbrMaxBufferSection   = (m_attack+1)/m_sectionLen;
  if (m_nbrMaxBufferSection*m_sectionLen < (m_attack+1))
    m_nbrMaxBufferSection++;
  m_attackConst  = (float)pow(0.1, 1.0 / (m_attack + 1));
  m_attackMs     = attackMsIn;

  /* reset */
  resetLimiter();

  return LIMITER_OK;
}

/* set release time */
int PeakLimiter::setLimiterRelease(float releaseMsIn)
{ 
  m_releaseConst = (float)pow(0.1, 1.0 / (releaseMsIn * m_sampleRate / 1000 + 1));
  m_releaseMs = releaseMsIn;

  return LIMITER_OK;
}

/* set limiter threshold */
int PeakLimiter::setLimiterThreshold(float thresholdIn)
{
  m_threshold = thresholdIn;

  return LIMITER_OK;
}

/* set limiter threshold */
float PeakLimiter::getLimiterThreshold()
{
  return m_threshold;
}


