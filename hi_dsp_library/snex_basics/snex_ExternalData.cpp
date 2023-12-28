/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licences for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licencing:
*
*   http://www.hartinstruments.net/hise/
*
*   HISE is based on the JUCE library,
*   which also must be licenced for commercial applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/


namespace snex {
	using namespace Types;
	using namespace hise;

ExternalData ExternalDataHolder::getData(ExternalData::DataType t, int index)
{
	using DataType = ExternalData::DataType;

	if (t == DataType::Table)
	{
		if (auto t = getTable(index))
		{
			return ExternalData(t, index);
		}
	}
	if (t == DataType::SliderPack)
	{
		if (auto sp = getSliderPack(index))
		{
			return ExternalData(sp, index);
		}
	}
	if (t == DataType::AudioFile)
	{
		if (auto bf = getAudioFile(index))
		{
			return ExternalData(bf, index);
		}
	}
	if (t == DataType::FilterCoefficients)
	{
		if (auto fd = getFilterData(index))
		{
			return ExternalData(fd, index);
		}
	}
	if (t == DataType::DisplayBuffer)
	{
		if (auto rb = getDisplayBuffer(index))
			return ExternalData(rb, index);
	}

	return ExternalData();
}

void ExternalDataProviderBase::initExternalData()
{
	int totalIndex = 0;

	if (externalDataHolder == nullptr)
		return;

	auto initAll = [this, &totalIndex](ExternalData::DataType d)
	{
		for (int i = 0; i < getNumRequiredDataObjects(d); i++)
		{
			auto b = externalDataHolder->getData(d, i);
			setExternalData(b, totalIndex++);
		}
	};

	ExternalData::forEachType(initAll);
}

int ExternalDataHolder::getAbsoluteIndex(ExternalData::DataType t, int dataIndex) const
{
	int offset = 0;

	ExternalData::forEachType([&](ExternalData::DataType dt)
	{
		if(t > dt)
			offset += getNumDataObjects(dt);
	});

	return offset + dataIndex;
}

hise::ComplexDataUIBase* ExternalDataHolder::getComplexBaseType(ExternalData::DataType t, int index)
{
	switch (t)
	{
	case ExternalData::DataType::Table:		 return getTable(index);
	case ExternalData::DataType::SliderPack: return getSliderPack(index);
	case ExternalData::DataType::AudioFile:  return getAudioFile(index);
	case ExternalData::DataType::FilterCoefficients: return getFilterData(index);
	case ExternalData::DataType::DisplayBuffer: return getDisplayBuffer(index);
    default: return nullptr;
	}
}


ExternalData::ExternalData(scriptnode::data::embedded::multichannel_data& d, DataType type_):
	dataType(DataType::AudioFile),
	obj(d.buffer.get())
{
	jassert(type_ == DataType::AudioFile);
	numSamples = d.getNumSamples();
	numChannels = d.getNumChannels();
	sampleRate = d.getSamplerate();

	for (int i = 0; i < numChannels; i++)
		d.channelData[i] = const_cast<float*>(d.getChannelData(i));

	data = d.channelData;

	auto mb = dynamic_cast<MultiChannelAudioBuffer*>(obj);

	MultiChannelAudioBuffer::SampleReference::Ptr newRef = new hise::MultiChannelAudioBuffer::SampleReference();

	newRef->buffer.setDataToReferTo(d.channelData, numChannels, numSamples);
	newRef->r = Result::ok();
	newRef->sampleRate = sampleRate;
	newRef->loopRange = {}; // Implement this maybe?
	
	mb->loadFromEmbeddedData(newRef);
}




ExternalData::ExternalData(ComplexDataUIBase* b, int absoluteIndex) :
	dataType(getDataTypeForClass(b)),
	obj(b)
{
	SimpleReadWriteLock::ScopedReadLock sl(b->getDataLock());

	switch (dataType)
	{
	case DataType::AudioFile:
	{
		auto buffer = dynamic_cast<MultiChannelAudioBuffer*>(obj);

		if (buffer->isXYZ())
		{
			isXYZAudioData = true;
			data = buffer->getXYZItems().begin();
			numSamples = buffer->getXYZItems().size();

			if (numSamples > 0)
			{
				auto fb = buffer->getFirstXYZData();

				sampleRate = fb->sampleRate;
				numChannels = fb->buffer.getNumChannels();
			}
			else
			{
				sampleRate = 44100.0;
				numChannels = 0;
			}
		}
		else
		{
			data = buffer->getDataPtrs();
			numChannels = buffer->getBuffer().getNumChannels();
			numSamples = buffer->getCurrentRange().getLength();
			sampleRate = buffer->sampleRate;
		}
		
		break;
	}
	case DataType::Table:
	{
		auto t = dynamic_cast<Table*>(obj);
		data = const_cast<float*>(t->getReadPointer());
		numSamples = t->getTableSize();
		numChannels = 1;
		break;
	}
	case DataType::SliderPack:
	{
		auto t = dynamic_cast<SliderPackData*>(obj);
		data = const_cast<float*>(t->getCachedData());
		numSamples = t->getNumSliders();
		numChannels = 1;
		break;
	}
	case DataType::FilterCoefficients:
	{
		data = nullptr;
		numSamples = 0;
		numChannels = 0;
		break;
	}
	case DataType::DisplayBuffer:
	{
		auto rb = dynamic_cast<SimpleRingBuffer*>(obj);
		data = rb->getWriteBuffer().getArrayOfWritePointers();
		numSamples = rb->getWriteBuffer().getNumSamples();
		numChannels = rb->getWriteBuffer().getNumChannels();
		break;
	}
    default: break;
	}
}

String ExternalData::getDataTypeName(DataType t, bool plural)
{
	switch (t)
	{
	case DataType::AudioFile: return plural ? "AudioFiles" : "AudioFile";
	case DataType::SliderPack: return plural ? "SliderPacks" : "SliderPack";
	case DataType::Table: return plural ? "Tables" : "Table";
	case DataType::FilterCoefficients: return plural ? "Filters" : "Filter";
	case DataType::DisplayBuffer: return plural ? "DisplayBuffers" : "DisplayBuffer";
	case DataType::ConstantLookUp: return "ConstantLookup";
	default: jassertfalse; return {};
	}
}

juce::Identifier ExternalData::getNumIdentifier(DataType t)
{
	String s;

	s << "Num";
	s << getDataTypeName(t, true);

	return Identifier(s);
}

void ExternalData::forEachType(const std::function<void(DataType)>& f)
{
	for (int i = 0; i < (int)DataType::numDataTypes; i++)
		f((DataType)i);
}

void ExternalData::referBlockTo(block& b, int channelIndex) const
{
	if(isEmpty())
	{
		b.referToNothing();
		return;
	}

	if (dataType == DataType::AudioFile || dataType == DataType::DisplayBuffer)
	{
		channelIndex = jmin(channelIndex, numChannels-1);

		if (numSamples > 0)
		{
			b.referToRawData(((float**)data)[channelIndex], numSamples);
		}
		else
			b.referToNothing();
	}
	else
		b.referToRawData((float*)data, numSamples);
}

void ExternalData::setDisplayedValue(double valueToDisplay)
{
	if (obj != nullptr)
	{
		obj->sendDisplayIndexMessage(valueToDisplay);
	}
}

juce::AudioSampleBuffer ExternalData::toAudioSampleBuffer() const
{
	if (isEmpty() || dataType == DataType::FilterCoefficients)
		return {};

	if (dataType == DataType::AudioFile || dataType == DataType::DisplayBuffer)
		return AudioSampleBuffer((float**)data, numChannels, numSamples);
	else
		return AudioSampleBuffer((float**)&data, 1, numSamples);
}

snex::ExternalData::DataType ExternalData::getDataTypeForId(const Identifier& id, bool plural/*=false*/)
{
	for (int i = 0; i < (int)DataType::numDataTypes; i++)
	{
		Identifier a(getDataTypeName((DataType)i, plural));

		if (a == id)
			return (DataType)i;
	}

	jassertfalse;
	return DataType::numDataTypes;
}

snex::ExternalData::DataType ExternalData::getDataTypeForClass(ComplexDataUIBase* d)
{
	if (auto s = dynamic_cast<SliderPackData*>(d))
		return DataType::SliderPack;
	if (auto t = dynamic_cast<Table*>(d))
		return DataType::Table;
	if (auto f = dynamic_cast<MultiChannelAudioBuffer*>(d))
		return DataType::AudioFile;
	if (auto f = dynamic_cast<FilterDataObject*>(d))
		return DataType::FilterCoefficients;
	if (auto f = dynamic_cast<SimpleRingBuffer*>(d))
		return DataType::DisplayBuffer;

	return DataType::numDataTypes;
}

hise::ComplexDataUIBase::EditorBase* ExternalData::createEditor(ComplexDataUIBase* dataObject)
{
	hise::ComplexDataUIBase::EditorBase* c = nullptr;

	if (auto t = dynamic_cast<hise::Table*>(dataObject))
	{
		c = new hise::TableEditor();
	}
	else if (auto t = dynamic_cast<hise::SliderPackData*>(dataObject))
	{
		c = new hise::SliderPack();
	}
	else if (auto t = dynamic_cast<hise::MultiChannelAudioBuffer*>(dataObject))
	{
		auto availableProviders = t->getAvailableXYZProviders();

		if (availableProviders.size() != 1)
			c = new hise::XYZMultiChannelAudioBufferEditor();
		else
			c = new hise::MultiChannelAudioBufferDisplay();
	}
	else if (auto t = dynamic_cast<hise::FilterDataObject*>(dataObject))
	{
		c = new hise::FilterGraph(0);
	}
	else if (auto rb = dynamic_cast<hise::SimpleRingBuffer*>(dataObject))
	{
		c = rb->getPropertyObject()->createComponent();
	}

	if(c != nullptr)
		c->setComplexDataUIBase(dataObject);

	return c;
}

juce::String InitialiserList::toString() const
{
	juce::String s;
	s << "{ ";

	for (auto l : root)
	{
		s << l->toString();

		if (root.getLast().get() != l)
			s << ", ";
	}

	s << " }";
	return s;
}



juce::Result InitialiserList::getValue(int index, VariableStorage& v)
{
	if (auto child = root[index])
	{
		try
		{
			if (child->getValue(v))
				return Result::ok();
			else
				return Result::fail("Can't resolve value at index " + juce::String(index));
		}
		catch (...)
		{
			return Result::fail("Expression can't be evaluated");
		}

	}
	else
		return Result::fail("Can't find item at index " + juce::String(index));
}

namespace external_data_icons
{
	static const unsigned char audiofile[] = { 110,109,233,230,38,67,0,0,0,0,98,201,22,49,67,0,0,0,0,231,91,57,67,4,86,4,65,231,91,57,67,240,167,147,65,108,231,91,57,67,238,28,38,67,98,231,91,57,67,205,76,48,67,201,22,49,67,236,145,56,67,233,230,38,67,236,145,56,67,108,240,167,147,65,236,145,56,67,
98,236,81,4,65,236,145,56,67,0,0,0,0,205,76,48,67,0,0,0,0,238,28,38,67,108,0,0,0,0,240,167,147,65,98,0,0,0,0,4,86,4,65,236,81,4,65,0,0,0,0,240,167,147,65,0,0,0,0,108,233,230,38,67,0,0,0,0,99,109,113,189,34,66,27,47,60,66,108,113,189,34,66,25,4,183,65,
108,242,210,183,65,25,4,183,65,108,242,210,183,65,37,134,181,66,108,141,151,129,65,37,134,181,66,98,137,65,88,65,37,134,181,66,115,104,53,65,72,225,185,66,115,104,53,65,250,62,191,66,108,115,104,53,65,35,27,196,66,98,115,104,53,65,213,120,201,66,137,
65,88,65,248,211,205,66,141,151,129,65,248,211,205,66,108,242,210,183,65,248,211,205,66,108,242,210,183,65,117,83,41,67,108,113,189,34,66,117,83,41,67,108,113,189,34,66,49,40,17,67,108,104,17,104,66,49,40,17,67,108,104,17,104,66,207,55,10,67,108,176,
178,150,66,207,55,10,67,108,176,178,150,66,244,61,247,66,108,172,92,185,66,244,61,247,66,108,172,92,185,66,31,69,224,66,108,37,70,219,66,31,69,224,66,108,37,70,219,66,221,228,255,66,108,33,240,253,66,221,228,255,66,108,33,240,253,66,61,42,26,67,108,80,
173,16,67,61,42,26,67,108,80,173,16,67,0,64,241,66,108,78,2,34,67,0,64,241,66,108,78,2,34,67,248,211,205,66,108,98,48,40,67,248,211,205,66,98,59,223,42,67,248,211,205,66,205,12,45,67,213,120,201,66,205,12,45,67,35,27,196,66,108,205,12,45,67,250,62,191,
66,98,205,12,45,67,203,225,185,66,125,223,42,67,168,134,181,66,98,48,40,67,37,134,181,66,108,78,2,34,67,37,134,181,66,108,78,2,34,67,115,40,143,66,108,80,173,16,67,115,40,143,66,108,80,173,16,67,240,39,24,66,108,33,240,253,66,240,39,24,66,108,33,240,
253,66,18,131,128,66,108,37,70,219,66,18,131,128,66,108,37,70,219,66,84,35,160,66,108,172,92,185,66,84,35,160,66,108,172,92,185,66,127,42,137,66,108,176,178,150,66,127,42,137,66,108,176,178,150,66,170,241,87,66,108,104,17,104,66,170,241,87,66,108,104,
17,104,66,27,47,60,66,108,113,189,34,66,27,47,60,66,99,101,0,0 };

	static const unsigned char sliderpack[] = { 110,109,250,190,39,67,0,0,0,0,98,231,251,49,67,0,0,0,0,199,75,58,67,244,253,4,65,199,75,58,67,102,102,148,65,108,199,75,58,67,248,243,38,67,98,199,75,58,67,229,48,49,67,231,251,49,67,197,128,57,67,250,190,39,67,197,128,57,67,108,102,102,148,65,197,128,
57,67,98,244,253,4,65,197,128,57,67,0,0,0,0,229,48,49,67,0,0,0,0,248,243,38,67,108,0,0,0,0,102,102,148,65,98,0,0,0,0,244,253,4,65,244,253,4,65,0,0,0,0,102,102,148,65,0,0,0,0,108,250,190,39,67,0,0,0,0,99,109,106,188,60,66,16,88,129,65,108,29,90,180,65,
16,88,129,65,98,131,192,145,65,16,88,129,65,139,108,107,65,90,100,157,65,139,108,107,65,244,253,191,65,98,139,108,107,65,182,115,72,66,139,108,107,65,29,26,6,67,139,108,107,65,31,69,33,67,98,139,108,107,65,66,0,38,67,78,98,148,65,4,214,41,67,100,59,186,
65,4,214,41,67,98,147,152,3,66,4,214,41,67,106,188,60,66,4,214,41,67,106,188,60,66,4,214,41,67,108,106,188,60,66,16,88,129,65,99,109,174,71,2,67,4,214,41,67,108,174,71,2,67,127,106,223,66,108,53,158,195,66,127,106,223,66,108,53,158,195,66,4,214,41,67,
108,174,71,2,67,4,214,41,67,99,109,201,118,177,66,4,214,41,67,108,201,118,177,66,133,171,7,67,108,68,11,97,66,133,171,7,67,108,68,11,97,66,4,214,41,67,108,201,118,177,66,4,214,41,67,99,109,100,155,35,67,4,214,41,67,98,186,201,37,67,195,213,41,67,197,
224,39,67,16,248,40,67,133,107,41,67,145,109,39,67,98,4,246,42,67,18,227,37,67,182,211,43,67,199,203,35,67,182,211,43,67,178,157,33,67,108,182,211,43,67,227,165,90,66,108,35,91,11,67,227,165,90,66,108,35,91,11,67,4,214,41,67,108,100,155,35,67,4,214,41,
67,99,101,0,0 };

	static const unsigned char table[] = { 110,109,195,245,35,67,0,0,0,0,98,207,247,45,67,0,0,0,0,141,23,54,67,0,0,2,65,141,23,54,67,86,14,145,65,108,141,23,54,67,92,47,35,67,98,141,23,54,67,39,49,45,67,141,247,45,67,39,81,53,67,195,245,35,67,39,81,53,67,108,86,14,145,65,39,81,53,67,98,0,0,2,
65,39,81,53,67,0,0,0,0,39,49,45,67,0,0,0,0,92,47,35,67,108,0,0,0,0,86,14,145,65,98,0,0,0,0,0,0,2,65,231,251,1,65,0,0,0,0,86,14,145,65,0,0,0,0,108,195,245,35,67,0,0,0,0,99,109,82,152,28,67,55,9,122,66,98,199,75,35,67,55,9,122,66,106,188,40,67,168,70,100,
66,106,188,40,67,213,120,73,66,108,106,188,40,67,33,176,208,65,98,106,188,40,67,123,20,155,65,199,75,35,67,184,30,95,65,82,152,28,67,184,30,95,65,108,33,80,4,67,184,30,95,65,98,213,56,251,66,184,30,95,65,141,87,240,66,123,20,155,65,141,87,240,66,33,176,
208,65,108,141,87,240,66,238,124,26,66,108,166,27,198,66,207,247,53,66,98,166,27,195,66,195,245,33,66,111,210,185,66,47,93,19,66,147,216,174,66,47,93,19,66,108,98,144,124,66,47,93,19,66,98,143,194,97,66,47,93,19,66,0,0,76,66,190,31,41,66,0,0,76,66,145,
237,67,66,108,0,0,76,66,174,135,146,66,98,0,0,76,66,102,230,155,66,215,163,86,66,61,10,164,66,63,53,102,66,4,22,168,66,108,20,46,36,66,66,224,242,66,108,80,141,198,65,66,224,242,66,98,170,241,144,65,66,224,242,66,23,217,74,65,137,193,253,66,23,217,74,
65,57,148,5,67,108,23,217,74,65,106,220,29,67,98,23,217,74,65,223,143,36,67,182,243,144,65,131,0,42,67,80,141,198,65,131,0,42,67,108,115,104,68,66,131,0,42,67,98,70,54,95,66,131,0,42,67,213,248,116,66,223,143,36,67,213,248,116,66,106,220,29,67,108,213,
248,116,66,57,148,5,67,98,213,248,116,66,82,56,1,67,150,195,107,66,61,202,250,66,182,243,93,66,137,129,246,66,108,84,99,144,66,223,207,170,66,108,147,216,174,66,223,207,170,66,98,0,64,188,66,92,207,170,66,197,32,199,66,20,238,159,66,197,32,199,66,174,
135,146,66,108,197,32,199,66,201,246,111,66,108,125,255,240,66,82,184,84,66,98,186,137,243,66,166,27,106,66,115,40,253,66,55,9,122,66,33,80,4,67,55,9,122,66,108,82,152,28,67,55,9,122,66,99,101,0,0 };

	static const unsigned char filter[] = { 110,109,123,116,69,67,2,43,7,62,98,213,152,90,67,47,221,164,62,12,34,110,67,137,65,151,65,207,87,110,67,213,120,39,66,98,215,131,110,67,195,53,190,66,215,131,110,67,16,88,20,67,207,87,110,67,188,148,73,67,98,221,36,110,67,127,74,95,67,102,230,91,67,76,
23,115,67,10,119,69,67,39,81,115,67,98,33,80,17,67,53,126,115,67,104,81,186,66,53,126,115,67,37,6,36,66,39,81,115,67,98,55,137,159,65,160,26,115,67,160,26,175,62,164,208,96,67,147,24,4,62,188,148,73,67,98,197,32,48,189,16,88,20,67,197,32,48,189,195,53,
190,66,147,24,4,62,213,120,39,66,98,139,108,167,62,86,14,162,65,141,151,148,65,250,126,170,62,104,17,36,66,2,43,7,62,98,117,83,186,66,88,57,52,189,27,79,17,67,88,57,52,189,123,116,69,67,2,43,7,62,99,109,154,25,37,66,18,131,157,65,98,186,73,239,65,205,
204,157,65,63,53,155,65,8,172,237,65,199,75,154,65,201,246,39,66,98,207,247,152,65,215,99,190,66,4,86,150,65,168,102,20,67,211,77,154,65,29,154,73,67,98,94,186,155,65,70,22,85,67,246,40,233,65,39,145,95,67,201,246,35,66,78,194,95,67,98,211,77,186,66,
215,67,96,67,236,81,17,67,215,67,96,67,35,123,69,67,78,194,95,67,98,35,187,80,67,117,147,95,67,250,254,90,67,45,178,85,67,27,47,91,67,29,154,73,67,98,20,174,91,67,94,90,20,67,20,174,91,67,39,49,190,66,27,47,91,67,84,99,39,66,98,72,1,91,67,96,229,242,
65,182,83,81,67,86,14,159,65,35,123,69,67,31,133,157,65,98,61,106,17,67,225,122,153,65,33,176,186,66,18,131,157,65,154,25,37,66,18,131,157,65,99,109,59,223,31,66,12,194,237,66,108,84,163,195,66,12,194,237,66,98,84,163,195,66,12,194,237,66,12,194,210,
66,45,114,247,66,213,184,242,66,219,249,178,66,98,166,123,1,67,20,46,144,66,164,16,11,67,104,209,136,66,70,246,22,67,31,69,182,66,98,137,97,39,67,6,1,245,66,168,230,62,67,37,102,46,67,221,196,71,67,127,42,66,67,98,211,141,72,67,68,235,67,67,227,101,72,
67,51,243,69,67,160,90,71,67,223,143,71,67,98,27,79,70,67,139,44,73,67,31,133,68,67,162,37,74,67,88,153,66,67,162,37,74,67,98,16,88,38,67,162,37,74,67,109,39,155,66,162,37,74,67,221,36,66,66,162,37,74,67,98,223,79,56,66,162,37,74,67,84,227,46,66,61,42,
73,67,207,247,39,66,68,107,71,67,98,68,11,33,66,139,172,69,67,51,51,29,66,27,79,67,67,186,73,29,66,219,217,64,67,98,68,11,30,66,137,33,43,67,59,223,31,66,12,194,237,66,59,223,31,66,12,194,237,66,99,101,0,0 };

	static const unsigned char buffer[] = { 110,109,219,153,60,67,236,81,184,61,98,221,4,81,67,207,247,147,62,166,219,99,67,125,63,141,65,248,51,100,67,100,187,29,66,108,188,52,100,67,94,186,30,66,98,143,98,100,67,113,253,177,66,143,98,100,67,217,78,10,67,188,52,100,67,184,158,59,67,98,209,2,100,
67,84,163,79,67,209,98,83,67,10,23,98,67,143,98,62,67,55,41,99,67,98,61,202,61,67,39,49,99,67,45,50,61,67,188,52,99,67,219,153,60,67,70,54,99,67,98,35,251,10,67,254,84,99,67,82,184,178,66,254,84,99,67,195,245,30,66,70,54,99,67,98,178,157,154,65,84,3,
99,67,125,63,245,62,233,102,81,67,186,73,12,62,238,220,59,67,108,113,61,10,62,238,156,59,67,98,236,81,56,189,20,78,10,67,236,81,56,189,119,254,177,66,113,61,10,62,137,193,30,66,98,84,227,165,62,219,249,151,65,203,161,143,65,209,34,155,62,195,245,30,66,
236,81,184,61,98,82,184,178,66,143,194,245,188,35,251,10,67,143,194,245,188,219,153,60,67,236,81,184,61,99,109,188,244,31,66,221,36,172,65,98,74,12,243,65,90,100,172,65,150,67,173,65,219,249,238,65,6,129,172,65,211,77,31,66,98,137,65,171,65,252,41,178,
66,156,196,168,65,76,87,10,67,18,131,172,65,82,152,59,67,98,82,184,173,65,76,23,69,67,248,83,239,65,98,176,77,67,225,122,31,66,115,200,77,67,98,209,226,178,66,229,240,77,67,156,4,11,67,6,65,78,67,201,150,60,67,49,200,77,67,98,195,21,70,67,203,161,77,
67,217,174,78,67,20,110,69,67,233,198,78,67,219,121,59,67,98,27,239,78,67,82,56,10,67,184,62,79,67,8,236,177,66,168,198,78,67,248,211,30,66,98,66,160,78,67,33,176,241,65,139,108,70,67,121,233,172,65,82,120,60,67,233,38,172,65,98,29,250,10,67,72,225,170,
65,76,247,178,66,221,36,172,65,188,244,31,66,221,36,172,65,99,109,244,253,162,66,211,205,53,66,98,82,248,184,66,100,187,54,66,121,105,202,66,115,104,94,66,125,191,212,66,174,7,128,66,98,104,17,246,66,150,67,182,66,170,49,255,66,84,99,248,66,174,135,13,
67,252,233,23,67,98,168,230,14,67,156,164,26,67,53,94,16,67,129,85,29,67,90,36,18,67,236,209,31,67,98,90,36,18,67,236,209,31,67,31,229,21,67,137,33,24,67,100,91,24,67,117,51,17,67,98,240,135,28,67,117,115,5,67,123,20,32,67,76,247,242,66,10,55,36,67,70,
118,219,66,108,109,167,36,67,231,251,216,66,108,29,122,40,67,129,149,195,66,108,240,167,60,67,131,0,210,66,98,166,187,53,67,143,194,248,66,141,119,48,67,55,137,16,67,88,89,40,67,223,47,35,67,98,2,43,35,67,10,23,47,67,246,200,25,67,125,255,60,67,150,3,
11,67,6,193,53,67,98,160,122,2,67,39,145,49,67,219,185,251,66,178,157,40,67,193,202,243,66,86,142,32,67,98,31,197,217,66,66,32,6,67,125,191,207,66,106,60,208,66,201,118,180,66,242,210,157,66,98,215,163,175,66,115,232,148,66,162,69,170,66,47,29,139,66,
168,198,162,66,82,56,134,66,98,168,198,162,66,82,56,134,66,201,246,155,66,227,165,141,66,121,169,151,66,254,84,150,66,98,57,244,139,66,213,248,173,66,113,189,133,66,205,76,200,66,246,40,129,66,231,251,225,66,98,246,40,129,66,231,251,225,66,115,232,122,
66,51,179,247,66,115,232,122,66,51,179,247,66,108,121,105,38,66,244,125,240,66,98,115,232,52,66,156,4,198,66,236,209,65,66,227,101,154,66,156,196,113,66,88,185,107,66,98,51,243,129,66,135,22,80,66,248,83,143,66,43,7,55,66,4,150,161,66,236,209,53,66,98,
61,74,162,66,205,204,53,66,186,73,162,66,205,204,53,66,244,253,162,66,211,205,53,66,99,101,0,0 };

static const unsigned char buffer_send[] = { 110,109,236,81,100,67,57,52,202,66,98,119,94,100,67,154,89,7,67,123,84,100,67,23,153,41,67,188,52,100,67,82,216,75,67,98,209,2,100,67,47,221,95,67,209,98,83,67,229,80,114,67,143,98,62,67,18,99,115,67,98,61,202,61,67,193,106,115,67,45,50,61,67,86,110,
115,67,219,153,60,67,223,111,115,67,98,35,251,10,67,217,142,115,67,82,184,178,66,217,142,115,67,195,245,30,66,223,111,115,67,98,178,157,154,65,47,61,115,67,125,63,245,62,131,160,97,67,186,73,12,62,135,22,76,67,108,113,61,10,62,201,214,75,67,98,236,81,
56,189,240,135,26,67,236,81,56,189,170,113,210,66,113,61,10,62,240,167,95,66,98,84,227,165,62,84,227,12,66,203,161,143,65,88,57,132,65,195,245,30,66,43,135,130,65,98,190,159,148,66,35,219,129,65,31,197,217,66,227,165,129,65,63,117,15,67,133,235,129,65,
108,180,232,36,67,150,195,22,66,98,186,137,246,66,43,135,22,66,12,66,163,66,219,249,22,66,188,244,31,66,219,249,22,66,98,74,12,243,65,154,25,23,66,150,67,173,65,90,100,56,66,6,129,172,65,57,52,96,66,98,137,65,171,65,47,157,210,66,156,196,168,65,229,144,
26,67,18,131,172,65,45,210,75,67,98,82,184,173,65,229,80,85,67,248,83,239,65,252,233,93,67,225,122,31,66,78,2,94,67,98,209,226,178,66,127,42,94,67,156,4,11,67,160,122,94,67,201,150,60,67,12,2,94,67,98,195,21,70,67,100,219,93,67,217,174,78,67,174,167,
85,67,233,198,78,67,182,179,75,67,98,180,232,78,67,197,96,34,67,37,38,79,67,29,26,242,66,45,242,78,67,188,116,159,66,108,236,81,100,67,57,52,202,66,99,109,244,253,162,66,57,180,118,66,98,82,248,184,66,209,162,119,66,121,105,202,66,109,167,143,66,125,
191,212,66,225,122,160,66,98,104,17,246,66,201,182,214,66,170,49,255,66,133,107,12,67,174,135,13,67,215,35,40,67,98,168,230,14,67,53,222,42,67,53,94,16,67,27,143,45,67,90,36,18,67,199,11,48,67,98,90,36,18,67,199,11,48,67,31,229,21,67,35,91,40,67,100,
91,24,67,14,109,33,67,98,240,135,28,67,14,173,21,67,123,20,32,67,129,181,9,67,10,55,36,67,121,233,251,66,108,109,167,36,67,158,111,249,66,108,29,122,40,67,55,9,228,66,108,240,167,60,67,57,116,242,66,98,166,187,53,67,225,154,12,67,141,119,48,67,18,195,
32,67,88,89,40,67,186,105,51,67,98,2,43,35,67,164,80,63,67,246,200,25,67,88,57,77,67,150,3,11,67,160,250,69,67,98,160,122,2,67,2,203,65,67,219,185,251,66,76,215,56,67,193,202,243,66,49,200,48,67,98,31,197,217,66,219,89,22,67,125,191,207,66,158,175,240,
66,201,118,180,66,37,70,190,66,98,215,163,175,66,166,91,181,66,162,69,170,66,229,144,171,66,168,198,162,66,8,172,166,66,98,168,198,162,66,8,172,166,66,201,246,155,66,23,25,174,66,121,169,151,66,180,200,182,66,98,57,244,139,66,8,108,206,66,113,189,133,
66,0,192,232,66,246,40,129,66,207,55,1,67,98,246,40,129,66,207,55,1,67,115,232,122,66,117,19,12,67,115,232,122,66,117,19,12,67,108,121,105,38,66,213,120,8,67,98,115,232,52,66,207,119,230,66,236,209,65,66,154,217,186,66,156,196,113,66,223,79,150,66,98,
51,243,129,66,250,126,136,66,248,83,143,66,145,237,119,66,4,150,161,66,88,185,118,66,98,61,74,162,66,51,179,118,66,186,73,162,66,51,179,118,66,244,253,162,66,57,180,118,66,99,109,182,115,60,67,127,106,22,66,108,72,225,22,67,111,18,3,61,108,61,74,121,
67,0,0,0,0,108,12,66,121,67,236,209,196,66,108,29,58,83,67,25,132,113,66,108,10,247,34,67,49,72,217,66,108,164,48,12,67,100,187,171,66,108,182,115,60,67,127,106,22,66,99,101,0,0 };

static const unsigned char audiofile_send[] = { 110,109,233,230,38,67,29,122,88,67,108,240,167,147,65,29,122,88,67,98,236,81,4,65,29,122,88,67,0,0,0,0,188,52,80,67,0,0,0,0,31,5,70,67,108,0,0,0,0,182,115,73,66,98,0,0,0,0,63,181,32,66,236,81,4,65,137,65,255,65,240,167,147,65,125,63,255,65,108,59,95,
212,66,125,63,255,65,108,231,91,57,67,115,40,222,66,108,231,91,57,67,31,5,70,67,98,231,91,57,67,188,52,80,67,10,23,49,67,219,121,88,67,233,230,38,67,29,122,88,67,99,109,113,189,34,66,203,33,91,66,108,242,210,183,65,203,33,91,66,108,242,210,183,65,135,
86,245,66,108,115,104,53,65,135,86,245,66,108,115,104,53,65,236,209,6,67,108,242,210,183,65,236,209,6,67,108,242,210,183,65,166,59,73,67,108,113,189,34,66,166,59,73,67,108,113,189,34,66,98,16,49,67,108,104,17,104,66,98,16,49,67,108,104,17,104,66,190,
31,42,67,108,176,178,150,66,190,31,42,67,108,176,178,150,66,233,134,27,67,108,172,92,185,66,233,134,27,67,108,172,92,185,66,127,10,16,67,108,37,70,219,66,127,10,16,67,108,37,70,219,66,94,218,31,67,108,33,240,253,66,94,218,31,67,108,33,240,253,66,61,10,
43,67,108,80,173,16,67,61,10,43,67,108,80,173,16,67,240,135,24,67,108,78,2,34,67,240,135,24,67,108,78,2,34,67,236,209,6,67,108,205,12,45,67,236,209,6,67,108,205,12,45,67,135,86,245,66,108,78,2,34,67,135,86,245,66,108,78,2,34,67,82,248,206,66,108,80,173,
16,67,82,248,206,66,108,80,173,16,67,182,243,169,66,108,33,240,253,66,182,243,169,66,108,33,240,253,66,242,82,192,66,108,37,70,219,66,242,82,192,66,108,37,70,219,66,51,243,223,66,108,172,92,185,66,51,243,223,66,108,172,92,185,66,94,250,200,66,108,176,
178,150,66,94,250,200,66,108,176,178,150,66,180,200,171,66,108,104,17,104,66,180,200,171,66,108,104,17,104,66,109,231,157,66,108,113,189,34,66,109,231,157,66,108,113,189,34,66,203,33,91,66,99,109,211,141,16,67,246,168,83,66,108,178,221,31,67,127,106,
22,66,108,4,150,244,66,111,18,3,61,108,248,179,92,67,0,0,0,0,108,8,172,92,67,236,209,196,66,108,25,164,54,67,25,132,113,66,108,57,84,39,67,203,97,151,66,108,211,141,16,67,246,168,83,66,99,101,0,0 };

static const unsigned char filter_send[] = { 110,109,211,109,110,67,168,198,196,66,98,203,129,110,67,66,64,12,67,94,122,110,67,113,29,54,67,207,87,110,67,94,250,95,67,98,221,36,110,67,33,176,117,67,102,230,91,67,86,190,132,67,10,119,69,67,100,219,132,67,98,33,80,17,67,236,241,132,67,104,81,186,
66,236,241,132,67,37,6,36,66,100,219,132,67,98,55,137,159,65,33,192,132,67,160,26,175,62,70,54,119,67,147,24,4,62,94,250,95,67,98,197,32,48,189,178,189,42,67,197,32,48,189,6,1,235,66,147,24,4,62,174,135,128,66,98,139,108,167,62,178,157,42,66,141,151,
148,65,10,215,181,65,104,17,36,66,88,57,180,65,98,12,2,163,66,209,34,179,65,231,251,243,66,96,229,178,65,160,122,34,67,6,129,179,65,108,203,1,54,67,47,221,39,66,98,23,25,7,67,63,181,38,66,184,94,176,66,10,87,40,66,154,25,37,66,10,87,40,66,98,186,73,239,
65,238,124,40,66,63,53,155,65,139,108,80,66,199,75,154,65,168,198,128,66,98,207,247,152,65,27,47,235,66,4,86,150,65,74,204,42,67,211,77,154,65,190,255,95,67,98,94,186,155,65,231,123,107,67,246,40,233,65,201,246,117,67,201,246,35,66,174,39,118,67,98,211,
77,186,66,121,169,118,67,236,81,17,67,121,169,118,67,35,123,69,67,174,39,118,67,98,35,187,80,67,23,249,117,67,250,254,90,67,141,23,108,67,27,47,91,67,190,255,95,67,98,12,162,91,67,12,194,47,67,14,173,91,67,162,5,255,66,158,79,91,67,186,137,158,66,108,
211,109,110,67,168,198,196,66,99,109,59,223,31,66,168,70,13,67,108,84,163,195,66,168,70,13,67,98,84,163,195,66,168,70,13,67,12,194,210,66,119,30,18,67,213,184,242,66,31,197,223,66,98,166,123,1,67,88,249,188,66,164,16,11,67,172,156,181,66,70,246,22,67,
98,16,227,66,98,137,97,39,67,37,230,16,67,168,230,62,67,199,203,68,67,221,196,71,67,33,144,88,67,98,211,141,72,67,229,80,90,67,227,101,72,67,213,88,92,67,160,90,71,67,129,245,93,67,98,27,79,70,67,236,145,95,67,31,133,68,67,68,139,96,67,88,153,66,67,68,
139,96,67,98,16,88,38,67,68,139,96,67,109,39,155,66,68,139,96,67,221,36,66,66,68,139,96,67,98,223,79,56,66,68,139,96,67,84,227,46,66,223,143,95,67,207,247,39,66,229,208,93,67,98,68,11,33,66,45,18,92,67,51,51,29,66,188,180,89,67,186,73,29,66,125,63,87,
67,98,68,11,30,66,43,135,65,67,59,223,31,66,168,70,13,67,59,223,31,66,168,70,13,67,99,109,35,123,71,67,127,106,22,66,108,180,232,33,67,111,18,3,61,108,213,40,130,67,0,0,0,0,108,188,36,130,67,236,209,196,66,108,137,65,94,67,25,132,113,66,108,119,254,45,
67,49,72,217,66,108,16,56,23,67,100,187,171,66,108,35,123,71,67,127,106,22,66,99,101,0,0 };

static const unsigned char sliderpack_send[] = { 110,109,250,190,39,67,180,72,83,67,108,102,102,148,65,180,72,83,67,98,244,253,4,65,180,72,83,67,0,0,0,0,213,248,74,67,0,0,0,0,231,187,64,67,108,0,0,0,0,242,82,49,66,98,0,0,0,0,59,95,8,66,244,253,4,65,125,63,206,65,102,102,148,65,125,63,206,65,108,223,
79,173,66,125,63,206,65,108,199,75,58,67,141,215,250,66,108,199,75,58,67,231,187,64,67,98,199,75,58,67,213,248,74,67,41,252,49,67,115,72,83,67,250,190,39,67,180,72,83,67,99,109,106,188,60,66,199,203,39,66,108,29,90,180,65,199,203,39,66,98,131,192,145,
65,199,203,39,66,139,108,107,65,236,209,53,66,139,108,107,65,184,30,71,66,98,139,108,107,65,186,201,151,66,139,108,107,65,12,226,31,67,139,108,107,65,14,13,59,67,98,139,108,107,65,49,200,63,67,78,98,148,65,244,157,67,67,100,59,186,65,244,157,67,67,98,
147,152,3,66,244,157,67,67,106,188,60,66,244,157,67,67,106,188,60,66,244,157,67,67,108,106,188,60,66,199,203,39,66,99,109,182,211,43,67,168,198,252,66,108,35,91,11,67,168,198,252,66,108,35,91,11,67,244,157,67,67,108,100,155,35,67,244,157,67,67,98,162,
37,40,67,178,157,67,67,182,211,43,67,158,239,63,67,182,211,43,67,162,101,59,67,108,182,211,43,67,168,198,252,66,99,109,174,71,2,67,244,125,186,66,108,53,158,195,66,244,125,186,66,108,53,158,195,66,244,157,67,67,108,174,71,2,67,244,157,67,67,108,174,71,
2,67,244,125,186,66,99,109,201,118,177,66,117,115,33,67,108,68,11,97,66,117,115,33,67,108,68,11,97,66,244,157,67,67,108,201,118,177,66,244,157,67,67,108,201,118,177,66,117,115,33,67,99,109,193,74,4,67,246,168,83,66,108,160,154,19,67,127,106,22,66,108,
98,16,220,66,111,18,3,61,108,39,113,80,67,0,0,0,0,108,246,104,80,67,236,209,196,66,108,6,97,42,67,25,132,113,66,108,104,17,27,67,203,97,151,66,108,193,74,4,67,246,168,83,66,99,101,0,0 };

static const unsigned char table_send[] = { 110,109,195,245,35,67,209,194,76,67,108,86,14,145,65,209,194,76,67,98,231,251,1,65,143,194,76,67,0,0,0,0,209,162,68,67,0,0,0,0,6,161,58,67,108,0,0,0,0,211,77,38,66,98,0,0,0,0,80,141,252,65,231,251,1,65,92,143,187,65,86,14,145,65,80,141,187,65,108,131,
0,169,66,80,141,187,65,108,141,23,54,67,104,17,242,66,108,141,23,54,67,6,161,58,67,98,141,23,54,67,209,162,68,67,207,247,45,67,143,194,76,67,195,245,35,67,209,194,76,67,99,109,115,104,68,66,55,9,11,66,108,80,141,198,65,55,9,11,66,98,170,241,144,65,55,
9,11,66,23,217,74,65,199,203,32,66,23,217,74,65,154,153,59,66,108,23,217,74,65,47,93,142,66,98,23,217,74,65,25,196,155,66,170,241,144,65,96,165,166,66,80,141,198,65,96,165,166,66,108,20,46,36,66,96,165,166,66,108,63,53,102,66,158,111,241,66,98,215,163,
86,66,100,123,245,66,0,0,76,66,184,158,253,66,0,0,76,66,250,126,3,67,108,0,0,76,66,109,199,27,67,98,0,0,76,66,160,122,34,67,143,194,97,66,133,235,39,67,98,144,124,66,133,235,39,67,108,147,216,174,66,133,235,39,67,98,242,210,185,66,68,235,39,67,166,27,
195,66,96,69,36,67,166,27,198,66,221,68,31,67,108,141,87,240,66,150,35,38,67,108,141,87,240,66,205,172,50,67,98,141,87,240,66,66,96,57,67,213,56,251,66,229,208,62,67,33,80,4,67,229,208,62,67,108,82,152,28,67,229,208,62,67,98,199,75,35,67,229,208,62,67,
106,188,40,67,66,96,57,67,106,188,40,67,205,172,50,67,108,106,188,40,67,156,100,26,67,98,106,188,40,67,104,177,19,67,8,76,35,67,197,64,14,67,82,152,28,67,131,64,14,67,108,33,80,4,67,131,64,14,67,98,115,40,253,66,131,64,14,67,186,137,243,66,231,59,18,
67,125,255,240,66,188,148,23,67,108,197,32,199,66,31,197,16,67,108,197,32,199,66,250,126,3,67,98,197,32,199,66,141,151,249,66,0,64,188,66,70,182,238,66,147,216,174,66,195,181,238,66,108,84,99,144,66,195,181,238,66,108,182,243,93,66,25,4,163,66,98,150,
195,107,66,100,187,158,66,213,248,116,66,254,20,151,66,213,248,116,66,47,93,142,66,108,213,248,116,66,154,153,59,66,98,213,248,116,66,205,204,32,66,76,55,95,66,61,10,11,66,115,104,68,66,55,9,11,66,99,109,88,89,3,67,246,168,83,66,108,55,169,18,67,127,
106,22,66,108,145,45,218,66,111,18,3,61,108,190,127,79,67,0,0,0,0,108,141,119,79,67,236,209,196,66,108,158,111,41,67,25,132,113,66,108,0,32,26,67,203,97,151,66,108,88,89,3,67,246,168,83,66,99,101,0,0 };
}

juce::Path ExternalData::Factory::createPath(const String& url) const
{
	Path p;

	LOAD_PATH_IF_URL(getDataTypeName(DataType::Table, false).toLowerCase(), external_data_icons::table_send);
	LOAD_PATH_IF_URL(getDataTypeName(DataType::SliderPack, false).toLowerCase(), external_data_icons::sliderpack_send);
	LOAD_PATH_IF_URL(getDataTypeName(DataType::AudioFile, false).toLowerCase(), external_data_icons::audiofile_send);
	LOAD_PATH_IF_URL(getDataTypeName(DataType::FilterCoefficients, false).toLowerCase(), external_data_icons::filter_send);
	LOAD_PATH_IF_URL(getDataTypeName(DataType::DisplayBuffer, false).toLowerCase(), external_data_icons::buffer_send);

	return p;
}

}
