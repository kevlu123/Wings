from common import *

class EndReachedError(Exception):
    pass

class Doc:
    def __init__(self):
        self.brief = None
        self.desc = None
        self.note = None
        self.warning = None
        self.attention = None
        self.params = []
        self.ret = None
        self.see = []
        self.file = None
    
    def __str__(self):
        s = "DOC"

        def append(name, li):
            nonlocal s
            if li:
                s += "\n " + name
                for x in li:
                    s += "\n  " + str(x)

        append("FILE", self.file)
        append("BRIEF", self.brief)
        append("DESC", self.desc)
        append("NOTE", self.note)
        append("WARNING", self.warning)
        append("ATTENTION", self.attention)
        append("RETURN", self.ret)
        append("PARAM", self.params)
        append("SEE", self.see)
        return s

class DocParam:
    def __init__(self, name, desc, is_output):
        self.name = name
        self.desc = desc
        self.is_output = is_output

    def __str__(self):
        s = self.name
        if self.is_output:
            s += " (out)"
        for x in self.desc:
            s += "\n   " + x
        return s

class StructField:
    def __init__(self, name, doc, type):
        self.name = name
        self.doc = doc
        self.type = type

class StructDef:
    def __init__(self, name, doc):
        self.name = name
        self.doc = doc
        self.fields = []

class EnumValue:
    def __init__(self, name, doc):
        self.name = name
        self.doc = doc

class EnumDef:
    def __init__(self, name, doc):
        self.name = name
        self.doc = doc
        self.values = []

class Param:
    def __init__(self, name, type, default):
        self.name = name
        self.type = type
        self.default = default

class Func:
    def __init__(self, name, doc, return_type):
        self.name = name
        self.doc = doc
        self.return_type = return_type
        self.params = []

class Defs:
    def __init__(self):
        self.file_doc = None
        self.structs = []
        self.enums = []
        self.functions = []

class Type:
    def __init__(self, s, is_output):
        self.raw = s
        s = s.replace("void*", "voidp")
        if is_output:
            s = s[:-1]
        self.is_output = is_output
        self.is_ptr = s.endswith("*")
        self.is_array = s.count("*") == 2
        self.name = s.replace("*", "").replace("const", "").strip()
        if self.name == "voidp":
            self.name = "void*"

class Parser:
    def __init__(self, path):
        self.lines = [line.strip(" \t\n") for line in open(path).readlines()]
        self.i = 0
    
    def current(self):
        if self.i >= len(self.lines) or self.lines[self.i].startswith("#undef"):
            raise EndReachedError
        return self.lines[self.i]

    def move_next(self):
        self.i += 1
    
    def move_back(self):
        self.i -= 1
    
    def parse(self):
        defs = Defs()
        try:
            defs.file_doc = self.read_doc()
            while True:
                doc = self.read_doc()
                self.move_to_decl()
                line = self.current()
                if "typedef struct" in line and line.endswith("{"):
                    struct = self.read_struct(doc)
                    defs.structs.append(struct)
                elif "typedef enum" in line:
                    enum = self.read_enum(doc)
                    defs.enums.append(enum)
                elif "typedef" in line:
                    pass
                elif line.startswith("WG_DLL_EXPORT"):
                    func = self.read_func(doc)
                    defs.functions.append(func)
                else:
                    raise NotImplementedError(self.current())
        except EndReachedError:
            return defs

    def read_func(self, doc):
        self.move_next()
        br = self.current().index("(")
        name = self.current()[:br].split()[-1]
        return_type = self.current()[:br - len(name) - 1]
        func = Func(name, doc, Type(return_type, False))
        param_list = self.current()[br + 1:-2].split(", ")
        for i, param in enumerate(param_list):
            split = param.split(" ")
            if split[-1].startswith("WG_DEFAULT_ARG"):
                default = split[-1][15:-1]
                param_name = split[-2]
                param_type = Type(" ".join(split[:-2]), doc.params[i].is_output)
            else:
                default = None
                param_name = split[-1]
                param_type = Type(" ".join(split[:-1]), doc.params[i].is_output)
            func.params.append(Param(param_name, param_type, default))
        self.move_next()
        self.move_next()
        return func

    def read_struct(self, doc):
        name = self.current().split()[2]
        struct = StructDef(name, doc)
        while "}" not in self.current():
            doc = self.read_doc()
            split = self.current()[:-1].split()
            field = split[-1]
            type = Type(" ".join(split[:-1]), False)
            self.move_next()
            struct.fields.append(StructField(field, doc, type))
        return struct
    
    def read_enum(self, doc):
        name = self.current().split()[2]
        enum = EnumDef(name, doc)
        while "}" not in self.current():
            doc = self.read_doc()
            enum_value = self.current()[:-1]
            self.move_next()
            enum.values.append(EnumValue(enum_value, doc))
        return enum

    def move_to(self, *args):
        while all(s not in self.current() for s in args):
            self.move_next()
    
    def move_to_prop(self):
        self.move_to("@")

    def move_to_doc(self):
        self.move_to("/**")
    
    def move_to_decl(self):
        self.move_to("typedef", "WG_DLL_EXPORT")
        while self.current().startswith("#define"):
            self.move_next()
            self.move_to("typedef", "WG_DLL_EXPORT")

    def read_doc(self):
        doc = Doc()
        self.move_to_doc()
        while "*/" not in self.current():
            prop, val = self.read_prop()
            if prop == "@brief":
                doc.brief, doc.desc = split_brief_and_desc(val)
            elif prop == "@param":
                p = DocParam(*split_first_word_from_list(val), False)
                doc.params.append(p)
            elif prop == "@param[out]":
                p = DocParam(*split_first_word_from_list(val), True)
                doc.params.append(p)
            elif prop == "@return":
                doc.ret = val
            elif prop == "@see":
                doc.see = val[0].split(", ")
            elif prop == "@note":
                doc.note = val
            elif prop == "@warning":
                doc.warning = val
            elif prop == "@attention":
                doc.attention = val
            elif prop == "@file":
                doc.file = val
            else:
                raise NotImplementedError(prop)
        self.move_next()
        return doc
    
    def read_prop(self):
        self.move_to_prop()
        lines = self.read_prop_lines()
        lines = [s.strip(" *\t") for s in lines]

        while not lines[0]:
            lines.pop(0)
        while not lines[-1]:
            lines.pop()
            
        return split_first_word_from_list(lines)

    def read_prop_lines(self):
        lines = [self.current()]
        self.move_next()
        while all(s not in self.current() for s in ("@", "*/")):
            lines.append(self.current())
            self.move_next()
        return lines

def split_first_word_from_list(li):
    s = li[0]
    space_index = s.index(' ')
    a, b = s[:space_index], s[space_index + 1:]
    li[0] = b
    return a, li

def split_brief_and_desc(li):
    if "" not in li:
        return li, None
    i = li.index("")
    return li[:i], li[i + 1:]

def parse():
    return Parser(PROJECT_ROOT.joinpath("single_include").joinpath("wings.h")).parse()
