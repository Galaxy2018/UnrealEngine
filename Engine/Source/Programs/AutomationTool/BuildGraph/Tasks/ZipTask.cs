﻿using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a zip task
	/// </summary>
	public class ZipTaskParameters
	{
		/// <summary>
		/// The directory to copy from
		/// </summary>
		[TaskParameter]
		public string FromDir;

		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Files;

		/// <summary>
		/// The zip file to create
		/// </summary>
		[TaskParameter]
		public string ZipFile;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.Tag)]
		public string Tag;
	}

	/// <summary>
	/// Task which creates a zip archive
	/// </summary>
	[TaskElement("Zip", typeof(ZipTaskParameters))]
	public class ZipTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ZipTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ZipTask(ZipTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			DirectoryReference FromDir = ResolveDirectory(Parameters.FromDir);

			// Find all the input files
			IEnumerable<FileReference> Files;
			if(Parameters.Files == null)
			{
				Files = FromDir.EnumerateFileReferences("*", System.IO.SearchOption.AllDirectories);
			}
			else
			{
				Files = ResolveFilespec(FromDir, Parameters.Files, TagNameToFileSet);
			}

			// Create the zip file
			FileReference ArchiveFile = ResolveFile(Parameters.ZipFile);
			CommandUtils.ZipFiles(ArchiveFile, FromDir, Files);

			// Apply the optional tag to the produced archive
			if(!String.IsNullOrEmpty(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, Parameters.Tag).Add(ArchiveFile);
			}

			// Add the archive to the set of build products
			BuildProducts.Add(ArchiveFile);
			return true;
		}
	}
}
