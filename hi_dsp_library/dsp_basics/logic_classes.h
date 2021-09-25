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
 *   which also must be licenced for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace scriptnode { using namespace juce;


/** Contains all different MIDI processing units. */
namespace midi_logic
{

template <int Unused> struct gate
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_INITIALISE;

    bool getMidiValue(HiseEvent& e, double& v)
    {
        if (e.isNoteOnOrOff())
        {
            v = (double)e.isNoteOn();
            return true;
        }

        return false;
    }
};

template <int Unused> struct velocity
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_INITIALISE;

    bool getMidiValue(HiseEvent& e, double& v)
    {
        if (e.isNoteOn())
        {
            v = e.getFloatVelocity();
            return true;
        }

        return false;
    }
};

template <int Unused> struct notenumber
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_INITIALISE;

    bool getMidiValue(HiseEvent& e, double& v)
    {
        if (e.isNoteOn())
        {
            v = (double)e.getNoteNumber() / 127.0;
            return true;
        }

        return false;
    }
};

template <int Unused> struct frequency
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_INITIALISE;

    static constexpr bool IsProcessingHiseEvent() { return true; }

    bool getMidiValue(HiseEvent& e, double& v)
    {
        if (e.isNoteOn())
        {
            v = (e.getFrequency()) / 20000.0;
            return true;
        }

        return false;
    }
};
}

/** Contains all units for clone cable value calculations. */
namespace duplilogic
{

struct spread
{
    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        if (numUsed == 1)
            return 0.5;

        auto n = (double)index / (double)(numUsed - 1) - 0.5;
        
        if (gamma != 0.0)
        {
            auto gn = hmath::sin(double_Pi * n);
            gn *= 0.5;
            n = gn * gamma + n * (1.0 - gamma);
        }

        n *= inputValue;

        return n + 0.5;
    }
};

struct triangle
{
    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        if (numUsed == 1)
            return 1.0;

        auto n = (double)index / (double)(numUsed - 1);

        n = hmath::abs(n - 0.5) * 2.0;

        if (gamma != 0.0)
        {
            auto gn = hmath::sin(n * double_Pi * 0.5);
            gn *= gn;

            n = gamma * gn + (1.0 - gamma) * n;
        }

        return 1.0 - inputValue * n;
    }
};



struct harmonics: public midi_logic::frequency<0>
{
    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        return (double)(index + 1) * inputValue;
    }
};

struct nyquist: public midi_logic::frequency<0>
{
    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        auto hvalue = harmonics().getValue(index, numUsed, inputValue, gamma);
        return hmath::smoothstep(hvalue, 1.0, jmin(0.99, gamma));
    }
};

struct fixed: public midi_logic::frequency<0>
{
    double getValue(int /*index*/, int /*numUsed*/, double inputValue, double /*gamma*/)
    {
        return inputValue;
    }
};

struct ducker
{
    double getValue(int /*index*/, int numUsed, double /*inputValue*/, double gamma)
    {
        auto v = 1.0 / jmax(1.0, (double)numUsed);
        
        if (gamma != 0.0)
        {
            v = std::pow(v, 1.0 - gamma);
        }

        return v;
    }
};

struct random
{
    Random r;

	static constexpr bool IsProcessingHiseEvent() { return true; }

	bool getMidiValue(HiseEvent& e, double& v)
	{
		if (e.isNoteOn())
		{
			// don't change the value, just return true
			return true;
		}

		return false;
	}

    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        double n;

        if (numUsed == 1)
            n = 0.5f;
        else
            n = (double)index / (double)(numUsed - 1) ;

        return jlimit(0.0, 1.0, n + (2.0 * r.nextDouble() - 1.0) * inputValue);
    }
};

struct scale
{
    double getValue(int index, int numUsed, double inputValue, double gamma)
    {
        if (numUsed == 1)
            return inputValue;

        auto n = (double)index / (double)(numUsed - 1) * inputValue;

        if (gamma != 1.0)
            n = hmath::pow(n, 1.0 + gamma);

        return n;
    }
};

}

namespace file_analysers
{
struct pitch
{
    double getValue(const ExternalData& d)
    {
        if (d.numSamples > 0)
        {
            block b;
            d.referBlockTo(b, 0);

            return PitchDetection::detectPitch(b.begin(), b.size(), d.sampleRate);
        }

        return 0.0;
    }
};

struct milliseconds
{
    double getValue(const ExternalData& d)
    {
        if (d.numSamples > 0 && d.sampleRate > 0.0)
        {
            return 1000.0 * (double)d.numSamples / d.sampleRate;
        }

        return 0.0;
    }
};

struct peak
{
    double getValue(const ExternalData& d)
    {
        if (d.numSamples > 0)
        {
            auto b = d.toAudioSampleBuffer();
            return (double)b.getMagnitude(0, d.numSamples);
        }

        return 0.0;
    }
};
}

namespace timer_logic
{
template <int NV> struct ping
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_RESET;

    double getTimerValue() const { return 1.0; }
};

template <int NV> struct random
{
    HISE_EMPTY_PREPARE;
    HISE_EMPTY_RESET;

    double getTimerValue() const
    {
        return hmath::randomDouble();
    }
};

template <int NV> struct toggle
{
    PolyData<double, NV> state;

    double getTimerValue()
    {
        double v = 0.0;

        for (auto& s : state)
        {
            s = 1.0 - s;
            v = s;
        }
        
        return v;
    }

    void prepare(PrepareSpecs ps)
    {
        state.prepare(ps);
    }

    void reset()
    {
        for (auto& s : state)
            s = 0.0;
    }
};

}

namespace faders
{
struct switcher
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        auto numParameters = (double)(numElements);
        auto indexToActivate = jmin(numElements - 1, (int)(normalisedInput * numParameters));

        return (double)(indexToActivate == Index);
    }
};

struct overlap
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        if (isPositiveAndBelow(Index, numElements))
        {
            switch (numElements)
            {
            case 2:
            {
                switch (Index)
                {
                case 0: return jlimit(0.0, 1.0, 2.0 - 2.0 * normalisedInput);
                case 1: return jlimit(0.0, 1.0, 2.0 * normalisedInput);
                default: return 0.0;
                }
            }
            case 3:
            {
                switch (Index)
                {
                case 0: return jlimit(0.0, 1.0, 3.0 - 3.0 * normalisedInput);
                case 1: return jlimit(0.0, 1.0, 3.0 * normalisedInput);
                default: return 0.0;
                }

            }
            case 4:
            {
                if (Index != 1)
                    return 0.0;

                auto v = 2.0 - hmath::abs(-4.0 * (normalisedInput + 0.66));

                v = jmax(0.0, v - 1.0);

                return jlimit(0.0, 1.0, v);
            }
            }
        }

        return 0.0;
    }
};

struct harmonics
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        return normalisedInput * (double)(Index + 1);
    }
};

struct linear
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        if (numElements == 1)
            return 1.0 - normalisedInput;
        else
        {
            const double u = (double)numElements - 1.0;
            const double offset = (1.0 - (double)Index) / u;
            auto v = 1.0 - Math.abs(1.0 - u * (normalisedInput + offset));
            return jlimit(0.0, 1.0, v);
        }

        return 0.0;
    }

    hmath Math;
};

struct squared
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        auto v = lf.getFadeValue<Index>(numElements, normalisedInput);
        return v * v;
    }

    linear lf;
};

struct rms
{
    HISE_EMPTY_INITIALISE;

    template <int Index> double getFadeValue(int numElements, double normalisedInput)
    {
        auto v = lf.getFadeValue<Index>(numElements, normalisedInput);
        return hmath::sqrt(v);
    }

    linear lf;
};


}


namespace smoothers
{
struct base
{
    virtual ~base() {};
    
    void setSmoothingTime(double t)
    {
        if (smoothingTimeMs != t)
        {
            smoothingTimeMs = t;
            refreshSmoothingTime();
        }
    }

    virtual float get() const = 0;
    virtual void reset() = 0;
    virtual void set(double v) = 0;
    virtual float advance() = 0;

    virtual void prepare(PrepareSpecs ps)
    {
        currentBlockRate = ps.sampleRate / (double)ps.blockSize;
        refreshSmoothingTime();
    }

    virtual void refreshSmoothingTime() = 0;

    virtual void setEnabled(double v) = 0;

    virtual HISE_EMPTY_INITIALISE;

    double currentBlockRate = 0.0;
    double smoothingTimeMs = 0.0;
};

struct no : public base
{
    float get() const final override
    {
        return v;
    }

    void reset() final override {};

    void set(double nv) final override
    {
        v = nv;
    }

    void setEnabled(double v) final override { };

    float advance() final override
    {
        return v;
    }

    void refreshSmoothingTime() final override {};

    float v = 0.0f;
};

struct low_pass : public base
{
    float get() const final override
    {
        return lastValue;
    }

    void reset() final override
    {
        isSmoothing = false;
        lastValue = target;
        s.resetToValue(target);
    }

    float advance() final override
    {
        if (isSmoothing && enabled)
        {
            auto thisValue = s.smooth(target);
            isSmoothing = std::abs(thisValue - target) > 0.001f;
            lastValue = thisValue;
            return thisValue;
        }

        return target;
    }

    void set(double targetValue) final override
    {
        auto tf = (float)targetValue;

        if (tf != target)
        {
            target = tf;
            isSmoothing = target != lastValue;
        }
    }

    void refreshSmoothingTime() final override
    {
        s.prepareToPlay(currentBlockRate);
        s.setSmoothingTime(smoothingTimeMs);
    }

    void setEnabled(double v) final override
    {
        enabled = v > 0.5;

        if (enabled)
            reset();
    }

private:

    bool enabled = true;
    bool isSmoothing = false;
    float lastValue = 0.0f;
    float target = 0.0f;
    Smoother s;
};

struct linear_ramp : public base
{
    void reset()final override
    {
        d.reset();
    }

    float advance() final override
    {
        return enabled ? d.advance() : d.targetValue;
    }

    float get() const final override
    {
        return enabled ? d.get() : d.targetValue;
    }

    void set(double newValue) final override
    {
        d.set(newValue);
    }

    void refreshSmoothingTime() final override
    {
        d.prepare(currentBlockRate, smoothingTimeMs);
    }

    void setEnabled(double v) final override
    {
        enabled = v > 0.5;

        if (enabled)
            reset();
    }

private:

    sdouble d;
    bool enabled = true;
};
}


} 
