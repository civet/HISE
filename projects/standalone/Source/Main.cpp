/*
  ==============================================================================

    This file was auto-generated by the Introjucer!

    It contains the basic startup code for a Juce application.

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "MainComponent.h"

class CommandLineActions
{
private:
    
    static void throwErrorAndQuit(const String& errorMessage)
    {
#if JUCE_DEBUG
        DBG(errorMessage);
        jassertfalse;
#else
        print("ERROR: " + errorMessage);
        exit(1);
#endif
    }
    
    static void print(const String& message)
    {
#if JUCE_DEBUG
        DBG(message);
#else
        std::cout << message << std::endl;
#endif
    }
    
    static StringArray getCommandLineArgs(const String& commandLine)
    {
        auto argsString = commandLine.fromFirstOccurrenceOf(" ", false, false);
        return StringArray::fromTokens(argsString, true);
    }
    
    
    
	static String getArgument(const StringArray& args, const String& prefix)
	{
		for (auto arg : args)
		{
			if (arg.unquoted().startsWith(prefix))
				return arg.unquoted().fromFirstOccurrenceOf(prefix, false, false);
		}

		return {};
	}

	static File getCurrentProjectFolder()
	{
		ScopedPointer<StandaloneProcessor> sp = new StandaloneProcessor();
		ScopedPointer<MainController> mc = dynamic_cast<MainController*>(sp->createProcessor());

		auto f = GET_PROJECT_HANDLER(mc->getMainSynthChain()).getWorkDirectory();

		mc = nullptr;
		sp = nullptr;

		return f;
	}

	

	static File getFilePathArgument(const StringArray& args, const File& root = File())
	{
		auto s = getArgument(args, "-p:");

		if (s.isNotEmpty() && File::isAbsolutePath(s))
			return File(s);

		if (root.isDirectory())
		{
			auto rel = root.getChildFile(s);

			if (rel.existsAsFile())
				return rel;
		}

		throwErrorAndQuit("`" + s + "` is not a valid path");

		return File();
	}

public:

	static void printHelp()
	{
		print("");
		print("HISE Command Line Tool");
		print("----------------------");
		print("");
		print("Usage: ");
		print("");
		print("HISE COMMAND [FILE] [OPTIONS]");
		print("");
		print("Commands: ");
		print("");
		print("export: builds the project using the default settings");
		print("export_ci: builds the project using customized behaviour for automated builds");
		print(" - always use VisualStudio 2017 on Windows" );
		print(" - don't copy the plugins to the plugin folders" );
		print(" - use a relative path for the project file" );
		print(" - ignore the global HISE path and use the HISE repository folder from the");
		print("   current HISE executable");
		print("full_exp -p:PATH: exports the project as Full Instrument Expansion (aka HISE Player Library)");
		print(" - you must supply the absolute path to the .XML file with the '-p:' argument.");
		print("compress_samples -p:PATH: collects all HLAC files into a hr1 archive");
		print(" - if an info.hxi file is found in the current work directory, it will embed it into the");
		print("   archive. You must supply a XML project file with the `-p:` argument that will be");
		print("   loaded during the export.");
		print("Arguments: " );
		print("FILE      The path to the project file (either .xml or .hip you want to export)." );
		print("          In CI mode, this will be the relative path from the current project folder");
		print("          In standard mode, it must be an absolute path");
		print("-h:{TEXT} sets the HISE path. Use this if you don't have compiler settings set." );
		print("-ipp      enables Intel Performance Primitives for fast convolution." );
		print("-l        This can be used to compile a version that runs on legacy CPU models.");
		print("-t:{TEXT} sets the project type ('standalone' | 'instrument' | 'effect' | 'midi')" );
		print("-p:{TEXT} sets the plugin type ('VST'  | 'AU'   | 'VST_AU' | 'AAX' |)" );
		print("                                'ALL'  | 'VST2' | 'VST3'   | 'VST23AU' )");
		print("          (Leave empty for standalone export). Note that if you use the VST2, VST3,");
		print("           VST23AU it will override the project settings so you can export both versions).");
		print("           Note: The VST23AU flag will skip AU on Windows and build only VST2 and VST3.");
        print("-nolto    deactivates link time optimisation. The resulting binary is not as optimized");
        print("          but the build time is much shorter");
        print("-D:NAME=VALUE Adds a temporary preprocessor definition to the extra definitions.");
        print("              You can use multiple definitions by using this flag multiple times.");
		print("--test [PLUGIN_FILE]" );
		print("Tests the given plugin" );
		print("");
		print("set_project_folder -p:PATH" );
		print("Changes the current project folder." );
		print("");
		print("set_hise_folder -p:PATH");
		print("Sets the location for the HISE source code folder.");
		print("get_project_folder" );
		print("Returns the current project folder." );
		print("");
		print("set_version -v:NEW_VERSION_STRING" );
		print("Sets the project version number to the given string");
		print("");
		print("clean [-p:PATH] [-all]" );
		print("Cleans the Binaries folder of the given project.");
		print("-p:PATH - the path to the project folder.");
		print("");
        print("create-win-installer [-a:x64|x86] [-noaax] [-vst2] [-vst3]" );
		print("Creates a template install script for Inno Setup for the project" );
		print("Add the -noaax flag to not include the AAX build");
		print("If you want to include VST2 and or VST3 plugins, specify the version.");
		print("If no VST flag is set, then the VST2 plugin is included as default");
        print("");
        print("create-docs -p:PATH");
        print("Creates the HISE documentation files from the markdown files in the given directory.");
		print("load -p:PATH");
		print("");
		print("Loads the given file (either .xml file or .hip file) and returns the status code");
		print("You can call Engine.setCommandLineStatus(1) in the onInit callback to report an error");
		print("");
		print("compile_networks -c:CONFIG");
		print("Compiles the DSP networks in the given project folder. Use the -c flag to specify the build");
		print("configuration ('Debug' or 'Release')");
		print("");
		print("run_unit_tests");
		print("Runs the unit tests. In order for this to work, HISE must be built with the CI configuration");

		exit(0);
	}

	static void createWindowsInstallerFile(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);
		
		ScopedPointer<StandaloneProcessor> sp = new StandaloneProcessor();
		ScopedPointer<MainController> mc = dynamic_cast<MainController*>(sp->createProcessor());

		const bool includeAAX = !args.contains("-noaax");
        
		const bool include32 = false;
        const bool include64 = !args.contains("-a:x86");

		const bool includeVST3 = args.contains("-vst3");

		const bool includeVST2 = !includeVST3 || args.contains("-vst2");

		auto content = BackendCommandTarget::Actions::createWindowsInstallerTemplate(mc, includeAAX, include32, include64, includeVST2, includeVST3);

		auto root = GET_PROJECT_HANDLER(mc->getMainSynthChain()).getWorkDirectory();

		auto installFile = root.getChildFile("WinInstaller.iss");
		
		installFile.replaceWithText(content);

		mc = nullptr;
		sp = nullptr;

		print("The installer script was written to " + installFile.getFullPathName());
		print("");

		exit(0);
	}

    
    
	static void setProjectVersion(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);

		auto versionString = getArgument(args, "-v:");

		SemanticVersionChecker checker(versionString, versionString);

		if (!checker.newVersionNumberIsValid())
		{
			throwErrorAndQuit(versionString + " is not a valid semantic version number");
		}

		auto pd = getCurrentProjectFolder();
		
		if (pd.isDirectory())
		{
			auto projectFile = pd.getChildFile("project_info.xml");

			if (projectFile.existsAsFile())
			{
				auto content = projectFile.loadFileAsString();

				auto wildcard = "Version value=\"(\\d+\\.\\d+\\.\\d+)\"";

				auto firstMatch = RegexFunctions::getFirstMatch(wildcard, content);

				if (firstMatch.size() == 2)
				{
					auto newVersion = firstMatch[0].replace(firstMatch[1], versionString);

					print("Old version: " + firstMatch[1]);
					print("New version: " + versionString);
					
					auto newContent = content.replace(firstMatch[0], newVersion);
					projectFile.replaceWithText(newContent);

					exit(0);
				}
				else throwErrorAndQuit("Regex parsing error. Check the file.");
				
			}
			else throwErrorAndQuit(pd.getFullPathName() + " is not a valid folder");
		}
		else throwErrorAndQuit(pd.getFullPathName() + " is not a valid folder");
	}
	
	static int loadPresetFile(const String& commandLine, const std::function<Result(BackendProcessor*)>& additionalFunction = {})
	{
		auto args = getCommandLineArgs(commandLine);

		CompileExporter::setExportingFromCommandLine();
		
		ScopedPointer<StandaloneProcessor> processor = new StandaloneProcessor();

		auto bp = dynamic_cast<BackendProcessor*>(processor->getCurrentProcessor());

        dynamic_cast<GlobalSettingManager*>(bp)->getSettingsObject().addTemporaryDefinitions(CompileExporter::getTemporaryDefinitions(commandLine));
        
		ModulatorSynthChain* mainSynthChain = bp->getMainSynthChain();
		File currentProjectFolder = GET_PROJECT_HANDLER(mainSynthChain).getWorkDirectory();

		File presetFile = getFilePathArgument(args, bp->getActiveFileHandler()->getRootFolder());

		CompileExporter::setExportUsingCI(false);

		File projectDirectory = presetFile.getParentDirectory().getParentDirectory();

		if (currentProjectFolder != projectDirectory)
		{
			GET_PROJECT_HANDLER(mainSynthChain).setWorkingProject(projectDirectory);
		}

		std::cout << "Loading the preset...";

		try
		{
			if (presetFile.getFileExtension() == ".hip")
			{
				bp->loadPresetFromFile(presetFile, nullptr);
			}
			else if (presetFile.getFileExtension() == ".xml")
			{
				auto xml = XmlDocument::parse(presetFile);

				if (xml != nullptr)
				{
					XmlBackupFunctions::addContentFromSubdirectory(*xml, presetFile);
					String newId = xml->getStringAttribute("ID");

					auto v = ValueTree::fromXml(*xml);
					XmlBackupFunctions::restoreAllScripts(v, mainSynthChain, newId);

					bp->loadPresetFromValueTree(v);
				}
			}
		}
		catch (hise::CommandLineException& c)
		{
			throwErrorAndQuit(c.r.getErrorMessage());
			return 1;
		}
		
		std::cout << "DONE" << std::endl << std::endl;

		if (additionalFunction)
		{
			auto ok = additionalFunction(bp);

			if (ok.failed())
			{
				processor = nullptr;
				throwErrorAndQuit(ok.getErrorMessage());
				return 1;
			}
		}

		processor = nullptr;
		
		return 0;
	}

	static void compileNetworks(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);
		auto config = getArgument(args, "-c:");

		if (config != "Debug" && config != "Release" && config != "CI")
		{
			throwErrorAndQuit("Invalid build configuration: " + config);
		}

		ScopedPointer<StandaloneProcessor> sp = new StandaloneProcessor();

		auto mc = dynamic_cast<MainController*>(sp->getCurrentProcessor());

		raw::Builder builder(mc);

		auto jsfx = builder.create<JavascriptMasterEffect>(mc->getMainSynthChain(), raw::IDs::Chains::FX);

		jsfx->getOrCreate("dsp");

		CompileExporter::setExportingFromCommandLine();

		hise::DspNetworkCompileExporter exporter(nullptr, dynamic_cast<BackendProcessor*>(mc));

		exporter.getComboBoxComponent("build")->setText(config, dontSendNotification);

		exporter.run();
	}

	static void setProjectFolder(const String& commandLine, bool exitOnSuccess=true)
	{
		auto args = getCommandLineArgs(commandLine);
		auto root = getFilePathArgument(args);

		ScopedPointer<StandaloneProcessor> sp = new StandaloneProcessor();

		ScopedPointer<MainController> mc = dynamic_cast<MainController*>(sp->createProcessor());

		auto handler = &GET_PROJECT_HANDLER(mc->getMainSynthChain());

		auto prevProject = handler->getWorkDirectory();

		handler->setWorkingProject(root);

		mc = nullptr;
		sp = nullptr;

		if(exitOnSuccess)
			exit(0);
	}

	static void getProjectFolder(const String& /*commandLine*/)
	{
		print("Current project folder:\n" + getCurrentProjectFolder().getFullPathName());
		exit(0);
	}

	static void cleanBuildFolder(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);

		File root = getCurrentProjectFolder();

		bool cleanEverything = args.contains("-all");

		auto buildDirectory = root.getChildFile("Binaries");

		if (buildDirectory.isDirectory())
		{
			if (cleanEverything)
			{
				print("Wiping Build folder...");

				buildDirectory.deleteRecursively();
				buildDirectory.createDirectory();
			}
			else
			{
				print("Cleaning Builds but keeping the source code...");
				buildDirectory.getChildFile("Builds").deleteRecursively();
				buildDirectory.getChildFile("JuceLibraryCode").deleteRecursively();
			}

			exit(0);
		}
		else
		{
			throwErrorAndQuit("Build folder not found at " + root.getFullPathName());
		}
	}
	
    static void createDocumentation(const String& commandLine)
    {
        auto args = getCommandLineArgs(commandLine);
        auto docRoot = getFilePathArgument(args);
        
        if(!docRoot.isDirectory())
            throwErrorAndQuit("Not a valid directory");
        
        print("Create HISE instance");
        
        CompileExporter::setExportingFromCommandLine();
        
        StandaloneProcessor sp;
        
        ScopedPointer<Component> ed = sp.createEditor();
        
        auto bp = dynamic_cast<BackendProcessor*>(sp.getCurrentProcessor());
        
        hise::DatabaseCrawler crawler(*bp);
        
        print("Rebuild database");
        
        bp->setDatabaseRootDirectory(docRoot);
        
        bp->setForceCachedDataUse(false);
        crawler.clearResolvers();
        bp->addContentProcessor(&crawler);

        print("Creating Content data");
        
        crawler.createContentTree();

        print("Creating Image data");
        
        crawler.createImageTree();
        
        print("Writing data files");
        
        crawler.createDataFiles(docRoot.getChildFile("cached"), true);
    }
    
	static void setHiseFolder(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);
		auto hisePath = getFilePathArgument(args);

		if (!hisePath.isDirectory())
			throwErrorAndQuit(hisePath.getFullPathName() + " is not a valid directory");

		if (!hisePath.getChildFile("hi_core/").isDirectory())
		{
			throwErrorAndQuit(hisePath.getFullPathName() + " is not HISE source folder");
		}

		auto compilerSettings = NativeFileHandler::getAppDataDirectory(nullptr).getChildFile("compilerSettings.xml");

		ScopedPointer<XmlElement> xml;

		if (compilerSettings.existsAsFile())
		{
			xml = XmlDocument::parse(compilerSettings).release();
		}
		else
		{
			xml = new XmlElement("CompilerSettings");
			
			auto c1 = new XmlElement("HisePath");
			c1->setAttribute("value", hisePath.getFullPathName());
			c1->setAttribute("type", "FILE");
			c1->setAttribute("description", "Path to HISE modules");
			xml->addChildElement(c1);

			auto c2 = new XmlElement("VisualStudioVersion");
			c2->setAttribute("value", "Visual Studio 2017");
			c2->setAttribute("type", "LIST");
			c2->setAttribute("description", "Installed VisualStudio version");
			c2->setAttribute("options", "Visual Studio 2013&#10;Visual Studio 2015");
			xml->addChildElement(c2);

			auto c3 = new XmlElement("UseIPP");
			c3->setAttribute("value", "Yes");
			c3->setAttribute("type", "LIST");
			c3->setAttribute("description", "Use IPP");
			c3->setAttribute("options", "Yes&#10;No");
			xml->addChildElement(c3);
		}

		if (xml == nullptr)
		{
			throwErrorAndQuit("Compiler Settings can't be loaded");
		}
		else
		{
			if (auto child = xml->getChildByName("HisePath"))
			{
				child->setAttribute("value", hisePath.getFullPathName());
				compilerSettings.replaceWithText(xml->createDocument(""));

				print("HISE SDK path set to " + hisePath.getFullPathName());
				exit(0);
			}
			else throwErrorAndQuit("Invalid XML");
		}
	}
};

REGISTER_STATIC_DSP_LIBRARIES()
{
	REGISTER_STATIC_DSP_FACTORY(HiseCoreDspFactory);
	REGISTER_STATIC_DSP_FACTORY(ScriptFilterBank);

#if ENABLE_JUCE_DSP
    REGISTER_STATIC_DSP_FACTORY(JuceDspModuleFactory);
#endif
}

AudioProcessor* hise::StandaloneProcessor::createProcessor()
{
	return new hise::BackendProcessor(deviceManager, callback);
}

class StdLogger : public Logger
{
public:

	StdLogger()
	{

	}

	void logMessage(const String& message) override
	{
		NewLine nl;
		std::cout << message << nl;
	}
};

//==============================================================================
class HISEStandaloneApplication  : public JUCEApplication
{
public:
    //==============================================================================
    HISEStandaloneApplication() {}

    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const String& commandLine) override
    {
		if (commandLine.startsWith("export"))
		{
			String pluginFile;

			hise::CompileExporter::ErrorCodes result = hise::CompileExporter::compileFromCommandLine(commandLine, pluginFile);

			if (result != hise::CompileExporter::OK)
			{
				std::cout << std::endl << "==============================================================================" << std::endl;
				std::cout << "EXPORT ERROR: " << hise::CompileExporter::getCompileResult(result) << std::endl;
				std::cout << "==============================================================================" << std::endl << std::endl;

				exit((int)result);
			}
			
			quit();
			return;
		}
		else if (commandLine.startsWith("full_exp"))
		{
			auto ok = CommandLineActions::loadPresetFile(commandLine, BackendCommandTarget::Actions::exportInstrumentExpansion);

			if (ok)
				exit(ok);

			quit();
			return;
		}
		else if (commandLine.startsWith("compress_samples"))
		{
			auto ok = CommandLineActions::loadPresetFile(commandLine, BackendCommandTarget::Actions::createSampleArchive);

			if (ok)
				exit(ok);

			quit();
			return;
		}
		else if (commandLine.startsWith("clean"))
		{
			CommandLineActions::cleanBuildFolder(commandLine);
			quit();
			return;
		}
		else if (commandLine.startsWith("create-win-installer"))
		{
			CommandLineActions::createWindowsInstallerFile(commandLine);
			quit();
			return;
		}
		else if (commandLine.startsWith("set_project_folder"))
		{
			CommandLineActions::setProjectFolder(commandLine);
			quit();
			return;
		}
		else if (commandLine.startsWith("get_project_folder"))
		{
			CommandLineActions::getProjectFolder(commandLine);
			quit();
			return;
		}
		else if (commandLine.startsWith("run_unit_tests"))
		{
#if HI_RUN_UNIT_TESTS
			UnitTestRunner runner;
			runner.setAssertOnFailure(false);
            
            // If you're working on a unit test, just add the "Current" category
            // and then uncomment this line.
            if(UnitTest::getTestsInCategory("Current").isEmpty())
                runner.runAllTests();
            else
                runner.runTestsInCategory("Current");

			for (int i = 0; i < runner.getNumResults(); i++)
			{
				auto result = runner.getResult(i);

				if (result->failures > 0)
				{
					std::cout << "Test Fails:\n";
					std::cout << result->messages.joinIntoString("\n");
					exit(1);
				}
			}
            
            quit();
            return;
#else
			std::cout << "You need to build HISE with the CI configuration in order to run the unit tests";
			exit(1);
#endif
		}
		else if (commandLine.startsWith("set_version"))
		{
			CommandLineActions::setProjectVersion(commandLine);
			quit();
			return;
		}
		else if (commandLine.startsWith("set_hise_folder"))
		{
			CommandLineActions::setHiseFolder(commandLine);
			quit();
			return;
		}
        else if (commandLine.startsWith("create-docs"))
        {
            CommandLineActions::createDocumentation(commandLine);
            quit();
            return;
        }
		else if (commandLine.startsWith("--help"))
		{
			CommandLineActions::printHelp();
			quit();
			return;
		}
		else if (commandLine.startsWith("--test"))
		{
			ScopedPointer<StdLogger> stdLogger = new StdLogger();

			Logger::setCurrentLogger(stdLogger);

			hise::BackendCommandTarget::Actions::testPlugin(commandLine.fromFirstOccurrenceOf("--test", false, false).trim().replace("\"", ""));

			Logger::setCurrentLogger(nullptr);
			stdLogger = nullptr;

			quit();
			return;
		}
		else if (commandLine.startsWith("load "))
		{
			auto ok = CommandLineActions::loadPresetFile(commandLine);

			if (ok != 0)
			{
				exit(ok);
				return;
			}
				

			quit();
			return;
		}
		else if (commandLine.startsWith("compile_networks"))
		{
			CommandLineActions::compileNetworks(commandLine);

			quit();
			return;
		}
		else
		{
			mainWindow = new MainWindow(commandLine);
			mainWindow->setUsingNativeTitleBar(true);
			mainWindow->toFront(true);
		}
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const String& ) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

	
    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainContentComponent class.
    */
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow(const String &commandLine)  : DocumentWindow ("HISE",
                                        Colours::lightgrey,
										DocumentWindow::TitleBarButtons::closeButton | DocumentWindow::maximiseButton | DocumentWindow::TitleBarButtons::minimiseButton)
        {
            setContentOwned (new MainContentComponent(commandLine), true);

#if JUCE_IOS
            
            Rectangle<int> area = Desktop::getInstance().getDisplays().getMainDisplay().userArea;
            
            setSize(area.getWidth(), area.getHeight());
            
#else
            
			setResizable(true, true);

			setUsingNativeTitleBar(true);


            
            centreWithSize (getWidth(), getHeight() - 28);
            
#endif
            
            setVisible (true);
			
        }

        void closeButtonPressed()
        {
			auto mw = dynamic_cast<MainContentComponent*>(getContentComponent());

            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.

			mw->requestQuit();

            
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    ScopedPointer<MainWindow> mainWindow;
};



//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (HISEStandaloneApplication)
