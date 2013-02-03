/* ----------------------------------------------------------------------------

   Squeezer
   ========
   Flexible general-purpose audio compressor with a touch of lemon.

   Copyright (c) 2013 Martin Zuther (http://www.mzuther.de/)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Thank you for using free software!

---------------------------------------------------------------------------- */

#include "side_chain.h"


float SideChain::fMeterMinimumDecibel;


SideChain::SideChain(int nSampleRate)
/*  Constructor.

    return value: none
*/
{
    setSampleRate(nSampleRate);

    setThreshold(-12.0f);
    setRatio(2.0f);
    setKneeWidth(0.0f);

    nDetector = Compressor::DetectorSmoothBranching;
    setAttackRate(10);
    setReleaseRate(100);
    setDetector(nDetector);

    fCrestFactorAutoGain = 0.0f;
    fMeterMinimumDecibel = -70.01f;

    // reset (i.e. initialise) all relevant variables
    reset();
}


SideChain::~SideChain()
/*  Destructor.

    return value: none
*/
{
}


void SideChain::reset()
/*  Reset all relevant variables.

    return value: none
*/
{
    fGainReduction = 0.0f;
    fGainCompensation = 0.0f;
}


void SideChain::setSampleRate(int nSampleRate)
/*  Set new sample rate.

    nSampleRate (int): new sample rate in Hertz

    return value: none
 */
{
    fSampleRate = (float) nSampleRate;
}


int SideChain::getDetector()
/*  Get current compressor detector type.

    return value (integer): returns compressor detector type
 */
{
    return nDetector;
}


void SideChain::setDetector(int nDetectorNew)
/*  Set new compressor detector type.

    nDetectorNew (integer): new compressor detector type

    return value: none
 */
{
    nDetector = nDetectorNew;
    fGainReductionIntermediate = 0.0f;

    setAttackRate(nAttackRate);
    setReleaseRate(nReleaseRate);
}


float SideChain::getThreshold()
/*  Get current threshold.

    return value (float): returns the current threshold in decibels
 */
{
    return fThreshold;
}


void SideChain::setThreshold(float fThresholdNew)
/*  Set new threshold.

    fThresholdNew (float): new threshold in decibels

    return value: none
 */
{
    fThreshold = fThresholdNew;
    fGainCompensation = calculateFinalGainReduction(fCrestFactorAutoGain);
}


float SideChain::getRatio()
/*  Get current compression ratio.

    return value (float): returns the current compression ratio
 */
{
    return 1.0f / (1.0f - fRatioInternal);
}


void SideChain::setRatio(float fRatioNew)
/*  Set new compression ratio.

    nRatioNew (float): new compression ratio

    return value: none
 */
{
    fRatioInternal = 1.0f - (1.0f / fRatioNew);
    fGainCompensation = calculateFinalGainReduction(fCrestFactorAutoGain);
}


float SideChain::getKneeWidth()
/*  Get current knee width.

    return value (float): returns the current knee width in decibels
 */
{
    return fKneeWidth;
}


void SideChain::setKneeWidth(float fKneeWidthNew)
/*  Set new knee width.

    nKneeWidthNew (float): new knee width in decibels

    return value: none
 */
{
    fKneeWidth = fKneeWidthNew;
    fKneeWidthHalf = fKneeWidth / 2.0f;
    fKneeWidthDouble = fKneeWidth * 2.0f;
}


int SideChain::getAttackRate()
/*  Get current attack rate.

    return value (integer): returns the current attack rate in
    milliseconds
 */
{
    return nAttackRate;
}


void SideChain::setAttackRate(int nAttackRateNew)
/*  Set new attack rate.

    nAttackRateNew (integer): new attack rate in milliseconds

    return value: none
 */
{
    nAttackRate = nAttackRateNew;

    if (nAttackRate == 0)
    {
        fAttackCoefficient = 0.0f;
    }
    else
    {
        float fAttackRateSeconds = nAttackRate / 1000.0f;

        if (nDetector == Compressor::DetectorLinear)
        {
            // rise time: rises 10 dB per interval defined in attack
            // rate (linear)
            fAttackCoefficient = 10.0f / (fAttackRateSeconds * fSampleRate);
        }
        else
        {
            // logarithmic envelope reaches 95% of the final reading
            // in the given attack time
            fAttackCoefficient = expf(logf(0.05f) / (fAttackRateSeconds * fSampleRate));
        }
    }
}


int SideChain::getReleaseRate()
/*  Get current release rate.

    return value (integer): returns the current release rate in
    milliseconds
 */
{
    return nReleaseRate;
}


void SideChain::setReleaseRate(int nReleaseRateNew)
/*  Set new release rate.

    nReleaseRateNew (integer): new release rate in milliseconds

    return value: none
 */
{
    nReleaseRate = nReleaseRateNew;

    if (nReleaseRate == 0)
    {
        fReleaseCoefficient = 0.0f;
    }
    else
    {
        float fReleaseRateSeconds = nReleaseRate / 1000.0f;

        if (nDetector == Compressor::DetectorLinear)
        {
            // fall time: falls 10 dB per interval defined in release
            // rate (linear)
            fReleaseCoefficient = 10.0f / (fReleaseRateSeconds * fSampleRate);
        }
        else
        {
            // logarithmic envelope reaches 95% of the final reading
            // in the given release time
            fReleaseCoefficient = expf(logf(0.05f) / (fReleaseRateSeconds * fSampleRate));
        }
    }
}


float SideChain::getGainReduction(bool useGainCompensation)
/*  Get current gain reduction.

    useGainCompensation (boolean): determines whether the gain
    reduction should be level compensated or not

    return value (float): returns the current gain reduction in
    decibel
 */
{
    if (useGainCompensation)
    {
        return fGainReduction - fGainCompensation;
    }
    else
    {
        return fGainReduction;
    }
}


float SideChain::calculateFinalGainReduction(float fInputLevel)
/*  Calculate final gain reduction from input level.

    fInputLevel (float): current input level in decibels

    return value: calculated final gain reduction in decibels
 */
{
    float fAboveThreshold = fInputLevel - fThreshold;

    if (fKneeWidth == 0.0f)
    {
        if (fInputLevel <= fThreshold)
        {
            return 0.0f;
        }
        else
        {
            return fAboveThreshold * fRatioInternal;
        }
    }
    else
    {
        // algorithm adapted from Giannoulis et al., "Digital Dynamic
        // Range Compressor Design - A Tutorial and Analysis", JAES,
        // 60(6):399-408, 2012
        if (fAboveThreshold < -fKneeWidthHalf)
        {
            return 0.0f;
        }
        else if (fAboveThreshold > fKneeWidthHalf)
        {
            return fAboveThreshold * fRatioInternal;
        }
        else
        {
            float fFactor = fAboveThreshold + fKneeWidthHalf;
            float fFactorSquared = fFactor * fFactor;

            return fFactorSquared / fKneeWidthDouble * fRatioInternal;
        }
    }
}


void SideChain::processSample(float fInputLevel)
/*  Process a single audio sample value.

    fInputLevel (float): current audio sample value in decibels

    return value: current gain reduction in decibels
*/
{
    // feed input level to gain computer
    float fGainReductionNew = calculateFinalGainReduction(fInputLevel);

    // feed output from gain computer to level detector
    switch (nDetector)
    {
    case Compressor::DetectorLinear:
        applyDetectorLinear(fGainReductionNew);
        break;

    case Compressor::DetectorSmoothBranching:
        applyDetectorSmoothBranching(fGainReductionNew);
        break;

    case Compressor::DetectorSmoothDecoupled:
        applyDetectorSmoothDecoupled(fGainReductionNew);
        break;

    default:
        DBG("[Squeezer] sidechain::processSample --> invalid detector");
        break;
    }
}


void SideChain::applyDetectorLinear(float fGainReductionNew)
/*  Calculate linear detector.

    fGainReductionNew (float): calculated new gain reduction in
    decibels

    return value: none
*/
{
    // no change in gain reduction
    if (fGainReductionNew == fGainReduction)
    {
        return;
    }
    // apply attack rate if proposed gain reduction is above old gain
    // reduction
    else if (fGainReductionNew > fGainReduction)
    {
        if (fAttackCoefficient == 0.0f)
        {
            fGainReduction = fGainReductionNew;
        }
        else
        {
            fGainReduction += fAttackCoefficient;

            if (fGainReduction > fGainReductionNew)
            {
                fGainReduction = fGainReductionNew;
            }
        }
    }
    // otherwise, apply release rate if proposed gain reduction is
    // below old gain reduction
    else
    {
        if (fReleaseCoefficient == 0.0f)
        {
            fGainReduction = fGainReductionNew;
        }
        else
        {
            fGainReduction -= fReleaseCoefficient;

            if (fGainReduction < fGainReductionNew)
            {
                fGainReduction = fGainReductionNew;
            }
        }
    }
}


void SideChain::applyDetectorSmoothBranching(float fGainReductionNew)
/*  Calculate smooth branching detector.

    fGainReductionNew (float): calculated gain reduction in decibels

    return value: none
*/
{
    // apply attack rate if proposed gain reduction is above old gain
    // reduction
    if (fGainReductionNew > fGainReduction)
    {
        if (fAttackCoefficient == 0.0f)
        {
            fGainReduction = fGainReductionNew;
        }
        else
        {
            // algorithm adapted from Giannoulis et al., "Digital
            // Dynamic Range Compressor Design - A Tutorial and
            // Analysis", JAES, 60(6):399-408, 2012

            float fGainReductionOld = fGainReduction;
            fGainReduction = (fAttackCoefficient * fGainReductionOld) + (1.0f - fAttackCoefficient) * fGainReductionNew;
        }
    }
    // otherwise, apply release rate if proposed gain reduction is
    // below old gain reduction
    else
    {
        if (fReleaseCoefficient == 0.0f)
        {
            fGainReduction = fGainReductionNew;
        }
        else
        {
            // algorithm adapted from Giannoulis et al., "Digital
            // Dynamic Range Compressor Design - A Tutorial and
            // Analysis", JAES, 60(6):399-408, 2012

            float fGainReductionOld = fGainReduction;
            fGainReduction = (fReleaseCoefficient * fGainReductionOld) + (1.0f - fReleaseCoefficient) * fGainReductionNew;
        }
    }
}


void SideChain::applyDetectorSmoothDecoupled(float fGainReductionNew)
/*  Calculate smooth decoupled detector.

    fGainReductionNew (float): calculated gain reduction in decibels

    return value: none
*/
{
    // algorithm adapted from Giannoulis et al., "Digital Dynamic
    // Range Compressor Design - A Tutorial and Analysis", JAES,
    // 60(6):399-408, 2012
    //
    // apply release envelope
    if (fReleaseCoefficient == 0.0f)
    {
        fGainReductionIntermediate = fGainReductionNew;
    }
    else
    {
        float fGainReductionIntermediateOld = fGainReductionIntermediate;
        fGainReductionIntermediate = (fReleaseCoefficient * fGainReductionIntermediateOld) + (1.0f - fReleaseCoefficient) * fGainReductionNew;

        // maximally fast peak detection
        if (fGainReductionNew > fGainReductionIntermediate)
        {
            fGainReductionIntermediate = fGainReductionNew;
        }
    }

    // apply attack envelope
    if (fAttackCoefficient == 0.0f)
    {
        fGainReduction = fGainReductionIntermediate;
    }
    else
    {
        float fGainReductionOld = fGainReduction;
        fGainReduction = (fAttackCoefficient * fGainReductionOld) + (1.0f - fAttackCoefficient) * fGainReductionIntermediate;
    }
}


float SideChain::level2decibel(float fLevel)
/*  Convert level from linear scale to decibels (dB).

    fLevel (float): audio level

    return value (float): returns given level in decibels (dB) when
    above "fMeterMinimumDecibel", otherwise "fMeterMinimumDecibel"
*/
{
    // log(0) is not defined, so return "fMeterMinimumDecibel"
    if (fLevel == 0.0f)
    {
        return fMeterMinimumDecibel;
    }
    else
    {
        // calculate decibels from audio level (a factor of 20.0 is
        // needed to calculate *level* ratios, whereas 10.0 is needed
        // for *power* ratios!)
        float fDecibels = 20.0f * log10f(fLevel);

        // to make meter ballistics look nice for low levels, do not
        // return levels below "fMeterMinimumDecibel"
        if (fDecibels < fMeterMinimumDecibel)
        {
            return fMeterMinimumDecibel;
        }
        else
        {
            return fDecibels;
        }
    }
}


float SideChain::decibel2level(float fDecibels)
/*  Convert level from decibels (dB) to linear scale.

    fLevel (float): audio level in decibels (dB)

    return value (float): given level in linear scale
*/
{
    // calculate audio level from decibels (a divisor of 20.0 is
    // needed to calculate *level* ratios, whereas 10.0 is needed for
    // *power* ratios!)
    float fLevel = powf(10.0f, fDecibels / 20.0f);
    return fLevel;
}


// Local Variables:
// ispell-local-dictionary: "british"
// End: