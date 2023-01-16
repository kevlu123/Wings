from common import *
import bindgen

#    C                         Managed                     Unmanaged

RETURN = {
    "void":                     ("void",                    "void"),
    "bool":                     ("bool",                    "byte"),
    "int":                      ("int",                     "int"),
    "Wg_int":                   ("long",                    "long"),
    "Wg_float":                 ("float",                   "float"),
    "Wg_Obj*":                  ("Obj",                     "Obj"),
    "Wg_Context*":              ("Context",                 "Context"),
    "const char*":              ("string",                  "IntPtr"),
    "void*":                    ("IntPtr",                  "IntPtr"),
}

IN_PARAM = {
    "const Wg_Config*":         ("Config?",                 "IntPtr"),
    "const Wg_Context*":        ("Context",                 "Context"),
    "Wg_Context*":              ("Context",                 "Context"),
    "const char*":              ("string",                  "IntPtr"),
    "Wg_ErrorCallback":         ("ErrorCallback",           "ErrorCallback"),
    "Wg_Function":              ("Function",                "Function"),
    "Wg_Finalizer":             ("Finalizer",               "Finalizer"),
    "Wg_IterationCallback":     ("IterationCallback",       "IterationCallback"),
    "Wg_ModuleLoader":          ("ModuleLoader",            "ModuleLoader"),
    "Wg_Exc":                   ("Exc",                     "Exc"),
    "Wg_UnOp":                  ("UnOp",                    "UnOp"),
    "Wg_BinOp":                 ("BinOp",                   "BinOp"),
    "Wg_Obj*":                  ("Obj",                     "Obj"),
    "const Wg_Obj*":            ("Obj",                     "Obj"),
    "bool":                     ("bool",                    "byte"),
    "int":                      ("int",                     "int"),
    "Wg_int":                   ("long",                    "long"),
    "Wg_float":                 ("float",                   "float"),
    "Wg_Obj*const*":            ("Obj[]",                   "IntPtr"),
    "Wg_Obj**":                 ("Obj[]",                   "IntPtr"),
    "void*":                    ("IntPtr",                  "IntPtr"),
    "const char*const*":        ("string[]",                "IntPtr"),
}

OUT_PARAM = {
    "Wg_Config*":               ("out Config",              "out Wg_ConfigNative"),
    "int*":                     ("out int",                 "out int"),
    "void**":                   ("out IntPtr",              "out IntPtr"),
    "Wg_Obj**":                 ("Obj[]",                   "[In, Out] Obj[]"),
}

FIELDS = {
    "int":                      ("int",                     "int"),
    "float":                    ("float",                   "float"),
    "bool":                     ("bool",                    "byte"),
    "void*":                    ("IntPtr",                  "IntPtr"),
    "Wg_PrintFunction":         ("PrintFunction?",          "IntPtr"),
    "const char*":              ("string?",                 "IntPtr"),
    "const char*const*":        ("string[]?",               "IntPtr"),
}

class Param:
    def make(param):
        if param.type.is_output:
            return OutParam(param)
        else:
            return InParam(param)

    def get_managed_param(self):
        return f"{self.managed} {self.in_name}{self.default}"

    def get_unmanaged_param(self):
        return f"{self.unmanaged} {self.in_name}"

class OutParam(Param):
    def __init__(self, param):
        type = param.type.raw
        self.setup = []
        self.cleanup = []
        self.managed, self.unmanaged = OUT_PARAM[type]
        self.default = ""
        self.in_name = param.name
        self.out_name = "out " + param.name

        if type == "Wg_Obj**":
            self.out_name = param.name
        elif type == "Wg_Config*":
            self.out_name = f"out var _{param.name}"
            self.cleanup = [
                f"{param.name} = MakeWg_Config(_{param.name});",
            ]

class InParam(Param):
    def __init__(self, param):
        type = param.type.raw
        self.in_name = param.name
        self.out_name = param.name
        self.setup = []
        self.cleanup = []
            
        # Managed/unmanaged type names
        self.managed, self.unmanaged = IN_PARAM[type]

        # Special types
        if type == "bool":
            self.out_name = f"(byte)({self.in_name} ? 1 : 0)"
        elif type == "const char*":
            self.out_name = "(IntPtr)_" + param.name
            self.setup = [
                f"fixed (byte* _{param.name} = {self.in_name} is null ? null : Encoding.ASCII.GetBytes({self.in_name} + '\\0')) {{",
            ]
        elif self.managed == "string[]":
            self.out_name = "(IntPtr)_" + param.name
            self.setup = [
                f"var handles = {self.in_name}.Select(x => "
                + f"GCHandle.Alloc(Encoding.ASCII.GetBytes(x + '\\0'), GCHandleType.Pinned)).ToArray();",
                
                f"fixed(void* _{param.name} = handles.Select(x => x.AddrOfPinnedObject()).ToArray()) {{",
            ]
            self.cleanup = [
                "foreach (var handle in handles) handle.Free();",
            ]
        elif self.managed.endswith("[]"):
            self.out_name = "(IntPtr)_" + param.name
            self.setup = [
                f"fixed ({self.managed[:-2]}* _{param.name} = {self.in_name}) {{",
            ]
        elif type == "const Wg_Config*":
            self.setup = [
                f"var _{param.name} = {self.in_name} is null ? new() : new Wg_ConfigNative({self.in_name}.Value);",
            ]
            self.out_name = f"{param.name} is null ? IntPtr.Zero : new IntPtr(&_{self.in_name})"
            self.cleanup = [
                f"if ({self.in_name} != null) {{",
                f"_{param.name}.Free({self.in_name}.Value);",
                "}",
            ]

        # Default argument
        if param.default:
            self.default = " = default"
            if self.managed == "string" or self.managed.endswith("[]"):
                self.managed += "?"
        else:
            self.default = ""
            
        self.cleanup.extend("}" for s in self.setup if s.strip().endswith("{"))

class CSBindGen:
    def __init__(self):
        self.f = open(PROJECT_ROOT.joinpath("bindings").joinpath("wings.cs"), "w")
        self.tabs = 0

    def close(self):
        self.f.close()

    def write(self, s):
        if s.strip().startswith("}"):
            self.dedent()
        self.f.write(self.tabs * '\t' + s + '\n')
        if s.strip().endswith("{"):
            self.indent()
    
    def indent(self):
        self.tabs += 1

    def dedent(self):
        self.tabs -= 1

    def gen(self):
        defs = bindgen.parse()
        self.write("using System.Text;")
        self.write("using System.Runtime.InteropServices;\n")
        self.write("namespace Wings {")
        self.write("public static class Wg {")

        self.write_ptr_newtype("Context")
        self.write_ptr_newtype("Obj")

        self.write_calling_convention()
        self.write("public delegate Obj Function(Context context, IntPtr argv, int argc);")
        self.write_calling_convention()
        self.write("public delegate void Finalizer(IntPtr userdata);")
        self.write_calling_convention()
        self.write("public delegate void PrintFunction(IntPtr message, int len, IntPtr userdata);")
        self.write_calling_convention()
        self.write("public delegate void ErrorCallback(IntPtr message);")
        self.write_calling_convention()
        self.write("public delegate void IterationCallback(Obj obj, IntPtr userdata);")
        self.write_calling_convention()
        self.write("[return: MarshalAs(UnmanagedType.U1)]")
        self.write("public delegate bool ModuleLoader(Context context);")
        self.write("\n")

        for struct in defs.structs:
            self.write_struct(struct)

        for enum in defs.enums:
            self.write_enum(enum)
        
        for func in defs.functions:
            self.write_func(func)

        self.write("}")
        self.write("}")

    def write_calling_convention(self):
        self.write("[UnmanagedFunctionPointer(CallingConvention.Cdecl)]")

    def write_ptr_newtype(self, typename):
        self.write(f"public struct {typename} {{")
        self.write(f"public IntPtr _data;")
        self.write(f"public static implicit operator bool({typename} a) => a._data != IntPtr.Zero;")
        self.write(f"public static bool operator==({typename} a, {typename} b) => a._data == b._data;")
        self.write(f"public static bool operator!=({typename} a, {typename} b) => !(a == b);")
        self.write(f"public override bool Equals(Object? a) => a is {typename} b && this == b;")
        self.write(f"public override int GetHashCode() => _data.GetHashCode();")
        self.write("}\n")
        
    def write_enum(self, enum):
        cutoff = enum.values[0].name.index('_', 3) + 1
        self.write(f"public enum {enum.name[3:]} {{")
        for value in enum.values:
            self.write_doc(value.doc)
            self.write(f"{value.name[cutoff:]},")
        self.write("}\n")

    def write_struct(self, struct):
        self.write("[StructLayout(LayoutKind.Sequential, Pack=1)]")
        self.write(f"private struct {struct.name}Native {{")
        for field in struct.fields:
            self.write(f"public {FIELDS[field.type.raw][1]} {field.name};")

        self.write(f"public {struct.name}Native({struct.name[3:]} src) {{")
        for field in struct.fields:
            if field.type.raw == "const char*":
                self.write(f"{field.name} = Marshal.StringToHGlobalAnsi(src.{field.name});")
            elif field.type.raw == "const char*const*":
                self.write(f"if (src.{field.name} is null) {{")
                self.write(f"{field.name} = default;")
                self.write("} else {")
                self.write(f"{field.name} = Marshal.AllocHGlobal(src.{field.name}.Length * IntPtr.Size);")
                self.write(f"for (int i = 0; i < src.{field.name}.Length; i++) {{")
                self.write(f"Marshal.WriteIntPtr({field.name}, i * IntPtr.Size, Marshal.StringToHGlobalAnsi(src.{field.name}[i]));")
                self.write("}")
                self.write("}")
            elif field.type.raw == "Wg_PrintFunction":
                self.write(f"{field.name} = src.{field.name} is null ? IntPtr.Zero : Marshal.GetFunctionPointerForDelegate(src.{field.name});")
            elif field.type.raw == "bool":
                self.write(f"{field.name} = (byte)(src.{field.name} ? 1 : 0);")
            else:
                self.write(f"{field.name} = src.{field.name};")
        self.write("}")

        self.write(f"public void Free({struct.name[3:]} src) {{")
        for field in struct.fields:
            if field.type.raw == "const char*":
                self.write(f"Marshal.FreeHGlobal({field.name});")
            elif field.type.raw == "const char*const*":
                self.write(f"if (src.{field.name} != null) {{")
                self.write(f"for (int i = 0; i < src.{field.name}.Length; i++) {{")
                self.write(f"Marshal.FreeHGlobal(Marshal.ReadIntPtr({field.name}, i * IntPtr.Size));")
                self.write("}")
                self.write(f"Marshal.FreeHGlobal({field.name});")
                self.write("}")
        self.write("}")

        self.write("}\n")
        
        self.write(f"private static {struct.name[3:]} Make{struct.name}({struct.name}Native src) {{")
        self.write(f"var dst = new {struct.name[3:]}();")
        for field in struct.fields:
            if field.type.raw in ("const char*", "const char*const*"):
                self.write(f"dst.{field.name} = null;")
            elif field.type.raw == "Wg_PrintFunction":
                self.write(f"dst.{field.name} = src.{field.name} == IntPtr.Zero ? null : Marshal.GetDelegateForFunctionPointer<PrintFunction>(src.{field.name});")
            elif field.type.raw == "bool":
                self.write(f"dst.{field.name} = src.{field.name} != 0;")
            else:
                self.write(f"dst.{field.name} = src.{field.name};")
        self.write("return dst;")
        self.write("}")

        self.write(f"public struct {struct.name[3:]} {{")
        for field in struct.fields:
            self.write_doc(field.doc)
            self.write(f"public {FIELDS[field.type.raw][0]} {field.name};")
        self.write("}\n")
        
    def write_func(self, func):
        self.write_doc(func.doc)

        processed_params = [Param.make(p) for p in func.params]

        managed_ret, unmanaged_ret = RETURN[func.return_type.raw]
        s = f"public static {managed_ret} {func.name[3:]}("
        s += ", ".join([p.get_managed_param() for p in processed_params]) + ") {"
        self.write(s)
        self.write("unsafe {")

        if func.return_type.raw != "void":
            self.write(f"{managed_ret} r;")

        for p in processed_params:
            for line in p.setup:
                self.write(line)

        args = ", ".join(p.out_name for p in processed_params)
        if func.return_type.raw == "void":
            self.write(f"{func.name}({args});")
        elif func.return_type.raw == "const char*":
            self.write(f"r = Marshal.PtrToStringAnsi({func.name}({args}))!;")
        elif func.return_type.raw == "bool":
            self.write(f"r = {func.name}({args}) != 0;")
        else:
            self.write(f"r = {func.name}({args});")

        for p in processed_params:
            for line in p.cleanup:
                self.write(line)
                
        if func.return_type.raw != "void":
            self.write(f"return r;")

        self.write("}")
        self.write("}\n")

        self.write(f'[DllImport("wings", CallingConvention = CallingConvention.Cdecl)]')
        s = f"private static extern unsafe {unmanaged_ret} {func.name}("
        s += ", ".join(p.get_unmanaged_param() for p in processed_params)
        s += ");\n"
        self.write(s)
        
    def write_xml(self, tag, content, name=None):
        if content:
            if name:
                self.write(f"/// <{tag} name=\"{name}\">")
            else:
                self.write(f"/// <{tag}>")
            content = [s.replace("Wg_", "") for s in content]
            content = [s.replace("NULL", "null") for s in content]
            for line in content:
                self.write(f"/// {line}")
            self.write(f"/// </{tag}>")

    def write_doc(self, doc):
        self.write_xml("summary", doc.brief)
        for param in doc.params:
            self.write_xml("param", param.desc, param.name)
        self.write_xml("returns", doc.ret)
        self.write_xml("see", doc.see)
        self.write_xml("remarks", doc.note)
        self.write_xml("remarks", doc.warning)
        self.write_xml("remarks", doc.attention)

CSBindGen().gen()
