using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;

namespace Packager
{
    public partial class MainFrm : Form
    {
        private class Manifest
        {
            public List<string> References = new List<string>();
            public List<string> Local = new List<string>();
            public List<string> External = new List<string>();
            public List<string> Content = new List<string>();
            public List<string> Code = new List<string>();
        }

        private struct CopyItem : IEquatable<CopyItem>
        {
            public string Source;
            public string Destination;

            public bool Equals(CopyItem otherItem)
            {
                return Source == otherItem.Source && Destination == otherItem.Destination;
            }
        }

        enum ManifestCategory
        {
            Local,
            Externals,
            Content,
            References,
            Code,

            None
        }

        public MainFrm()
        {
            InitializeComponent();

            sourceTextBox.Text = Properties.Settings.Default.CurrentProject;
            outputTextBox.Text = Properties.Settings.Default.CurrentOutput;
            zipPathTextBox.Text = Properties.Settings.Default.CurrentZIP;
        }

        private void SaveSettings()
        {
            Properties.Settings.Default.CurrentProject = sourceTextBox.Text;
            Properties.Settings.Default.CurrentOutput = outputTextBox.Text;
            Properties.Settings.Default.CurrentZIP = zipPathTextBox.Text;
            Properties.Settings.Default.Save();
        }

        private void ClearLog()
        {
            logListBox.Items.Clear();
        }

        private void WriteLog(string log)
        {
            logListBox.Items.Add(log);
            logListBox.Refresh();
        }

        private Manifest ParseManifestFile(string filePath)
        {
            Manifest manifest = new Manifest();
            string[] lines = File.ReadAllLines(filePath);
            ManifestCategory category = ManifestCategory.None;
            for(int lineNum = 1; lineNum <= lines.Length; ++lineNum)
            {
                string line = lines[lineNum - 1];

                if(line.Length == 0)
                    continue;

                if(line.StartsWith("#"))
                    continue;

                if(line.StartsWith("[Content]"))
                {
                    category = ManifestCategory.Content;
                    continue;
                }

                if(line.StartsWith("[Externals]"))
                {
                    category = ManifestCategory.Externals;
                    continue;
                }

                if(line.StartsWith("[Local]"))
                {
                    category = ManifestCategory.Local;
                    continue;
                }

                if(line.StartsWith("[References]"))
                {
                    category = ManifestCategory.References;
                    continue;
                }

                if(line.StartsWith("[Code]"))
                {
                    category = ManifestCategory.Code;
                    continue;
                }

                if(line.StartsWith("["))
                {
                    WriteLog(string.Format("Unrecognized tag '{0}' on line {1}", line, lineNum));
                    continue;
                }

                if(category == ManifestCategory.Local)
                    manifest.Local.Add(line);
                else if(category == ManifestCategory.Content)
                    manifest.Content.Add(line);
                else if(category == ManifestCategory.Externals)
                    manifest.External.Add(line);
                else if(category == ManifestCategory.References)
                    manifest.References.Add(line);
                else if(category == ManifestCategory.Code)
                    manifest.Code.Add(line);
            }

            return manifest;
        }

        private void GenerateCopyItems(string sourceDir, string sourceRootDir, string outputRootDir,
                                       string filter, List<CopyItem> copyItems)
        {
            Debug.Assert(sourceDir.StartsWith(sourceRootDir));
            int rootDirLen = sourceRootDir.Length;
            string outputDir = outputRootDir;
            if(sourceDir.Length > rootDirLen)
            {
                Debug.Assert(sourceDir.ToCharArray()[rootDirLen] == '\\');
                string relativeDir = sourceDir.Substring(rootDirLen + 1);
                outputDir = Path.Combine(outputRootDir, relativeDir);
            }

            string[] filePaths = Directory.GetFiles(sourceDir, filter, SearchOption.TopDirectoryOnly);
            foreach(string filePath in filePaths)
            {
                CopyItem copyItem = new CopyItem();
                copyItem.Source = filePath;
                copyItem.Destination = Path.Combine(outputDir, Path.GetFileName(filePath));
                if(copyItems.Contains(copyItem) == false)
                    copyItems.Add(copyItem);
            }
        }

        private void RemoveCopyItems(string sourceDir, string filter, List<CopyItem> copyItems)
        {
            string[] filePaths = Directory.GetFiles(sourceDir, filter, SearchOption.TopDirectoryOnly);
            foreach(string filePath in filePaths)
            {
                int numItems = copyItems.Count;
                for(int i = numItems - 1; i >= 0; --i)
                {
                    if(copyItems[i].Source == filePath)
                        copyItems.RemoveAt(i);
                }
            }
        }

        private void ExecuteCopyItems(List<CopyItem> copyItems)
        {
            foreach(CopyItem copyItem in copyItems)
            {
                string destinationDir = Path.GetDirectoryName(copyItem.Destination);
                if(Directory.Exists(destinationDir) == false)
                    Directory.CreateDirectory(destinationDir);
                File.Copy(copyItem.Source, copyItem.Destination);
            }
        }

        private void ProcessManifestItem(string sourceDir, string sourceRootDir, string outputRootDir,
                                         string manifestItem, List<CopyItem> copyItems)
        {
            bool include = true;
            if(manifestItem.StartsWith("|"))
            {
                manifestItem = manifestItem.TrimStart("|".ToCharArray());
                include = false;
            }

            if(manifestItem.EndsWith("\\"))
            {
                // It's a directory, recursively process all of its files and nested directories
                string manifestDir = manifestItem.TrimEnd("\\".ToCharArray());
                manifestDir = Path.Combine(sourceDir, manifestDir);
                if(include)
                    GenerateCopyItems(manifestDir, sourceRootDir, outputRootDir, "*.*", copyItems);
                else
                    RemoveCopyItems(manifestDir, "*.*", copyItems);

                string[] subDirs = Directory.GetDirectories(manifestDir, "*", SearchOption.AllDirectories);
                foreach(string subDir in subDirs)
                {
                    if(include)
                        GenerateCopyItems(subDir, sourceRootDir, outputRootDir, "*.*", copyItems);
                    else
                        RemoveCopyItems(subDir, "*.*", copyItems);
                }
            }
            else
            {
                // It's a file or file filter pattern
                string manifestPath = Path.Combine(sourceDir, manifestItem);
                string manifestDir = Path.GetDirectoryName(manifestPath);
                string manifestFilter = Path.GetFileName(manifestPath);

                if(include)
                    GenerateCopyItems(manifestDir, sourceRootDir, outputRootDir, manifestFilter, copyItems);
                else
                    RemoveCopyItems(manifestDir, manifestFilter, copyItems);
            }
        }

        private void ProcessProjectManifest(string sourceRootDir, string outputRootDir, string projectName, bool referenced, List<CopyItem> copyItems)
        {
            string sourceProjectDir = Path.Combine(sourceRootDir, projectName);
            string sourceContentDir = Path.Combine(sourceRootDir, "Content");
            string sourceExternalsDir = Path.Combine(sourceRootDir, "Externals");
            string sourceFrameworkDir = Path.Combine(sourceRootDir, "SampleFramework12");

            string outputProjectDir = Path.Combine(outputRootDir, projectName);
            string outputContentDir = Path.Combine(outputRootDir, "Content");
            string outputExternalsDir = Path.Combine(outputRootDir, "Externals");
            string outputFrameworkDir = Path.Combine(outputRootDir, "SampleFramework12");

            string projectManifestPath = Path.Combine(sourceProjectDir, "Manifest.txt");
            if(File.Exists(projectManifestPath) == false)
            {
                WriteLog("No manifest file found in source directory, aborting");
                return;
            }

            WriteLog("Parsing project manifest file");
            Manifest projectManifest = ParseManifestFile(projectManifestPath);

            List<string> codeFiles = new List<string>() { "*.h", "*.cpp", "*.cs", "*.vcxproj", "*.vcxproj.filters",
                                                          "*.sln", "*.ico", "*.rc", "*.sublime-project", "*.csproj",
                                                          "*.resx", "*.props", "*.config", "*.natvis" };

            List<string> shaderFiles = new List<string>() { "*.hlsl" };

            if(includeCodeCB.Checked)
            {
                WriteLog("Gathering code files");

                if(copyGitIgnoreCB.Checked)
                    codeFiles.Add(".gitignore");

                foreach(string filter in codeFiles)
                    GenerateCopyItems(sourceProjectDir, sourceRootDir, outputRootDir, filter, copyItems);

                foreach(string manifestItem in projectManifest.Code)
                    ProcessManifestItem(sourceProjectDir, sourceRootDir, outputRootDir, manifestItem, copyItems);

                if(copyGitIgnoreCB.Checked && referenced == false)
                {
                    CopyItem gitIgnoreItem = new CopyItem();
                    gitIgnoreItem.Source = Path.Combine(sourceProjectDir, @"..\.gitignore");
                    gitIgnoreItem.Destination = Path.Combine(outputRootDir, @".gitignore");
                    copyItems.Add(gitIgnoreItem);
                }

                WriteLog("Gathering external files");

                foreach(string manifestItem in projectManifest.External)
                    ProcessManifestItem(sourceExternalsDir, sourceRootDir, outputRootDir, manifestItem, copyItems);
            }

            WriteLog("Gathering shader files");

            foreach(string filter in shaderFiles)
                GenerateCopyItems(sourceProjectDir, sourceRootDir, outputRootDir, filter, copyItems);

            if(includeExeCB.Checked && referenced == false)
            {
                string exeDir = Path.Combine(sourceProjectDir, "x64", "Release");
                string exeName = projectName + ".exe";
                string exePath = Path.Combine(exeDir, exeName);
                if(File.Exists(exePath))
                {
                    WriteLog("Gathering executable files");

                    CopyItem exeItem = new CopyItem();
                    exeItem.Source = exePath;
                    exeItem.Destination = Path.Combine(outputProjectDir, exeName);
                    copyItems.Add(exeItem);

                    // Also copy external DLL;s
                    string[] dllPaths = Directory.GetFiles(exeDir, "*.dll", SearchOption.TopDirectoryOnly);
                    foreach(string dllPath in dllPaths)
                    {
                        string dllName = Path.GetFileName(dllPath);

                        CopyItem dllItem = new CopyItem();
                        dllItem.Source = dllPath;
                        dllItem.Destination = Path.Combine(outputProjectDir, dllName);
                        copyItems.Add(dllItem);
                    }

                    GenerateCopyItems(sourceProjectDir, sourceRootDir, outputRootDir, "*.dll", copyItems);                    
                }
                else
                {
                    WriteLog("No executable detected! Skipping.");
                }
            }

            if(includeShaderCacheCB.Checked && referenced == false)
            {
                string cacheDir = Path.Combine(sourceProjectDir, "ShaderCache", "Release");
                if(Directory.Exists(cacheDir))
                {
                    WriteLog("Gathering shader cache files");
                    GenerateCopyItems(cacheDir, sourceRootDir, outputRootDir, "*.cache", copyItems);
                }
                else
                {
                    WriteLog("No shader cache directory found, skipping.");
                }
            }

            foreach(string manifestItem in projectManifest.Local)
                ProcessManifestItem(sourceProjectDir, sourceRootDir, outputRootDir, manifestItem, copyItems);

            WriteLog("Gathering content files");

            foreach(string manifestItem in projectManifest.Content)
                ProcessManifestItem(sourceContentDir, sourceRootDir, outputRootDir, manifestItem, copyItems);

            if(copyGitIgnoreCB.Checked && includeCodeCB.Checked && referenced == false)
            {
                CopyItem contentIgnore;
                contentIgnore.Source = Path.Combine(sourceContentDir, @".gitignore");
                contentIgnore.Destination = Path.Combine(outputContentDir, @".gitignore");
                copyItems.Add(contentIgnore);

                CopyItem externalsIgnore;
                externalsIgnore.Source = Path.Combine(sourceExternalsDir, @".gitignore");
                externalsIgnore.Destination = Path.Combine(outputExternalsDir, @".gitignore");
                copyItems.Add(externalsIgnore);
            }

            foreach(string reference in projectManifest.References)
            {
                WriteLog(string.Format("Processing referenced project '{0}'", reference));
                string referenceDir = Path.Combine(sourceRootDir, reference);
                if(Directory.Exists(referenceDir) == false)
                {
                    WriteLog(string.Format("Referenced project directory '{0}' doesn't exist, or isn't a directory. Skipping.", referenceDir));
                    continue;
                }

                ProcessProjectManifest(sourceRootDir, outputRootDir, reference, true, copyItems);
            }
        }

        private void Package()
        {
            ClearLog();

            if(includeCodeCB.Checked == false && includeExeCB.Checked == false)
            {
                WriteLog("Need to include either code and/or the executable in order to package, aborting.");
                return;
            }

            string invalidChars = new string(Path.GetInvalidPathChars());

            string sourceProjectDir = sourceTextBox.Text;
            if(sourceProjectDir.Contains(invalidChars))
            {
                WriteLog("Invalid path characters in the specified source project path");
                return;
            }

            while(sourceProjectDir.EndsWith("\\") || sourceProjectDir.EndsWith("/"))
                sourceProjectDir = sourceProjectDir.Substring(0, sourceProjectDir.Length - 1);

            if(File.Exists(sourceProjectDir) || Path.HasExtension(sourceProjectDir))
            {
                WriteLog("The source project path must be a directory, not a file");
                return;
            }

            if(Directory.Exists(sourceProjectDir) == false)
            {
                WriteLog(string.Format("Source project directory '{0}' does not exist! Aborting.", sourceProjectDir));
                return;
            }

            if(outputTextBox.Text.Length == 0)
            {
                WriteLog("No output directory specified! Aborting.");
                return;
            }

            string sourceRootDir = Path.GetDirectoryName(sourceProjectDir);
            string sourceContentDir = Path.Combine(sourceRootDir, "Content");
            string sourceExternalsDir = Path.Combine(sourceRootDir, "Externals");
            string sourceFrameworkDir = Path.Combine(sourceRootDir, "SampleFramework12");
            if(Directory.Exists(sourceContentDir) == false)
            {
                WriteLog("Source content directory not found! Aborting.");
                return;
            }

            if(Directory.Exists(sourceExternalsDir) == false)
            {
                WriteLog("Source externals directory not found! Aborting.");
                return;
            }

            if(Directory.Exists(sourceFrameworkDir) == false)
            {
                WriteLog("Source framework directory not found! Aborting.");
                return;
            }

            string projectName = sourceProjectDir.Substring(sourceRootDir.Length + 1);

            string outputRootDir = outputTextBox.Text;
            if(outputRootDir.Contains(invalidChars))
            {
                WriteLog("Invalid path characters in the specified output path");
                return;
            }

            while(outputRootDir.EndsWith("\\") || outputRootDir.EndsWith("/"))
                outputRootDir = outputRootDir.Substring(0, outputRootDir.Length - 1);

            if(File.Exists(outputRootDir) || Path.HasExtension(outputRootDir))
            {
                WriteLog("The output path must be a directory, not a file");
                return;
            }

            if(createZipCB.Checked)
            {
                string outputZipPath = zipPathTextBox.Text;
                if(outputZipPath.Length == 0)
                {
                    WriteLog("No output zip file path specified! Aborting.");
                    return;
                }

                if(File.Exists(outputZipPath))
                {
                    string msg = string.Format("Output ZIP file '{0}' already exists! Overwrite?", outputZipPath);
                    if(MessageBox.Show(this, msg, "Packager", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.No)
                    {
                        WriteLog("Packaging Canceled.");
                        return;
                    }
                }
            }

            if(Directory.Exists(outputRootDir))
            {
                int numFiles = Directory.GetFiles(outputRootDir).Length;
                int numDirs = Directory.GetDirectories(outputRootDir).Length;
                if(numFiles > 0 || numDirs > 0)
                {
                    string msg = string.Format("Output directory '{0}' already exists, continuing will delete all of its contents. Continue?", outputRootDir);
                    if(MessageBox.Show(this, msg, "Packager", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.No)
                    {
                        WriteLog("Packaging canceled.");
                        return;
                    }

                    WriteLog(string.Format("Deleting output directory '{0}'", outputRootDir));
                    Directory.Delete(outputRootDir, true);
                }
            }

            if(Directory.Exists(outputRootDir) == false)
            {
                WriteLog(string.Format("Creating output directory '{0}'", outputRootDir));
                Directory.CreateDirectory(outputRootDir);
            }

            List<CopyItem> copyItems = new List<CopyItem>();
            ProcessProjectManifest(sourceRootDir, outputRootDir, projectName, false, copyItems);

            WriteLog("Copying files");

            ExecuteCopyItems(copyItems);

            if(createZipCB.Checked)
            {
                string outputZipPath = zipPathTextBox.Text;
                if(File.Exists(outputZipPath))
                    File.Delete(outputZipPath);

                WriteLog("Creating ZIP archive");
                ZipFile.CreateFromDirectory(outputRootDir, outputZipPath, CompressionLevel.Optimal, false);

                if(deleteOutputDirCB.Checked)
                {
                    WriteLog("Deleting output directory");
                    Directory.Delete(outputRootDir, true);
                }
            }

            WriteLog("Packaging complete!");
        }

        private void browseProjectButton_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog dlg = new FolderBrowserDialog();
            dlg.Description = "Choose Source Project Folder";
            dlg.SelectedPath = sourceTextBox.Text;
            dlg.ShowNewFolderButton = false;
            if(dlg.ShowDialog(this) == DialogResult.OK)
            {
                sourceTextBox.Text = dlg.SelectedPath;
                SaveSettings();
            }
        }

        private void browseOutputButton_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog dlg = new FolderBrowserDialog();
            dlg.Description = "Choose Output Folder";
            dlg.SelectedPath = outputTextBox.Text;
            dlg.ShowNewFolderButton = true;
            if(dlg.ShowDialog(this) == DialogResult.OK)
            {
                outputTextBox.Text = dlg.SelectedPath;
                SaveSettings();
            }
        }

        private void browseZipButton_Click(object sender, EventArgs e)
        {
            SaveFileDialog dlg = new SaveFileDialog();
            dlg.AddExtension = true;
            dlg.AutoUpgradeEnabled = true;
            dlg.DefaultExt = "zip";
            dlg.Filter = "Zip files (*.zip)|*.zip|All files (*.*)|*.*";
            dlg.FilterIndex = 1;
            if(zipPathTextBox.Text.Length > 0)
                dlg.InitialDirectory = Path.GetDirectoryName(zipPathTextBox.Text);
            dlg.OverwritePrompt = true;
            dlg.SupportMultiDottedExtensions = true;
            dlg.Title = "Choose ZIP File Location...";
            if(dlg.ShowDialog(this) == DialogResult.OK)
            {
                zipPathTextBox.Text = dlg.FileName;
                SaveSettings();
            }
        }

        private void packageButton_Click(object sender, EventArgs e)
        {
            SaveSettings();
            Package();
        }

        private void createZipCB_CheckedChanged(object sender, EventArgs e)
        {
            bool enabled = createZipCB.Checked;
            deleteOutputDirCB.Enabled = enabled;
            browseZipButton.Enabled = enabled;
            zipPathTextBox.Enabled = enabled;
        }

        private void includeCodeCB_CheckedChanged(object sender, EventArgs e)
        {
            bool enabled = includeCodeCB.Checked;
            copyGitIgnoreCB.Enabled = enabled;
        }
    }
}
