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

#ifndef WAVETABLESYNTH_H_INCLUDED
#define WAVETABLESYNTH_H_INCLUDED

//#include "ClarinetData.h"

namespace hise { using namespace juce;

#define WAVETABLE_HQ_MODE 1

class WavetableSynth;

class WavetableSound: public ModulatorSynthSound
{
public:

	/** Creates a new wavetable sound.
	*
	*	You have to supply a ValueTree with the following properties:
	*
	*	- 'data' the binary data
	*	- 'amount' the number of wavetables
	*	- 'noteNumber' the noteNumber
	*	- 'sampleRate' the sample rate
	*
	*/
	WavetableSound(const ValueTree &wavetableData, Processor* parent);;

	bool appliesToNote (int midiNoteNumber) override   { return midiNotes[midiNoteNumber]; }
    bool appliesToChannel (int /*midiChannel*/) override   { return true; }
	bool appliesToVelocity (int /*midiChannel*/) override  { return true; }

	/** Returns a read pointer to the wavetable with the given index.
	*
	*	Make sure you don't get off bounds, it will return a nullptr if the index is bigger than the wavetable amount.
	*/
	const float *getWaveTableData(int channelIndex, int wavetableIndex) const;

	float getUnnormalizedMaximum()
	{
		return unnormalizedMaximum;
	}

	int getWavetableAmount() const
	{
		return wavetableAmount;
	}

	int getTableSize() const
	{
		return wavetableSize;
	};

	float getMaxLevel() const
	{
		return maximum;
	}

	/** Call this in the prepareToPlay method to calculate the pitch error. It also resamples if the playback frequency is different. */
	void calculatePitchRatio(double playBackSampleRate);

	double getPitchRatio(double midiNoteNumber) const
	{
        double p = pitchRatio;
        
        p *= hmath::pow(2.0, (midiNoteNumber - noteNumber) / 12.0);
        
		return p;
	}
    
    Range<double> getFrequencyRange() const
    {
        return frequencyRange;
    }

	void normalizeTables();

	float getUnnormalizedGainValue(int tableIndex)
	{
		jassert(isPositiveAndBelow(tableIndex, wavetableAmount));
		
		return unnormalizedGainValues[tableIndex];
	}

	bool isStereo() const { return stereo; };

	float isReversed() const { return reversed; };

private:

	float reversed = 0.0f;
	bool stereo = false;

	float maximum;
	float unnormalizedMaximum;
	HeapBlock<float> unnormalizedGainValues;

    Range<double> frequencyRange;
    
	BigInteger midiNotes;
	int noteNumber;

	AudioSampleBuffer wavetables;
	AudioSampleBuffer emptyBuffer;

	double sampleRate;
	double pitchRatio;
    double playbackSampleRate;

	int wavetableSize;
	int wavetableAmount;
};

class WavetableSynth;

class WavetableSynthVoice: public ModulatorSynthVoice
{
public:

	WavetableSynthVoice(ModulatorSynth *ownerSynth);

	bool canPlaySound(SynthesiserSound *) override
	{
		return true;
	};

	int getSmoothSize() const;

	void startNote (int midiNoteNumber, float /*velocity*/, SynthesiserSound* s, int /*currentPitchWheelPosition*/) override;;

	void calculateBlock(int startSample, int numSamples) override;;

	int getCurrentTableIndex() const
	{
		return currentTableIndex;
	};

	void setHqMode(bool useHqMode)
	{
		hqMode = useHqMode;
	};

    bool updateSoundFromPitchFactor(double pitchFactor, WavetableSound* soundToUse);
    
private:

	WavetableSynth *wavetableSynth;

	int octaveTransposeFactor;

	WavetableSound *currentSound;

	int currentWaveIndex;
	int nextWaveIndex;
	int tableSize;
	int currentTableIndex;

	float currentGainValue;
    int noteNumberAtStart = 0;
    double startFrequency = 0.0;
	
	Interpolator tableGainInterpolator;

	bool hqMode;

	float const *lowerTable;
	float const *upperTable;



	float const *currentTable;
};


/** A two-dimensional wavetable synthesiser.
	@ingroup synthTypes
*/
class WavetableSynth: public ModulatorSynth,
					  public WaveformComponent::Broadcaster
{
public:

	SET_PROCESSOR_NAME("WavetableSynth", "Wavetable Synthesiser", "A two-dimensional wavetable synthesiser.");

	enum EditorStates
	{
		TableIndexChainShow = ModulatorSynth::numEditorStates,
		TableIndexBipolarShow
	};

	enum SpecialParameters
	{
		HqMode = ModulatorSynth::numModulatorSynthParameters,
		LoadedBankIndex,
		TableIndexValue,
		numSpecialParameters
	};

	enum ChainIndex
	{
		Gain = 0,
		Pitch = 1,
		TableIndex = 2,
		TableIndexBipolar
	};

	enum class WavetableInternalChains
	{
		TableIndexModulation = ModulatorSynth::numInternalChains,
		TableIndexBipolarChain,
		numInternalChains
	};

	WavetableSynth(MainController *mc, const String &id, int numVoices);;

	void loadWaveTable(const ValueTree& v)
	{
		clearSounds();
        
		jassert(v.isValid());
        
		for(int i = 0; i < v.getNumChildren(); i++)
		{
			auto s = new WavetableSound(v.getChild(i), this);

			s->calculatePitchRatio(getSampleRate());

			reversed = s->isReversed();

			addSound(s);
		}
	}

	void getWaveformTableValues(int displayIndex, float const** tableValues, int& numValues, float& normalizeValue) override;
	
	void restoreFromValueTree(const ValueTree &v) override
	{
		ModulatorSynth::restoreFromValueTree(v);

		loadAttribute(LoadedBankIndex, "LoadedBankIndex");
		loadAttribute(HqMode, "HqMode");
		loadAttributeWithDefault(TableIndexValue);
	};

	ValueTree exportAsValueTree() const override
	{
		ValueTree v = ModulatorSynth::exportAsValueTree();

		saveAttribute(HqMode, "HqMode");
		saveAttribute(LoadedBankIndex, "LoadedBankIndex");
		saveAttribute(TableIndexValue, "TableIndexValue");

		return v;
	}

	int getNumChildProcessors() const override { return (int)WavetableInternalChains::numInternalChains;	};

	int getNumInternalChains() const override {return (int)WavetableInternalChains::numInternalChains; };

	virtual Processor *getChildProcessor(int processorIndex) override
	{
		jassert(processorIndex < (int)WavetableInternalChains::numInternalChains);

		switch(processorIndex)
		{
		case GainModulation:	return gainChain;
		case PitchModulation:	return pitchChain;
		case (int)WavetableInternalChains::TableIndexModulation:	return tableIndexChain;
		case (int)WavetableInternalChains::TableIndexBipolarChain: return tableIndexBipolarChain;
		case MidiProcessor:		return midiProcessorChain;
		case EffectChain:		return effectChain;
		default:				jassertfalse; return nullptr;
		}
	};

	virtual const Processor *getChildProcessor(int processorIndex) const override
	{
		jassert(processorIndex < (int)WavetableInternalChains::numInternalChains);

		switch(processorIndex)
		{
		case GainModulation:	return gainChain;
		case PitchModulation:	return pitchChain;
		case (int)WavetableInternalChains::TableIndexModulation:	return tableIndexChain;
		case (int)WavetableInternalChains::TableIndexBipolarChain: return tableIndexBipolarChain;
		case MidiProcessor:		return midiProcessorChain;
		case EffectChain:		return effectChain;
		default:				jassertfalse; return nullptr;
		}
	};

	void prepareToPlay(double newSampleRate, int samplesPerBlock) override
	{
		if(newSampleRate > -1.0)
		{
			for(int i = 0; i < sounds.size(); i++)
			{
				static_cast<WavetableSound*>(getSound(i))->calculatePitchRatio(newSampleRate);
			}

			
		}

		if (samplesPerBlock > 0 && newSampleRate > 0.0)
			tableIndexKnobValue.prepare(newSampleRate, 80.0);


		ModulatorSynth::prepareToPlay(newSampleRate, samplesPerBlock);
	}

	float getTotalTableModValue(int offset);

#if 0
	const float *getTableModValues(ChainIndex modIndex) const
	{
		return modChains[modIndex].getReadPointerForVoiceValues(0);
	}

	float getConstantTableModValue(ChainIndex modIndex) const noexcept
	{
		return modChains[modIndex].getConstantModulationValue();
	}
#endif

	float getDefaultValue(int parameterIndex) const override
	{
		if (parameterIndex < ModulatorSynth::numModulatorSynthParameters) return ModulatorSynth::getDefaultValue(parameterIndex);

		switch (parameterIndex)
		{
		case HqMode: return 1.0f;
		case LoadedBankIndex: return -1.0f;
		case TableIndexValue: return 1.0f;
		default: jassertfalse; return 0.0f;
		}
	}

	float getAttribute(int parameterIndex) const override 
	{
		if(parameterIndex < ModulatorSynth::numModulatorSynthParameters) return ModulatorSynth::getAttribute(parameterIndex);

		switch(parameterIndex)
		{
		case HqMode:		return hqMode ? 1.0f : 0.0f;
		case LoadedBankIndex: return (float)currentBankIndex;
		case TableIndexValue: return tableIndexKnobValue.targetValue;
		default:			jassertfalse; return -1.0f;
		}
	};

	void setInternalAttribute(int parameterIndex, float newValue) override
	{
		if(parameterIndex < ModulatorSynth::numModulatorSynthParameters)
		{
			ModulatorSynth::setInternalAttribute(parameterIndex, newValue);
			return;
		}

		switch(parameterIndex)
		{
		case HqMode:
			{
				ScopedLock sl(getMainController()->getLock());

				hqMode = newValue == 1.0f;

				for(int i = 0; i < getNumVoices(); i++)
				{
					static_cast<WavetableSynthVoice*>(getVoice(i))->setHqMode(hqMode);
				}
				break;
			}
		case TableIndexValue:
			tableIndexKnobValue.set(jlimit(0.0f, 1.0f, newValue));

			if(getNumActiveVoices() == 0)
				setDisplayTableValue((1.0f - reversed) * newValue + (1.0f - newValue) * reversed);

			break;
		case LoadedBankIndex:
		{
			loadWavetableFromIndex((int)newValue);
			break;

		}
		default:					jassertfalse;
									break;
		}
	};

	ProcessorEditorBody* createEditor(ProcessorEditor *parentEditor) override;

	StringArray getWavetableList() const
	{
		auto dir = getMainController()->getCurrentFileHandler().getSubDirectory(ProjectHandler::SubDirectories::AudioFiles);

		Array<File> wavetables;
		dir.findChildFiles(wavetables, File::findFiles, true, "*.hwt");
		wavetables.sort();

		StringArray sa;

		for (auto& f : wavetables)
			sa.add(f.getFileNameWithoutExtension());

		return sa;
	}

	void loadWavetableFromIndex(int index)
	{
		if (currentBankIndex != index)
		{
			currentBankIndex = index;
		}

		if (currentBankIndex == 0)
		{
			clearSounds();
		}

		auto dir = getMainController()->getCurrentFileHandler().getSubDirectory(ProjectHandler::SubDirectories::AudioFiles);

		Array<File> wavetables;

		dir.findChildFiles(wavetables, File::findFiles, true, "*.hwt");
		wavetables.sort();

		if (wavetables[index-1].existsAsFile())
		{
			FileInputStream fis(wavetables[index-1]);

			ValueTree v = ValueTree::readFromStream(fis);

			loadWaveTable(v);
		}
		else
		{
			clearSounds();
		}
	}

	void setDisplayTableValue(float nv)
	{
		displayTableValue = nv;
	}

	float getDisplayTableValue() const;

	void renderNextBlockWithModulators(AudioSampleBuffer& outputAudio, const HiseEventBuffer& inputMidi) override
	{
		ModulatorSynth::renderNextBlockWithModulators(outputAudio, inputMidi);
	}

private:

	float displayTableValue = 1.0f;

	sfloat tableIndexKnobValue;
	
	float reversed = 0.0f;

	int currentBankIndex = 0;

	bool hqMode;

	ModulatorChain* tableIndexChain;
	ModulatorChain* tableIndexBipolarChain;
};


} // namespace hise

#endif  // WAVETABLESYNTH_H_INCLUDED
