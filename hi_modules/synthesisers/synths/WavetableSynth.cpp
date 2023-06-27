/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

//#include "ClarinetData.cpp"

namespace hise { using namespace juce;

#ifndef USE_MOD2_WAVETABLESIZE
#define USE_MOD2_WAVETABLESIZE 1
#endif

WavetableSynth::WavetableSynth(MainController *mc, const String &id, int numVoices) :
	ModulatorSynth(mc, id, numVoices)
{
	modChains += {this, "Table Index"};
	modChains += {this, "Table Index Bipolar", ModulatorChain::ModulationType::Normal, Modulation::PanMode};

	finaliseModChains();

	tableIndexChain = modChains[ChainIndex::TableIndex].getChain();
	tableIndexBipolarChain = modChains[ChainIndex::TableIndexBipolar].getChain();

	//modChains[ChainIndex::TableIndex].setExpandToAudioRate(true);
	//modChains[ChainIndex::TableIndexBipolar].setExpandToAudioRate(true);

	parameterNames.add("HqMode");
	parameterNames.add("LoadedBankIndex");
	parameterNames.add("TableIndexValue");
	editorStateIdentifiers.add("TableIndexChainShown");

	for (int i = 0; i < numVoices; i++) 
		addVoice(new WavetableSynthVoice(this));

	tableIndexChain->setColour(JUCE_LIVE_CONSTANT(Colour(0xff4D54B3)));
	tableIndexBipolarChain->setColour(Colour(0xff4D54B3));
}

void WavetableSynth::getWaveformTableValues(int /*displayIndex*/, float const** tableValues, int& numValues, float& normalizeValue)
{
	WavetableSynthVoice *wavetableVoice = dynamic_cast<WavetableSynthVoice*>(getLastStartedVoice());

	if (wavetableVoice != nullptr)
	{
		WavetableSound *wavetableSound = dynamic_cast<WavetableSound*>(wavetableVoice->getCurrentlyPlayingSound().get());

		if (wavetableSound != nullptr)
		{
			const int tableIndex = wavetableVoice->getCurrentTableIndex();
			*tableValues = wavetableSound->getWaveTableData(0, tableIndex);
			numValues = wavetableSound->getTableSize();
			normalizeValue = 1.0f / wavetableSound->getMaxLevel();
		}
	}
	else
	{
		*tableValues = nullptr;
		numValues = 0;
		normalizeValue = 1.0f;
	}
}

float WavetableSynth::getTotalTableModValue(int offset)
{
	offset /= HISE_EVENT_RASTER;

	auto gainMod = modChains[ChainIndex::TableIndex].getModValueForVoiceWithOffset(offset);
	auto bipolarMod = modChains[ChainIndex::TableIndexBipolar].getModValueForVoiceWithOffset(offset);

	bipolarMod *= (float)modChains[ChainIndex::TableIndexBipolar].getChain()->shouldBeProcessedAtAll();

	auto mv = jlimit<float>(0.0f, 1.0f, (tableIndexKnobValue.advance() + bipolarMod) * gainMod);

	return mv * (1.0f - reversed) + (1.0f - mv) * reversed;
}

ProcessorEditorBody* WavetableSynth::createEditor(ProcessorEditor *parentEditor)
{
#if USE_BACKEND

	return new WavetableBody(parentEditor);

#else 

	ignoreUnused(parentEditor);
	jassertfalse;
	return nullptr;

#endif
}


float WavetableSynth::getDisplayTableValue() const
{
	return (1.0f - reversed) * displayTableValue + (1.0f - displayTableValue) * reversed;
}

WavetableSynthVoice::WavetableSynthVoice(ModulatorSynth *ownerSynth):
	ModulatorSynthVoice(ownerSynth),
	wavetableSynth(dynamic_cast<WavetableSynth*>(ownerSynth)),
	octaveTransposeFactor(1),
	currentSound(nullptr),
	hqMode(true)
{
		
};

bool WavetableSynthVoice::updateSoundFromPitchFactor(double pitchFactor, WavetableSound* soundToUse)
{
    if(soundToUse == nullptr)
    {
        auto thisFreq = startFrequency * pitchFactor;
        
        if(!currentSound->getFrequencyRange().contains(thisFreq))
        {
            auto owner = getOwnerSynth();
            
            for(int i = 0; i < owner->getNumSounds(); i++)
            {
                auto ws = static_cast<WavetableSound*>(owner->getSound(i));
                
                if(ws->getFrequencyRange().contains(thisFreq))
                {
                    soundToUse = ws;
                    break;
                }
            }
        }
    }
    
    if(soundToUse == nullptr)
        return false;
    
    if(currentSound != soundToUse)
    {
        currentSound = soundToUse;
        
        lowerTable = currentSound->getWaveTableData(0, currentTableIndex);
        upperTable = lowerTable;

        currentTableIndex = 0;

        tableSize = currentSound->getTableSize();
        
		uptimeDelta = currentSound->getPitchRatio(noteNumberAtStart);
        uptimeDelta *= getOwnerSynth()->getMainController()->getGlobalPitchFactor();

		if (startUptimeDelta != 0.0)
		{
			auto ratio = uptimeDelta / startUptimeDelta;
			voiceUptime *= ratio;
		}
		
		saveStartUptimeDelta();
		

        return true;
    }
    
    
    return false;
}

float calculateSample(const float* lowerTable, const float* upperTable, span<int, 4>& i, bool hqMode, float alpha, float tableAlpha)
{
	float l0 = lowerTable[i[0]];
	float l1 = lowerTable[i[1]];
	float l2 = lowerTable[i[2]];
	float l3 = lowerTable[i[3]];

	float sample;

	if (lowerTable != upperTable)
	{
		float u0 = upperTable[i[0]];
		float u1 = upperTable[i[1]];
		float u2 = upperTable[i[2]];
		float u3 = upperTable[i[3]];

		float upperSample, lowerSample;

		if (hqMode)
		{
			upperSample = Interpolator::interpolateCubic(u0, u1, u2, u3, alpha);
			lowerSample = Interpolator::interpolateCubic(l0, l1, l2, l3, alpha);
		}
		else
		{
			upperSample = Interpolator::interpolateLinear(u1, u2, alpha);
			lowerSample = Interpolator::interpolateLinear(l1, l2, alpha);
		}

		sample = Interpolator::interpolateLinear(lowerSample, upperSample, tableAlpha);
	}
	else
	{
		sample = hqMode ? Interpolator::interpolateCubic(l0, l1, l2, l3, alpha) :
			Interpolator::interpolateLinear(l1, l2, alpha);
	}

	return sample;
}

void WavetableSynthVoice::calculateBlock(int startSample, int numSamples)
{
	const int startIndex = startSample;
	const int samplesToCopy = numSamples;

	const float *voicePitchValues = getOwnerSynth()->getPitchValuesForVoice();
	
	auto numTables = currentSound->getWavetableAmount();

	auto stereoMode = currentSound->isStereo();

	while (--numSamples >= 0)
	{
		int index = (int)voiceUptime;

		span<int, 4> i;

#if USE_MOD2_WAVETABLESIZE
		i[0] = (index + tableSize - 1) & (tableSize - 1);
		i[1] = index & (tableSize - 1);
		i[2] = (index+1) & (tableSize - 1);
		i[3] = (index + 2) & (tableSize - 1);
#else
        const auto i[1] = index % (tableSize);

        auto i[2] = i[1] + 1;
        auto i[0] = i[1] - 1;
        auto i[3] = i[1] + 2;
            
        if (i[1] == 0)         i[0] = tableSize-1;
		if (i[2] >= tableSize) i[2] = 0;
        if (i[3] >= tableSize) i[3] = 0;

#endif

		const float tableModValue = static_cast<WavetableSynth*>(getOwnerSynth())->getTotalTableModValue(startSample);
		const float tableValue = tableModValue * (float)(numTables - 1);

		const int lowerTableIndex = (int)(tableValue);

		if (i[2] < i[1])
			currentTableIndex = lowerTableIndex;

		const float tableDelta = tableValue - (float)lowerTableIndex;
		jassert(0.0f <= tableDelta && tableDelta <= 1.0f);

		const int upperTableIndex = jmin(numTables - 1, lowerTableIndex + 1);

		lowerTable = currentSound->getWaveTableData(0, lowerTableIndex);
		upperTable = currentSound->getWaveTableData(0, upperTableIndex);
		const float alpha = float(voiceUptime) - (float)index;

		auto l = calculateSample(lowerTable, upperTable, i, hqMode, alpha, tableDelta);

		// Stereo mode assumed
		voiceBuffer.setSample(0, startSample, l);

		if (stereoMode)
		{
			auto lowerTableR = currentSound->getWaveTableData(1, lowerTableIndex);
			auto upperTableR = currentSound->getWaveTableData(1, upperTableIndex);

			auto r = calculateSample(lowerTableR, upperTableR, i, hqMode, alpha, tableDelta);
			voiceBuffer.setSample(1, startSample, r);
		}

		jassert(voicePitchValues == nullptr || voicePitchValues[startSample] > 0.0f);

		const double delta = (uptimeDelta * (voicePitchValues == nullptr ? 1.0 : voicePitchValues[startSample]));

		voiceUptime += delta;

		++startSample;
	}

	if (hqMode)
	{
		auto pf = voicePitchValues != nullptr ? voicePitchValues[startIndex + samplesToCopy / 2] : (uptimeDelta / startUptimeDelta);

		updateSoundFromPitchFactor(pf, nullptr);
	}

	if (auto modValues = getOwnerSynth()->getVoiceGainValues())
	{
		FloatVectorOperations::multiply(voiceBuffer.getWritePointer(0, startIndex), modValues + startIndex, samplesToCopy);

		if(stereoMode)
			FloatVectorOperations::multiply(voiceBuffer.getWritePointer(1, startIndex), modValues + startIndex, samplesToCopy);
		else
			FloatVectorOperations::copy(voiceBuffer.getWritePointer(1, startIndex), voiceBuffer.getReadPointer(0, startIndex), samplesToCopy);
		
	}
	else
	{
		const float constantGain = getOwnerSynth()->getConstantGainModValue();

		FloatVectorOperations::multiply(voiceBuffer.getWritePointer(0, startIndex), constantGain, samplesToCopy);

		if(stereoMode)
			FloatVectorOperations::multiply(voiceBuffer.getWritePointer(1, startIndex), constantGain, samplesToCopy);
		else
			FloatVectorOperations::copy(voiceBuffer.getWritePointer(1, startIndex), voiceBuffer.getReadPointer(0, startIndex), samplesToCopy);
	}

	getOwnerSynth()->effectChain->renderVoice(voiceIndex, voiceBuffer, startIndex, samplesToCopy);

	if (getOwnerSynth()->getLastStartedVoice() == this)
	{
		auto owner = static_cast<WavetableSynth*>(getOwnerSynth());
		owner->setDisplayTableValue((float)currentTableIndex / (float)numTables);
		owner->triggerWaveformUpdate();
	}

		
}

void WavetableSynthVoice::startNote(int midiNoteNumber, float /*velocity*/, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    currentSound = nullptr;
	currentTableIndex = 0;
    
	ModulatorSynthVoice::startNote(midiNoteNumber, 0.0f, nullptr, -1);

	midiNoteNumber += getTransposeAmount();
    
    noteNumberAtStart = midiNoteNumber;
    
    startFrequency = MidiMessage::getMidiNoteInHertz(noteNumberAtStart);
    
	

    updateSoundFromPitchFactor(1.0, static_cast<WavetableSound*>(s));
    
	voiceUptime = (double)getCurrentHiseEvent().getStartOffset() / 441.0 * (double)tableSize;

}

WavetableSound::WavetableSound(const ValueTree &wavetableData, Processor* parent)
{
	jassert(wavetableData.getType() == Identifier("wavetable"));

	stereo = wavetableData.hasProperty("data1");

	reversed = (float)(int)wavetableData.getProperty("reversed", false);
	

	MemoryBlock mb = MemoryBlock(*wavetableData.getProperty("data", var::undefined()).getBinaryData());
	const int numSamples = (int)(mb.getSize() / sizeof(float));

	wavetables.setSize(stereo ? 2 : 1, numSamples);

	FloatVectorOperations::copy(wavetables.getWritePointer(0, 0), (float*)mb.getData(), numSamples);

	if (stereo)
	{
		MemoryBlock mb2 = MemoryBlock(*wavetableData.getProperty("data1", var::undefined()).getBinaryData());
		FloatVectorOperations::copy(wavetables.getWritePointer(1, 0), (float*)mb2.getData(), numSamples);
	}

	maximum = wavetables.getMagnitude(0, numSamples);

	wavetableAmount = wavetableData.getProperty("amount", 64);

	sampleRate = wavetableData.getProperty("sampleRate", 48000.0);

	midiNotes.setRange(0, 127, false);
	noteNumber = wavetableData.getProperty("noteNumber", 0);
	midiNotes.setBit(noteNumber, true);

    if(wavetableData.hasProperty(SampleIds::LoKey))
    {
        auto l = (int)wavetableData[SampleIds::LoKey];
        auto h = (int)wavetableData[SampleIds::HiKey];
        
        midiNotes.setRange(l, h - l + 1, true);
    }
    
	wavetableSize = numSamples / wavetableAmount;

#if USE_MOD2_WAVETABLESIZE

	if (!isPowerOfTwo(wavetableSize))
	{
		debugError(parent, "Wavetable with non-power two buffer size loaded. Please recompile HISE without USE_MOD2_WAVETABLESIZE.");
	}

#endif

	emptyBuffer = AudioSampleBuffer(1, wavetableSize);
	emptyBuffer.clear();

	unnormalizedMaximum = 0.0f;

	normalizeTables();

	pitchRatio = 1.0;
    
    auto lowDelta = MidiMessage::getMidiNoteInHertz(midiNotes.findNextSetBit(0));
    auto highDelta = MidiMessage::getMidiNoteInHertz(midiNotes.getHighestBit());
                                
    
    frequencyRange = { lowDelta, highDelta };
}

const float * WavetableSound::getWaveTableData(int channelIndex, int wavetableIndex) const
{
	jassert(isPositiveAndBelow(wavetableIndex, wavetableAmount));
	jassert((wavetableIndex + 1) * wavetableSize <= wavetables.getNumSamples());
	jassert(channelIndex == 0 || isStereo());

	return wavetables.getReadPointer(channelIndex, wavetableIndex * wavetableSize);
}

void WavetableSound::calculatePitchRatio(double playBackSampleRate_)
{
    playbackSampleRate = playBackSampleRate_;
    
	const double idealCycleLength = playbackSampleRate / MidiMessage::getMidiNoteInHertz(noteNumber);

	pitchRatio = (double)wavetableSize / idealCycleLength;
}

void WavetableSound::normalizeTables()
{
	unnormalizedGainValues.calloc(wavetableAmount);

	for (int i = 0; i < wavetableAmount; i++)
	{
		const float peak = wavetables.getMagnitude(i * wavetableSize, wavetableSize);

		unnormalizedGainValues[i] = peak;

		if (peak == 0.0f) continue;

		if (peak > unnormalizedMaximum)
			unnormalizedMaximum = peak;
	}

	maximum = 1.0f;
}

} // namespace hise
