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
 *   which also must be licensed for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

namespace scriptnode
{
using namespace juce;
using namespace hise;
using namespace snex;

core::SnexOscillator::OscillatorCallbacks::OscillatorCallbacks(SnexSource& p, ObjectStorageType& o) :
	CallbackHandlerBase(p, o)
{

}

void core::SnexOscillator::OscillatorCallbacks::reset()
{
	SimpleReadWriteLock::ScopedWriteLock l(getAccessLock());
	ok = false;
	tickFunction = {};
	processFunction = {};
}

juce::Result core::SnexOscillator::OscillatorCallbacks::recompiledOk(snex::jit::ComplexType::Ptr objectClass)
{
	

	auto r = Result::ok();

	auto newTickFunction = getFunctionAsObjectCallback("tick");
	auto newProcessFunction = getFunctionAsObjectCallback("process");

	r = newTickFunction.validateWithArgs(Types::ID::Float, { Types::ID::Double });

	if (r.wasOk())
		r = newProcessFunction.validateWithArgs(Types::ID::Void, { Types::ID::Pointer });

	{
		SimpleReadWriteLock::ScopedWriteLock l(getAccessLock());
		ok = r.wasOk();
		std::swap(newTickFunction, tickFunction);
		std::swap(newProcessFunction, processFunction);
	}

	return r;
}

float core::SnexOscillator::OscillatorCallbacks::tick(double uptime)
{
	if (auto c = ScopedCallbackChecker(*this))
		return tickFunction.call<float>(uptime);

	return 0.0;
}

void core::SnexOscillator::OscillatorCallbacks::process(OscProcessData& d)
{
	if (auto c = ScopedCallbackChecker(*this))
		processFunction.callVoid(&d);
}

core::SnexOscillator::SnexOscillator() :
	SnexSource(),
	callbacks(*this, object)
{
	setCallbackHandler(&callbacks);
}

String core::SnexOscillator::getEmptyText(const Identifier& id) const
{
	using namespace snex::cppgen;

	cppgen::Base c(cppgen::Base::OutputType::AddTabs);
	cppgen::Struct s(c, id, {}, {});

	c.addComment("This function will be called once per sample", cppgen::Base::CommentType::Raw);
	c << "float tick(double uptime)\n";
	{
		StatementBlock sb(c);
		c << "return Math.fmod(uptime, 1.0);";
	}

	c.addEmptyLine();
	c.addComment("This function will calculate a chunk of samples", cppgen::Base::CommentType::Raw);
	c << "void process(OscProcessData& d)\n";
	{
		StatementBlock sb(c);
		c << "for (auto& s : d.data)";
		{
			StatementBlock sb2(c);
			c << "s = tick(d.uptime);";
			c << "d.uptime += d.delta;";
		}
	}

	String pf;

	c.addEmptyLine();
	addDefaultParameterFunction(pf);

	c << pf;

	s.flushIfNot();

	auto code = c.toString();



	return code;
}

void core::SnexOscillator::initialise(NodeBase* n)
{
	SnexSource::initialise(n);

}

float core::SnexOscillator::tick(double uptime)
{
	return callbacks.tick(uptime);
}

void core::SnexOscillator::process(OscProcessData& d)
{
	callbacks.process(d);
}

core::NewSnexOscillatorDisplay::NewSnexOscillatorDisplay(SnexOscillator* osc, PooledUIUpdater* updater) :
	ScriptnodeExtraComponent<SnexOscillator>(osc, updater),
	menuBar(osc)
{
	addAndMakeVisible(menuBar);
	setSize(512, 144);
	getObject()->addCompileListener(this);
}

core::NewSnexOscillatorDisplay::~NewSnexOscillatorDisplay()
{
	getObject()->removeCompileListener(this);
}

void core::NewSnexOscillatorDisplay::complexDataAdded(snex::ExternalData::DataType t, int index)
{
	rebuildPath = true;
}

void core::NewSnexOscillatorDisplay::parameterChanged(int snexParameterId, double newValue)
{
	rebuildPath = true;
}

void core::NewSnexOscillatorDisplay::complexDataTypeChanged()
{
	rebuildPath = true;
}

void core::NewSnexOscillatorDisplay::wasCompiled(bool ok)
{
	if (ok)
	{
		errorMessage = {};
		rebuildPath = true;
		repaint();
	}
	else
	{
		p = {};
		errorMessage = getObject()->getWorkbench()->getLastResult().compileResult.getErrorMessage();
		repaint();
	}
}

void core::NewSnexOscillatorDisplay::paint(Graphics& g)
{
	g.setColour(Colours::white.withAlpha(0.2f));
	g.drawRect(pathBounds, 1.0f);
	g.setFont(GLOBAL_BOLD_FONT());
	g.drawText("SNEX Oscillator", pathBounds.reduced(3.0f), Justification::topRight);

	if (errorMessage.isNotEmpty())
	{
		g.setColour(Colours::white.withAlpha(0.7f));
		g.setFont(GLOBAL_BOLD_FONT());
		g.drawText(errorMessage, pathBounds, Justification::centred);
	}
	else if (!p.getBounds().isEmpty())
	{

		GlobalHiseLookAndFeel::fillPathHiStyle(g, p, (int)pathBounds.getWidth(), (int)pathBounds.getHeight());
	}
}

void core::NewSnexOscillatorDisplay::timerCallback()
{
	if (rebuildPath)
	{
		if (!pathBounds.isEmpty())
		{
			buffer.setSize((int)pathBounds.getWidth());

			for (auto& s : buffer)
				s = 0.0f;

			od.data.referTo(buffer);
			od.uptime = 0.0;
			od.delta = 1.0 / (double)buffer.size();
			od.voiceIndex = 0;

			SnexOscillator::OscTester tester(*getObject());

			tester.callbacks.process(od);

			p.clear();
			p.startNewSubPath(0.0f, 0.0f);

			float i = 0.0f;

			for (auto& s : buffer)
			{
				FloatSanitizers::sanitizeFloatNumber(s);
				jlimit(-10.0f, 10.0f, s);
				p.lineTo(i, -1.0f * s);
				i += 1.0f;
			}

			p.lineTo(i - 1.0f, 0.0f);
			p.closeSubPath();

			if (p.getBounds().getHeight() > 0.0f && p.getBounds().getWidth() > 0.0f)
			{
				p.scaleToFit(pathBounds.getX(), pathBounds.getY(), pathBounds.getWidth(), pathBounds.getHeight(), false);
			}
		}

		rebuildPath = false;
		repaint();
	}
}

void core::NewSnexOscillatorDisplay::resized()
{
	auto t = getLocalBounds();
	menuBar.setBounds(t.removeFromTop(24));
	t.removeFromTop(20);

	pathBounds = t.reduced(2).toFloat();
}

Component* core::NewSnexOscillatorDisplay::createExtraComponent(void* obj, PooledUIUpdater* u)
{
	return new NewSnexOscillatorDisplay(static_cast<SnexOscillator*>(obj), u);
}

}

