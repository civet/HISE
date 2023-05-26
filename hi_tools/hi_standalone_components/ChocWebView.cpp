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


#include "choc/gui/choc_WebView.h"

namespace hise {
using namespace juce;

struct WebViewData::Pimpl
{
	Pimpl() = default;
	
	~Pimpl()
	{
		callbacks.clear();
		resources.clear();
	}

	OwnedArray<WebViewData::ExternalResource> resources;
	OwnedArray<WebViewData::CallbackItem> callbacks;
};

// Implementation

struct WebViewData::CallbackItem
{
	CallbackItem() = default;

	CallbackItem(const String& name_, const CallbackType& f_) :
		name(name_.toStdString()),
		callback(f_)
	{};

	choc::value::Value operator()(const choc::value::ValueView& args)
	{
		auto x = choc::json::toString(args);
		auto obj = JSON::parse(String(x.data()));
		auto ret = callback(obj);

		if (ret.isString())
		{
			auto s = ret.toString().toStdString();
			return choc::value::createString(s);
		}
		if (ret.isDouble())
			return choc::value::createFloat64((double)ret);
		if (ret.isInt() || ret.isInt64())
			return choc::value::createInt64((int64)ret);

		return choc::json::parse(JSON::toString(ret).toStdString());
	}

	void registerToWebView(choc::ui::WebView* wv) const
	{
		if (callback)
			wv->bind(name, *this);
	}

	std::string name;
	CallbackType callback;
};

WebViewData::WebViewData()
{
	pimpl = new Pimpl();
}

WebViewData::~WebViewData()
{
	delete pimpl;
	pimpl = nullptr;

	
	scripts.clear();
	errorLogger = {};
}

void WebViewData::addCallback(const String& callbackName, const CallbackType& function)
{
	auto id = callbackName.toStdString();

	for (auto p : pimpl->callbacks)
	{
		if (p->name == id)
		{
			p->callback = function;
			return;
		}
	}

	pimpl->callbacks.add(new CallbackItem(callbackName, function));
}



void WebViewData::addResource(const String& path, const String& mimeType, const String& content)
{
	auto pstring = path.toStdString();
	for (auto r : pimpl->resources)
	{
		if (r->path == pstring && errorLogger)
		{
			String m;
			m << "Duplicate WebView resource: " << path;
		}
	}

	pimpl->resources.add(new ExternalResource(path, mimeType, content));
}

void WebViewData::addPNGImage(const String& path, const Image& img)
{
	MemoryOutputStream mos;
	PNGImageFormat png;
	png.writeImageToStream(img, mos);

	mos.flush();
	auto mb = mos.getMemoryBlock();

	auto nr = new ExternalResource(path);

	auto& dv = std::get<0>(nr->resource);
	dv.resize(mb.getSize());
	memcpy(dv.data(), mb.getData(), mb.getSize());
	pimpl->resources.add(nr);
}

void WebViewData::restoreFromValueTree(const ValueTree& v)
{
	if (v.isValid() && v.getType() == Identifier("WebViewResources"))
	{
		serverType = ServerType::Embedded;

		enableCache = true;

		for (const auto& c : v)
		{
			auto nr = new ExternalResource(c["path"]);

			std::get<1>(nr->resource) = c["mime-type"].toString().toStdString();

			auto& dv = std::get<0>(nr->resource);

			if (auto b = c["data"].getBinaryData())
			{
				dv.resize(b->getSize());
				memcpy(dv.data(), b->getData(), b->getSize());
			}
			else
			{
				jassertfalse;
			}

			pimpl->resources.add(nr);
		}
	}
}

void WebViewData::setRootDirectory(const File& newRootDirectory)
{
	if (serverType != ServerType::Embedded)
	{
		rootDirectory = newRootDirectory;
		serverType = ServerType::FileBased;
	}
	
}

void WebViewData::setUsePersistentCalls(bool shouldUsePersistentCalls)
{
	usePersistentCalls = shouldUsePersistentCalls;
}

void WebViewData::reset(bool resetFiles)
{
	if (enableCache)
		return;

	pimpl->resources.clear();
	pimpl->callbacks.clear();
	initScripts.clear();
	scripts.clear();

	if (resetFiles)
	{
		rootFile = {};
		rootDirectory = {};
		serverType = ServerType::Uninitialised;
	}
}

juce::ValueTree WebViewData::exportAsValueTree() const
{
	if (!enableCache && errorLogger)
		errorLogger("You must not disable the caching when exporting the WebView resources");

	ValueTree v("WebViewResources");

	for (auto r : pimpl->resources)
	{
		ValueTree c("Resource");
		c.setProperty("path", String(r->path), nullptr);
		c.setProperty("mime-type", String(std::get<1>(r->resource)), nullptr);

		auto& dv = std::get<0>(r->resource);
		c.setProperty("data", var(dv.data(), dv.size()), nullptr);
		v.addChild(c, -1, nullptr);
	}

	return v;
}

void WebViewData::setEnableCache(bool shouldCacheFiles)
{
	enableCache = shouldCacheFiles;

	if (!shouldCacheFiles)
		pimpl->resources.clear();
}

void WebViewData::call(const String& functionName, const var& args)
{
	String js;

	js << functionName << "(";

	if (args.isObject() || args.isArray())
		js << JSON::toString(args);
	else if (args.isString())
		js << args.toString().quoted();
	else
		js << args.toString();

	js << ");";

	evaluate(functionName, js);

	
}

void WebViewData::evaluate(const String& identifier, const String& jsCode)
{
	if(usePersistentCalls)
		initScripts.set(identifier, jsCode);

	// Apparently that only works on the message thread...
	jassert(MessageManager::getInstance()->isThisTheMessageThread());

	for (auto c : registeredViews)
	{
		if (auto w = dynamic_cast<WebViewWrapper*>(c.getComponent()))
		{
			w->call(jsCode);
		}
	}
}

void WebViewData::registerWebView(Component* c)
{
	registeredViews.addIfNotAlreadyThere(c);
}

void WebViewData::deregisterWebView(Component* c)
{
	registeredViews.removeAllInstancesOf(c);
}

WebViewData::OpaqueResourceType WebViewData::fetch(const std::string& path)
{
	std::string pathToUse = path == "/" ? rootFile : path;

	for (auto r : pimpl->resources)
	{
		if (r->path == pathToUse)
			return r->resource;
	}

	if (serverType == ServerType::FileBased)
	{
		auto f = rootDirectory.getChildFile(pathToUse.substr(1));

		if (f.existsAsFile())
		{
			FileInputStream fis(f);

			ScopedPointer<ExternalResource> nr = new ExternalResource(pathToUse);
			auto& dv = std::get<0>(nr->resource);
			dv.resize(fis.getTotalLength());
			fis.read(dv.data(), fis.getTotalLength());

			String mime;

			auto extension = f.getFileExtension().substring(1).toLowerCase();

			if (extension == "js")
				extension = "javascript";

			if (ImageFileFormat::findImageFormatForFileExtension(f) != nullptr)
				mime << "image/" << extension;
			else
				mime << "text/" << extension;

			std::get<1>(nr->resource) = mime.toStdString();

			if (enableCache)
			{
				pimpl->resources.add(nr.release());
				return pimpl->resources.getLast()->resource;
			}
			else
				return nr->resource;
		}
	}

	if (errorLogger)
	{
		String m;
		m << "WebView Resource for " << pathToUse << " not found";
		errorLogger(m);
	}

	return {};
};

WebViewWrapper::WebViewWrapper(WebViewData::Ptr d) :
	data(d)
{
	data->registerWebView(this);

	Component::SafePointer<WebViewWrapper> safeThis(this);

	MessageManager::callAsync([safeThis]()
	{
		if(safeThis.getComponent() != nullptr)
			safeThis->refresh();
	});
}

WebViewWrapper::~WebViewWrapper()
{
	jassert(MessageManager::getInstance()->isThisTheMessageThread());

	if (data != nullptr)
		data->deregisterWebView(this);

	if(content != nullptr)
		content->setWindowHandle(nullptr);

	content = nullptr;
	webView = nullptr;
	data = nullptr;
}

void WebViewWrapper::resized()
{
	if (content != nullptr)
	{
		content->setBounds(getLocalBounds());
		
	}
		
}

void WebViewWrapper::call(const String& jsCode)
{
	if(webView != nullptr)
		webView->evaluateJavascript(jsCode.toStdString());
}


void WebViewWrapper::refresh()
{
	jassert(MessageManager::getInstance()->isThisTheMessageThread());

	auto currentFocusComponent = Component::getCurrentlyFocusedComponent();

	content = new NativeComponentType();

	choc::ui::WebView::Options options;
	options.enableDebugMode = false;
	auto d = data;
	options.fetchResource = [d](const std::string& path)
	{
		choc::ui::WebView::Options::Resource s;

		if (d->serverType == WebViewData::ServerType::Uninitialised)
			return s;

		auto type = d->fetch(path);

		s.data = std::move(std::get<0>(type));
		s.mimeType = std::move(std::get<1>(type));

		return s;
	};

	webView = new choc::ui::WebView(options);

	for (const auto& x : data->scripts)
		webView->addInitScript(x.toStdString());

	if (data->usePersistentCalls)
	{
		for (const auto& x : data->initScripts.getAllValues())
		{
			String asyncCode;
			asyncCode << "document.addEventListener('DOMContentLoaded', function() {" << x << " }, false);";
			webView->addInitScript(asyncCode.toStdString());
		}
	}
	
	for (const auto c : data->pimpl->callbacks)
		c->registerToWebView(webView);

	content->setWindowHandle(webView->getViewHandle());

	addAndMakeVisible(content);
	
	refreshBounds(UnblurryGraphics::getScaleFactorForComponent(this));

	if(currentFocusComponent != nullptr)
		currentFocusComponent->grabKeyboardFocusAsync();
}

void WebViewWrapper::refreshBounds(float newScaleFactor)
{
	if(content != nullptr)
		content->resizeToFitCrossPlatform();

	if (webView != nullptr)
		webView->resizeToFit(newScaleFactor);
		
	resized();
}

WebViewData::ExternalResource::ExternalResource(const String& path_, const String& mimeType, const String& data) :
	path(path_.toStdString())
{
	auto& dataVector = std::get<0>(resource);
	dataVector.reserve(data.length());

	auto b = data.begin();
	auto e = data.end();

	while (b != e)
	{
		dataVector.push_back(*b++);
	}

	std::get<1>(resource) = mimeType.toStdString();
}

WebViewData::ExternalResource::ExternalResource(const String& path_) :
	path(path_.toStdString())
{

}

}
