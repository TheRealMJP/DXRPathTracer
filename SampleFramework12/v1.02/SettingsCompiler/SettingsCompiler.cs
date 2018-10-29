using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Reflection;
using System.CodeDom.Compiler;
using Microsoft.CSharp;

namespace SettingsCompiler
{
    class SettingsCompiler
    {
        const uint CBufferRegister = 12;

        static Assembly CompileSettings(string inputFilePath)
        {
            string fileName = Path.GetFileNameWithoutExtension(inputFilePath);

            string code = File.ReadAllText(inputFilePath);
            code = "using SettingsCompiler;\r\n\r\n" + "namespace " + fileName + "\r\n{\r\n" + code;
            code += "\r\n}";

            Dictionary<string, string> compilerOpts = new Dictionary<string, string> { { "CompilerVersion", "v4.0" } };
            CSharpCodeProvider compiler = new CSharpCodeProvider(compilerOpts);

            string exePath = Assembly.GetEntryAssembly().Location;
            string exeDir = Path.GetDirectoryName(exePath);
            string dllPath = Path.Combine(exeDir, "SettingsCompilerAttributes.dll");

            string[] sources = { code };
            CompilerParameters compilerParams = new CompilerParameters();
            compilerParams.GenerateInMemory = true;
            compilerParams.ReferencedAssemblies.Add("System.dll");
            compilerParams.ReferencedAssemblies.Add(dllPath);
            CompilerResults results = compiler.CompileAssemblyFromSource(compilerParams, sources);
            if(results.Errors.HasErrors)
            {
                string errMsg = "Errors were returned from the C# compiler:\r\n\r\n";
                foreach(CompilerError compilerError in results.Errors)
                {
                    int lineNum = compilerError.Line - 4;
                    errMsg += inputFilePath + "(" + lineNum + "): " + compilerError.ErrorText + "\r\n";
                }
                throw new Exception(errMsg);
            }

            return results.CompiledAssembly;
        }

        static void ReflectType(Type settingsType, List<Setting> settings, List<Type> enumTypes,
                                Dictionary<string, object> constants, string group)
        {
            object settingsInstance = Activator.CreateInstance(settingsType);

            BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
            FieldInfo[] fields = settingsType.GetFields(flags);
            foreach(FieldInfo field in fields)
            {
                foreach(Setting setting in settings)
                    if(setting.Name == field.Name)
                        throw new Exception(string.Format("Duplicate setting \"{0}\" detected", setting.Name));

                if(field.IsLiteral)
                {
                    if (field.FieldType == typeof(float) ||
                        field.FieldType == typeof(int) ||
                        field.FieldType == typeof(uint) ||
                        field.FieldType == typeof(bool))
                    {
                        if (constants.ContainsKey(field.Name))
                            throw new Exception(string.Format("Duplicate constant \"{0}\" detected", field.Name));
                        constants.Add(field.Name, field.GetValue(null));
                    }
                    else
                        throw new Exception(string.Format("Invalid constant type \"{0}\" detected for field {1}", field.FieldType, field.Name));

                    continue;
                }
                else if(field.IsStatic)
                    continue;

                Type fieldType = field.FieldType;
                object fieldValue = field.GetValue(settingsInstance);
                if(fieldType == typeof(float))
                    settings.Add(new FloatSetting((float)fieldValue, field, group));
                else if(fieldType == typeof(int))
                    settings.Add(new IntSetting((int)fieldValue, field, group));
                else if(fieldType == typeof(bool))
                    settings.Add(new BoolSetting((bool)fieldValue, field, group));
                else if(fieldType.IsEnum)
                {
                    if(enumTypes.Contains(fieldType) == false)
                        enumTypes.Add(fieldType);
                    settings.Add(new EnumSetting(fieldValue, field, fieldType, group));
                }
                else if(fieldType == typeof(Direction))
                    settings.Add(new DirectionSetting((Direction)fieldValue, field, group));
                else if(fieldType == typeof(Orientation))
                    settings.Add(new OrientationSetting((Orientation)fieldValue, field, group));
                else if(fieldType == typeof(Color))
                    settings.Add(new ColorSetting((Color)fieldValue, field, group));
                else if(fieldType == typeof(Button))
                    settings.Add(new ButtonSetting(field, group));
                else
                    throw new Exception("Invalid type for setting " + field.Name);
            }
        }

        static void ReflectSettings(Assembly assembly, string inputFilePath, List<Setting> settings, List<Type> enumTypes,
                                    Dictionary<string, object> constants, List<SettingGroup> groups)
        {
            string filePath = Path.GetFileNameWithoutExtension(inputFilePath);
            Type settingsType = assembly.GetType(filePath + ".Settings", false);
            if(settingsType == null)
                throw new Exception("Settings file " + inputFilePath + " doesn't define a \"Settings\" class");

            ReflectType(settingsType, settings, enumTypes, constants, "");

            Type[] nestedTypes = settingsType.GetNestedTypes();
            foreach(Type nestedType in nestedTypes)
            {
                string groupName = DisplayNameAttribute.GetDisplayName(nestedType);
                bool expandGroup = ExpandGroupAttribute.ExpandGroup(nestedType.GetTypeInfo());
                groups.Add(new SettingGroup(groupName, expandGroup));
                ReflectType(nestedType, settings, enumTypes, constants, groupName);
            }
        }

        static void WriteIfChanged(List<string> lines, string outputPath)
        {
            string outputText = "";
            foreach(string line in lines)
                outputText += line + "\r\n";

            string fileText = "";
            if(File.Exists(outputPath))
                fileText = File.ReadAllText(outputPath);

            int idx = fileText.IndexOf("// ================================================================================================");
            if(idx >= 0)
                outputText += "\r\n" + fileText.Substring(idx);

            if(fileText != outputText)
                File.WriteAllText(outputPath, outputText);
        }

        public static void WriteEnumTypes(List<string> lines, List<Type> enumTypes)
        {
            foreach(Type enumType in enumTypes)
            {
                if(enumType.GetEnumUnderlyingType() != typeof(int))
                    throw new Exception("Invalid underlying type for enum " + enumType.Name + ", must be int");
                string[] enumNames = enumType.GetEnumNames();
                int numEnumValues = enumNames.Length;

                Array values = enumType.GetEnumValues();
                int[] enumValues = new int[numEnumValues];
                for(int i = 0; i < numEnumValues; ++i)
                    enumValues[i] = (int)values.GetValue(i);

                lines.Add("enum class " + enumType.Name);
                lines.Add("{");
                for(int i = 0; i < values.Length; ++i)
                    lines.Add("    " + enumNames[i] + " = " + enumValues[i] + ",");
                lines.Add("\r\n    NumValues");

                lines.Add("};\r\n");

                lines.Add("typedef EnumSettingT<" + enumType.Name + "> " + enumType.Name + "Setting;\r\n");
            }
        }

        public static void WriteEnumLabels(List<string> lines, List<Type> enumTypes)
        {
            foreach(Type enumType in enumTypes)
            {
                string[] enumNames = enumType.GetEnumNames();
                int numEnumValues = enumNames.Length;
                string[] enumLabels = new string[numEnumValues];

                for(int i = 0; i < numEnumValues; ++i)
                {
                    FieldInfo enumField = enumType.GetField(enumNames[i]);
                    EnumLabelAttribute attr = enumField.GetCustomAttribute<EnumLabelAttribute>();
                    enumLabels[i] = attr != null ? attr.Label : enumNames[i];
                }

                lines.Add("static const char* " + enumType.Name + "Labels[] =");
                lines.Add("{");
                foreach(string label in enumLabels)
                    lines.Add("    \"" + label + "\",");

                lines.Add("};\r\n");
            }
        }

        static void GenerateHeader(List<Setting> settings, string outputName, string outputPath,
                                   List<Type> enumTypes, Dictionary<string, object> constants)
        {
            List<string> lines = new List<string>();

            lines.Add("#pragma once");
            lines.Add("");
            lines.Add("#include <PCH.h>");
            lines.Add("#include <Settings.h>");
            lines.Add("#include <Graphics\\GraphicsTypes.h>");
            lines.Add("");
            lines.Add("using namespace SampleFramework12;");
            lines.Add("");

            WriteEnumTypes(lines, enumTypes);

            lines.Add("namespace " + outputName);
            lines.Add("{");

            foreach(KeyValuePair<string, object> constant in constants)
            {
                Type constantType = constant.Value.GetType();
                string typeStr = constantType.ToString();
                string valueStr = constant.Value.ToString();
                if(constantType == typeof(uint))
                    typeStr = "uint64";
                else if(constantType == typeof(int))
                    typeStr = "int64";
                else if(constantType == typeof(bool))
                {
                    typeStr = "bool";
                    valueStr = valueStr.ToLower();
                }
                else if(constantType == typeof(float))
                {
                    typeStr = "float";
                    valueStr = FloatSetting.FloatString((float)constant.Value);
                }
                lines.Add(string.Format("    static const {0} {1} = {2};", typeStr, constant.Key, valueStr));
            }

            lines.Add("");

            uint numCBSettings = 0;
            foreach(Setting setting in settings)
            {
                setting.WriteDeclaration(lines);
                if(setting.UseAsShaderConstant)
                    ++numCBSettings;
            }


            lines.Add("");
            lines.Add(string.Format("    struct {0}CBuffer",  outputName));
            lines.Add("    {");


            if(numCBSettings > 0)
            {
                uint cbSize = 0;
                foreach(Setting setting in settings)
                    setting.WriteCBufferStruct(lines, ref cbSize);
            }
            else
            {
                lines.Add("        uint32 Dummy;");
            }

            lines.Add("    };");
            lines.Add("");
            lines.Add("    extern ConstantBuffer CBuffer;");
            lines.Add("    const extern uint32 CBufferRegister;");

            lines.Add("");
            lines.Add("    void Initialize();");
            lines.Add("    void Shutdown();");
            lines.Add("    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix);");
            lines.Add("    void UpdateCBuffer();");
            lines.Add("    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);");
            lines.Add("    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);");

            lines.Add("};");

            WriteIfChanged(lines, outputPath);
        }

        static void GenerateCPP(List<Setting> settings, string outputName, string outputPath,
                                List<Type> enumTypes, List<SettingGroup> groups)
        {
            List<string> lines = new List<string>();

            lines.Add("#include <PCH.h>");
            lines.Add("#include \"" + outputName + ".h\"");
            lines.Add("");
            lines.Add("using namespace SampleFramework12;");
            lines.Add("");

            WriteEnumLabels(lines, enumTypes);

            lines.Add("namespace " + outputName);
            lines.Add("{");

            lines.Add("    static SettingsContainer Settings;");
            lines.Add("");

            foreach(Setting setting in settings)
                setting.WriteDefinition(lines);

            lines.Add("");
            lines.Add("    ConstantBuffer CBuffer;");
            lines.Add(string.Format("    const uint32 CBufferRegister = {0};", CBufferRegister));

            lines.Add("");
            lines.Add("    void Initialize()");
            lines.Add("    {");
            lines.Add("");

            lines.Add(string.Format("        Settings.Initialize({0});", groups.Count));
            lines.Add("");

            foreach(SettingGroup group in groups)
            {
                lines.Add(string.Format("        Settings.AddGroup(\"{0}\", {1});", group.Name, group.Expand ? "true" : "false"));
                lines.Add("");
            }

            foreach(Setting setting in settings)
            {
                setting.WriteInitialization(lines);
                setting.WritePostInitialization(lines);
            }

            lines.Add("        ConstantBufferInit cbInit;");
            lines.Add("        cbInit.Size = sizeof(AppSettingsCBuffer);");
            lines.Add("        cbInit.Dynamic = true;");
            lines.Add("        cbInit.Name = L\"AppSettings Constant Buffer\";");
            lines.Add("        CBuffer.Initialize(cbInit);");

            lines.Add("    }");

            lines.Add("");
            lines.Add("    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix)");
            lines.Add("    {");

            lines.Add("        Settings.Update(displayWidth, displayHeight, viewMatrix);");
            lines.Add("");

            foreach(Setting setting in settings)
                setting.WriteVirtualCode(lines);

            lines.Add("    }");

            lines.Add("");
            lines.Add("    void UpdateCBuffer()");
            lines.Add("    {");
            lines.Add("        AppSettingsCBuffer cbData;");

            foreach(Setting setting in settings)
                setting.WriteCBufferUpdate(lines);


            lines.Add("");
            lines.Add("        CBuffer.MapAndSetData(cbData);");

            lines.Add("    }");


            lines.Add("    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)");
            lines.Add("    {");

            lines.Add("        CBuffer.SetAsGfxRootParameter(cmdList, rootParameter);");

            lines.Add("    }");

            lines.Add("    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)");
            lines.Add("    {");

            lines.Add("        CBuffer.SetAsComputeRootParameter(cmdList, rootParameter);");

            lines.Add("    }");

            lines.Add("    void Shutdown()");
            lines.Add("    {");

            lines.Add("        CBuffer.Shutdown();");

            lines.Add("    }");

            lines.Add("}");

            WriteIfChanged(lines, outputPath);
        }

        static void GenerateHLSL(List<Setting> settings, string outputName, string outputPath,
                                 List<Type> enumTypes, Dictionary<string, object> constants)
        {
            uint numCBSettings = 0;
            foreach(Setting setting in settings)
            {
                if(setting.UseAsShaderConstant)
                    ++numCBSettings;
            }

            List<string> lines = new List<string>();

            if(numCBSettings == 0)
                WriteIfChanged(lines, outputPath);

            lines.Add(string.Format("struct {0}_Layout", outputName));
            lines.Add("{");

            foreach(Setting setting in settings)
                setting.WriteHLSL(lines);

            lines.Add("};");
            lines.Add("");

            lines.Add(string.Format("ConstantBuffer<{0}_Layout> {0} : register(b{1});", outputName, CBufferRegister));
            lines.Add("");

            foreach (Type enumType in enumTypes)
            {
                string[] enumNames = enumType.GetEnumNames();
                Array enumValues = enumType.GetEnumValues();
                for(int i = 0; i < enumNames.Length; ++i)
                {
                    string line = "static const int " + enumType.Name + "_";
                    line += enumNames[i] + " = " + (int)enumValues.GetValue(i) + ";";
                    lines.Add(line);
                }

                lines.Add("");
            }

            foreach(KeyValuePair<string, object> constant in constants)
            {
                Type constantType = constant.Value.GetType();
                string typeStr = constantType.ToString();
                string valueStr = constant.Value.ToString();
                if(constantType == typeof(uint))
                    typeStr = "uint";
                else if(constantType == typeof(int))
                    typeStr = "int";
                else if(constantType == typeof(bool))
                {
                    typeStr = "bool";
                    valueStr = valueStr.ToLower();
                }
                else if(constantType == typeof(float))
                {
                    typeStr = "float";
                    valueStr = FloatSetting.FloatString((float)constant.Value);
                }
                lines.Add(string.Format("static const {0} {1} = {2};", typeStr, constant.Key, valueStr));
            }

            WriteIfChanged(lines, outputPath);
        }

        static void Run(string[] args)
        {
            if(args.Length < 1)
                throw new Exception("Invalid command-line parameters");

            List<Setting> settings = new List<Setting>();
            List<Type> enumTypes = new List<Type>();
            Dictionary<string, object> constants = new Dictionary<string, object>();
            List<SettingGroup> groups = new List<SettingGroup>();

            string filePath = args[0];
            string fileName = Path.GetFileNameWithoutExtension(filePath);

            Assembly compiledAssembly = CompileSettings(filePath);
            ReflectSettings(compiledAssembly, filePath, settings, enumTypes, constants, groups);

            string outputDir = Path.GetDirectoryName(filePath);
            string outputPath = Path.Combine(outputDir, fileName) + ".h";
            GenerateHeader(settings, fileName, outputPath, enumTypes, constants);

            outputPath = Path.Combine(outputDir, fileName) + ".cpp";
            GenerateCPP(settings, fileName, outputPath, enumTypes, groups);

            outputPath = Path.Combine(outputDir, fileName) + ".hlsl";
            GenerateHLSL(settings, fileName, outputPath, enumTypes, constants);

            // Generate a dummy file that MSBuild can use to track dependencies
            outputPath = Path.Combine(outputDir, fileName) + ".deps";
            File.WriteAllText(outputPath, "This file is output to allow MSBuild to track dependencies");
        }

        static int Main(string[] args)
        {

            if(Debugger.IsAttached)
            {
                Run(args);
            }
            else
            {
                try
                {
                    Run(args);
                }
                catch(Exception e)
                {
                    Console.WriteLine("An error ocurred during settings compilation:\n\n" + e.Message);
                    return 1;
                }
            }

            return 0;
        }
    }
}