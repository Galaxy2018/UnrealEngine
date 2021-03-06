// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using UnrealBuildTool;

public class Python : ModuleRules
{
	public Python(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string PythonRoot = null;
		string PythonIncludePath = null;
		string PythonLibPath = null;
		string PythonLibName = null;
		
		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			// Check for an explicit version before using the auto-detection logic
			PythonRoot = System.Environment.GetEnvironmentVariable("UE_PYTHON_DIR");
		}

		// Perform auto-detection to try and find the Python root
		if (PythonRoot == null)
		{
			var KnownPaths = new List<string>();

			// todo: This isn't correct for cross-compilation, we need to consider the host platform too
			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				KnownPaths.AddRange(
					new string[] {
						//"C:/Program Files/Python36",
						"C:/Python27",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				KnownPaths.AddRange(
					new string[] {
						"/Library/Frameworks/Python.framework/Versions/2.7",
						//"/System/Library/Frameworks/Python.framework/Versions/2.7",
					}
				);
			}

			foreach (var KnownPath in KnownPaths)
			{
				if (Directory.Exists(KnownPath))
				{
					PythonRoot = KnownPath;
					break;
				}
			}
		}

		// Work out the include path
		if (PythonRoot != null)
		{
			PythonIncludePath = Path.Combine(PythonRoot, (Target.Platform == UnrealTargetPlatform.Mac) ? "Headers" : "include");
			if (!Directory.Exists(PythonIncludePath))
			{
				PythonRoot = null;
			}
		}

		// Work out the lib path
		if (PythonRoot != null)
		{
			string LibFolder = null;
			string LibNamePattern = null;
			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win32:
				case UnrealTargetPlatform.Win64:
					LibFolder = "libs";
					LibNamePattern = "python*.lib";
					break;

				case UnrealTargetPlatform.Mac:
					LibFolder = "lib";
					LibNamePattern = "libpython*.dylib";
					break;

				case UnrealTargetPlatform.Linux:
					LibFolder = "lib";
					LibNamePattern = "libpython*.so";
					break;

				default:
					break;
			}

			if (LibFolder != null && LibNamePattern != null)
			{
				PythonLibPath = Path.Combine(PythonRoot, LibFolder);

				if (Directory.Exists(PythonLibPath))
				{
					string[] MatchingLibFiles = Directory.GetFiles(PythonLibPath, LibNamePattern);
					if (MatchingLibFiles.Length > 0)
					{
						PythonLibName = Path.GetFileName(MatchingLibFiles[0]);
					}
				}
			}

			if (PythonLibPath == null || PythonLibName == null)
			{
				PythonRoot = null;
			}
		}

		// Make sure the Python install is the correct architecture
		if (PythonRoot != null)
		{
			string ExpectedPointerSizeResult = Target.Platform == UnrealTargetPlatform.Win32 ? "4" : "8";

			// Invoke Python to query the pointer size of the interpreter so we can work out whether it's 32-bit or 64-bit
			// todo: We probably need to do this for all platforms, but right now it's only an issue on Windows
			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				string PythonBinary = Path.Combine(PythonRoot, "python");
				string Result = InvokePython(PythonBinary, "-c \"import struct; print(struct.calcsize('P'))\"");
				Result = Result != null ? Result.Replace("\r", "").Replace("\n", "") : null;
				if (Result == null || Result != ExpectedPointerSizeResult)
				{
					PythonRoot = null;
				}
			}
		}

		if (PythonRoot == null)
		{
			PublicDefinitions.Add("WITH_PYTHON=0");
		}
		else
		{
			PublicDefinitions.Add("WITH_PYTHON=1");
			PublicDefinitions.Add(string.Format("UE_PYTHON_DIR=\"{0}\"", PythonRoot.Replace('\\', '/')));

			// Some versions of Python need this define set when building on MSVC
			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("HAVE_ROUND=1");
			}

			PublicSystemIncludePaths.Add(PythonIncludePath);
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				// Mac doesn't understand PublicLibraryPaths
				PublicAdditionalLibraries.Add(Path.Combine(PythonLibPath, PythonLibName));
			}
			else
			{
				PublicLibraryPaths.Add(PythonLibPath);
				PublicAdditionalLibraries.Add(PythonLibName);
			}
		}
	}

	private string InvokePython(string InPythonBinary, string InPythonArgs)
	{
		ProcessStartInfo ProcStartInfo = new ProcessStartInfo();
		ProcStartInfo.FileName = InPythonBinary;
		ProcStartInfo.Arguments = InPythonArgs;
		ProcStartInfo.UseShellExecute = false;
		ProcStartInfo.RedirectStandardOutput = true;

		try
		{
			using (Process Proc = Process.Start(ProcStartInfo))
			{
				using (StreamReader StdOutReader = Proc.StandardOutput)
				{
					return StdOutReader.ReadToEnd();
				}
			}
		}
		catch
		{
			return null;
		}
	}
}
