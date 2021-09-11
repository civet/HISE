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

	static File getFilePathArgument(const StringArray& args)
	{
		auto s = getArgument(args, "-p:");

		if (s.isNotEmpty() && File::isAbsolutePath(s))
			return File(s);

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
		print("Arguments: " );
		print("FILE      The path to the project file (either .xml or .hip you want to export)." );
		print("          In CI mode, this will be the relative path from the current project folder");
		print("          In standard mode, it must be an absolute path");
		print("-h:{TEXT} sets the HISE path. Use this if you don't have compiler settings set." );
		print("-ipp      enables Intel Performance Primitives for fast convolution." );
		print("-l        This can be used to compile a version that runs on legacy CPU models.");
		print("-t:{TEXT} sets the project type ('standalone' | 'instrument' | 'effect' | 'midi')" );
		print("-p:{TEXT} sets the plugin type ('VST' | 'AU' | 'VST_AU' | 'AAX' | 'ALL')" );
		print("          (Leave empty for standalone export)" );
		print("-a:{TEXT} sets the architecture ('x86', 'x64', 'x86x64')." );
		print("          (Leave empty on OSX for Universal binary.)" );
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
        print("create-win-installer [-a:x64|x86] [-noaax] [-rlottie]" );
		print("Creates a template install script for Inno Setup for the project" );
		print("Add the -noaax flag to not include the AAX build");
        print("Add the -rlottie flag to include the .dlls for RLottie.");
        print("(You'll need to put them into the AdditionalSourceCode directory");
        print("Add the -a:x64 or -a:x86 flag to just create an installer for the specified platform");

		exit(0);
	}

	static void createWindowsInstallerFile(const String& commandLine)
	{
		auto args = getCommandLineArgs(commandLine);
		
		ScopedPointer<StandaloneProcessor> sp = new StandaloneProcessor();
		ScopedPointer<MainController> mc = dynamic_cast<MainController*>(sp->createProcessor());

		const bool includeAAX = !args.contains("-noaax");
        
        const bool includeRlottie = args.contains("-rlottie");
        const bool include32 = !args.contains("-a:x64");
        const bool include64 = !args.contains("-a:x86");

		auto content = BackendCommandTarget::Actions::createWindowsInstallerTemplate(mc, includeAAX, include32, include64, includeRlottie);

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
	
	static void setProjectFolder(const String& commandLine)
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

		auto compilerSettings = NativeFileHandler::getAppDataDirectory().getChildFile("compilerSettings.xml");

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
