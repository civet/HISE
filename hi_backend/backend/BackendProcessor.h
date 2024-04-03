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

#ifndef BACKEND_PROCESSOR_H_INCLUDED
#define BACKEND_PROCESSOR_H_INCLUDED

namespace hise { using namespace juce;



class BackendProcessor;

struct AnalyserInfo: public ReferenceCountedObject
{
	AnalyserInfo(SimpleRingBuffer::PropertyObject* o1, SimpleRingBuffer::PropertyObject* o2)
	{
		ringBuffer[0] = new SimpleRingBuffer();
		ringBuffer[0]->setPropertyObject(o1);
		ringBuffer[1] = new SimpleRingBuffer();
		ringBuffer[1]->setPropertyObject(o2);
	}
	
	using Ptr = ReferenceCountedObjectPtr<AnalyserInfo>;

	SimpleRingBuffer::Ptr getAnalyserBuffer(bool getPost) const
	{
		return ringBuffer[(int)getPost];
	}
	
	std::pair<WeakReference<Processor>, int> currentlyAnalysedProcessor = { nullptr, 0 };
	int lastNoteNumber = -1;
	double duration = 0.0;
	SimpleRingBuffer::Ptr ringBuffer[2];
};

struct ExampleAssetManager: public ReferenceCountedObject,
							public ProjectHandler
						    
{
	ExampleAssetManager(MainController* mc):
	  ProjectHandler(mc),
	  mainProjectHandler(mc->getCurrentFileHandler())
	{
		setWorkingProject(mainProjectHandler.getRootFolder());

		auto hp = GET_HISE_SETTING(mc->getMainSynthChain(), HiseSettings::Compiler::HisePath).toString();

		if(hp.isNotEmpty())
		{
			rootDirectory = File(hp).getChildFile("example_assets");

			for(auto d: getSubDirectoryIds())
				rootDirectory.getChildFile(getIdentifier(d)).createDirectory();

			checkSubDirectories();
			downloadThread = new DownloadThread(*this);
		}
		else
		{
			debugError(mc->getMainSynthChain(), "You need to set the HISE path in order to download the example assets");
		}
	}

	FileHandlerBase& mainProjectHandler;

	File getSubDirectory(SubDirectories dir) const override
	{
		auto redirected = getSubDirectoryIds();

		if(redirected.contains(dir))
			return ProjectHandler::getSubDirectory(dir);
		else
			return mainProjectHandler.getSubDirectory(dir);
	}

	Array<SubDirectories> getSubDirectoryIds() const override
	{
		return {
			FileHandlerBase::SubDirectories::AudioFiles,
			FileHandlerBase::SubDirectories::SampleMaps,
			FileHandlerBase::SubDirectories::Samples,
			FileHandlerBase::SubDirectories::Images,
			FileHandlerBase::SubDirectories::MidiFiles
		};
	}

	File getRootFolder() const override
	{
		return rootDirectory;
	}

	struct DownloadThread: public Thread,
						   public URL::DownloadTaskListener
	{
		DownloadThread(ExampleAssetManager& parent_):
		  Thread("Download example assets"),
		  parent(parent_)
		{
			startThread(5);
		}

		~DownloadThread()
		{
			stopThread(1000);
		}

		ExampleAssetManager& parent;

		std::unique_ptr<URL::DownloadTask> download;

		void finished (URL::DownloadTask* task, bool success) override
		{
			String message;

			if(success)
				message << "Download complete";
			else
				message << "Download failed";

			debugToConsole(parent.getMainController()->getMainSynthChain(), message);
		}
			
        void progress (URL::DownloadTask* task, int64 bytesDownloaded, int64 totalLength) override
		{
			String message;
			message << "Downloading example assets... " << String(bytesDownloaded/1024) << " / " << String(totalLength/1024) << " kB";
			debugToConsole(parent.getMainController()->getMainSynthChain(), message);
		}

		void downloadAndExtract()
		{
			debugToConsole(parent.getMainController()->getMainSynthChain(), "Downloading example assets...");

			parent.rootDirectory.createDirectory();
			URL url("https://github.com/qdr/HiseSnippetDB/releases/download/1.0.0/Assets.zip");

			auto target = parent.rootDirectory.getChildFile("Assets.zip");

			download = url.downloadToFile(target, {}, this);

			while(!download->isFinished())
			{
				if(threadShouldExit())
					break;

				wait(300);
			}

			if(!download->hadError())
			{
				debugToConsole(parent.getMainController()->getMainSynthChain(), "Extracting example assets...");

				ZipFile zf(target);

				auto ok = zf.uncompressTo(parent.rootDirectory, true);

				if(!ok.wasOk())
				{
					debugToConsole(parent.getMainController()->getMainSynthChain(), "Extracted OK");
					target.deleteFile();
				}
				else
					debugToConsole(parent.getMainController()->getMainSynthChain(), ok.getErrorMessage());
			}
		}

		void run() override
		{
			if(parent.rootDirectory != File())
			{
				downloadAndExtract();

				parent.checkSubDirectories();

				parent.pool->getAudioSampleBufferPool().loadAllFilesFromProjectFolder();
				parent.pool->getImagePool().loadAllFilesFromProjectFolder();
				parent.pool->getMidiFilePool().loadAllFilesFromProjectFolder();
				parent.pool->getSampleMapPool().loadAllFilesFromProjectFolder();
			}
			
		}

		bool done;
	};

	ScopedPointer<DownloadThread> downloadThread;
	File rootDirectory;

	using Ptr = ReferenceCountedObjectPtr<ExampleAssetManager>;
};

/** This is the main audio processor for the backend application. 
*
*	It connects to a BackendProcessorEditor and has extensive development features.
*
*	It is a wrapper for all plugin types and provides 8 parameters for the macro controls.
*	It also acts as global MainController to allow every child object to get / set certain global information
*/
class BackendProcessor: public PluginParameterAudioProcessor,
					    public AudioProcessorDriver,
						public MainController,
						public ProjectHandler::Listener,
						public MarkdownDatabaseHolder,
						public ExpansionHandler::Listener,
						public SimpleRingBuffer::WriterBase
{
public:
	BackendProcessor(AudioDeviceManager *deviceManager_=nullptr, AudioProcessorPlayer *callback_=nullptr);

	~BackendProcessor();

	void projectChanged(const File& newRootDirectory) override;

	void refreshExpansionType();

	void handleEditorData(bool save)
	{
#if IS_STANDALONE_APP
		File jsonFile = NativeFileHandler::getAppDataDirectory(nullptr).getChildFile("editorData.json");

		if (save)
		{
			if (editorInformation.isObject())
				jsonFile.replaceWithText(JSON::toString(editorInformation));
			else
				jsonFile.deleteFile();
		}
		else
		{
			editorInformation = JSON::parse(jsonFile);
		}
#else
		ignoreUnused(save);
#endif

		
	}

	void prepareToPlay (double sampleRate, int samplesPerBlock);
	void releaseResources() 
	{
		
	};

	void getStateInformation	(MemoryBlock &destData) override;;

	void logMessage(const String& message, bool isCritical) override
	{
		if (isCritical)
		{
			debugError(getMainSynthChain(), message);
		}
		else
			debugToConsole(getMainSynthChain(), message);
	}

	void setStateInformation(const void *data,int sizeInBytes) override;

	void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

	virtual void processBlockBypassed (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);;

	void handleControllersForMacroKnobs(const MidiBuffer &midiMessages);

	AudioProcessorEditor* createEditor();
	bool hasEditor() const {return true;};

	bool acceptsMidi() const {return true;};
	bool producesMidi() const {return false;};
	
	double getTailLengthSeconds() const {return 0.0;};

	ModulatorSynthChain *getMainSynthChain() override {return synthChain; };

	const ModulatorSynthChain *getMainSynthChain() const override { return synthChain; }

	void registerItemGenerators() override;
	void registerContentProcessor(MarkdownContentProcessor* processor) override;
	File getCachedDocFolder() const override;
	File getDatabaseRootDirectory() const override;

	void toggleDynamicBufferSize() 
	{
		simulateDynamicBufferSize = !isUsingDynamicBufferSize();
	}

	void setDatabaseRootDirectory(File newDatabaseDirectory)
	{
		databaseRoot = newDatabaseDirectory;
	}

	BackendProcessor* getDocProcessor();

	BackendRootWindow* getDocWindow();

	Component* getRootComponent() override;

	bool databaseDirectoryInitialised() const override
	{
		auto path = getSettingsObject().getSetting(HiseSettings::Documentation::DocRepository).toString();

		auto d = File(path);
		
		return d.isDirectory() && d.getChildFile("hise-modules").isDirectory();
	}

	/// @brief returns the number of PluginParameter objects, that are added in the constructor
    int getNumParameters() override
	{
		return 8;
	}

	/// @brief returns the PluginParameter value of the indexed PluginParameter.
    float getParameter (int index) override
	{
		return synthChain->getMacroControlData(index)->getCurrentValue() / 127.0f;
	}

	/** @brief sets the PluginParameter value.
	
		This method uses the 0.0-1.0 range to ensure compatibility with hosts. 
		A more user friendly but slower function for GUI handling etc. is setParameterConverted()
	*/
    void setParameter (int index, float newValue) override
	{
		synthChain->setMacroControl(index, newValue * 127.0f, sendNotificationAsync);
	}


	/// @brief returns the name of the PluginParameter
    const String getParameterName (int index) override
	{
		return "Macro " + String(index + 1);
	}

	/// @brief returns a converted and labeled string that represents the current value
    const String getParameterText (int index) override
	{
		
		return String(synthChain->getMacroControlData(index)->getDisplayValue(), 1);
	}

	JavascriptProcessor* createInterface(int width, int height);;

	void setEditorData(var editorState);

	juce::OnlineUnlockStatus* getLicenseUnlocker() 
	{
		return &scriptUnlocker;
	}

	ScriptUnlocker scriptUnlocker;

#if HISE_INCLUDE_SNEX_FLOATING_TILES
	snex::ui::WorkbenchManager workbenches;
	virtual void* getWorkbenchManager() override { return reinterpret_cast<void*>(&workbenches); }
#endif

	BackendDllManager::Ptr dllManager;

	
    LambdaBroadcaster<Processor*> processorAddBroadcaster;
	
	LambdaBroadcaster<Identifier, Processor*> workspaceBroadcaster;

    ExternalClockSimulator externalClockSim;
	
	void pushToAnalyserBuffer(AnalyserInfo::Ptr info, bool post, const AudioSampleBuffer& buffer, int numSamples);

	int getCurrentAnalyserNoteNumber() const
	{
		return currentNoteNumber;
	}

	AnalyserInfo::Ptr getAnalyserInfoForProcessor(Processor* p)
	{
		if(auto sl = SimpleReadWriteLock::ScopedTryReadLock(postAnalyserLock))
		{
			for(auto i: currentAnalysers)
			{
				if(i->currentlyAnalysedProcessor.first == p)
					return i;
			}
		}

		return nullptr;
	}
	
	Result setAnalysedProcessor(AnalyserInfo::Ptr newInfo, bool add)
	{
		SimpleReadWriteLock::ScopedWriteLock sl(postAnalyserLock);

		if(add)
		{
			for(auto i: currentAnalysers)
			{
				if(i->currentlyAnalysedProcessor.first == newInfo->currentlyAnalysedProcessor.first)
					return Result::fail("Another analyser is already assigned to the module " + i->currentlyAnalysedProcessor.first->getId());
			}
			currentAnalysers.add(newInfo);
		}
			
		else
			currentAnalysers.removeObject(newInfo);

		return Result::ok();
	}

	bool isSnippetBrowser() const
	{
		return isSnippet;
	}

	void setIsSnippetBrowser()
	{
		isSnippet = true;
	}

	ExampleAssetManager::Ptr getAssetManager()
	{
		if(assetManager == nullptr)
			assetManager = new ExampleAssetManager(this);

		return assetManager;
	}

	ExampleAssetManager::Ptr assetManager;

private:

	bool isSnippet = false;

	int currentNoteNumber = -1;

	mutable SimpleReadWriteLock postAnalyserLock;

	ReferenceCountedArray<AnalyserInfo> currentAnalysers;
	
	File databaseRoot;

	MemoryBlock tempLoadingData;

	friend class BackendProcessorEditor;
	friend class BackendCommandTarget;
	friend class CombinedDebugArea;

	ScopedPointer<ModulatorSynthChain> synthChain;
	
	var editorInformation;

	ScopedPointer<BackendProcessor> docProcessor;
	BackendRootWindow* docWindow;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackendProcessor)
};

struct ScopedAnalyser
{
	ScopedAnalyser(MainController* mc, Processor* p, AudioSampleBuffer& b, int numSamples_):
	  bp(static_cast<BackendProcessor*>(mc))
	{
		info = bp->getAnalyserInfoForProcessor(p);

		if(info == nullptr)
			return;

		buffer = &b;
		numSamples = numSamples_;

		bp->pushToAnalyserBuffer(info, false, *buffer, numSamples);
		milliSeconds = Time::getMillisecondCounterHiRes();
	}

	~ScopedAnalyser()
	{
		if(buffer != nullptr)
		{
			auto now = Time::getMillisecondCounterHiRes();

			info->duration = now - milliSeconds;
			bp->pushToAnalyserBuffer(info, true, *buffer, numSamples);
		}
	}

	double milliSeconds = 0.0;
	AnalyserInfo::Ptr info;
	AudioSampleBuffer* buffer = nullptr;
	int numSamples = -1;
	BackendProcessor* bp;
	
};


} // namespace hise

#endif

