import pathlib
import re
import os
from os import path, sep

IGNORE = ["main.cpp", "tests.h", "tests.cpp", "cpp.hint"]

cur_path = pathlib.Path(__file__).parent.absolute()
input_folder = f"{str(cur_path.absolute())}{sep}"
h_output_folder = f"{str(cur_path)}{sep}..{sep}single_include{sep}"
hs_output_folder = f"{str(cur_path)}{sep}..{sep}header_and_source{sep}"

filenames = [
    str(f.absolute()) for f in pathlib.Path(input_folder).glob("*.[ch]*")
    if not f.name.startswith("test.")
]
source_files = [f for f in filenames if f.endswith(".cpp")]
seen = set()

os.makedirs(h_output_folder, exist_ok=True)
os.makedirs(hs_output_folder, exist_ok=True)

def get_includes(s):
    lines = [x for x in s.splitlines() if x.startswith("#include \"")]
    return [x.split()[1].strip('"') for x in lines]

def remove_includes(s):
    return "\n".join([
        x for x in s.splitlines()
        if not x.startswith("#include \"")
    ])

def process_file(filename, inline):
    if filename in seen or filename.split(sep)[-1] in IGNORE:
        return ""
    seen.add(filename)

    with open(filename) as f:
        raw = f.read()

    content = ""
    for include in get_includes(raw):
        content += process_file(os.path.abspath(input_folder + include), inline)
    content += remove_includes(raw)
    if inline:
        content = inline_symbols(content)
    return content + "\n\n"

def needs_inline(s: str):
    # Skip macros
    if s.startswith("#define"):
        return False
    
    # Module initialization code strings
    if "_CODE " in s:
        return True
    # Functions
    if (re.match(r".+[a-zA-Z0-9_]+.+[a-zA-Z0-9_]+\(.*\).+\{", s)
        and s.count("(") + s.count(")") == 2):
        return True
    # Constructors
    if (s.endswith(":") and "case " not in s and "default:" not in s
        and len(s.split()) > 1 and "//" not in s):
        return True
    # Overloaded operators
    if "::operator" in s:
        return True
    # Constant collections
    if "static const std::unordered_" in s or "static const std::vector<" in s:
        return True
    # Forward declared functions
    if "static void Compile" in s or "static CodeError Parse" in s:
        return True
    # Misc
    if "std::atomic<Wg_ErrorCallback> errorCallback;" in s or "static thread_local " in s:
        return True

    return False

def inline_symbols(s):
    lines = []
    in_multiline_string = False
    for line in s.splitlines():
        if "R\"(" in line:
            in_multiline_string = True
        if ")\";" in line:
            in_multiline_string = False

        if in_multiline_string or "inline " in line or not needs_inline(line):
            lines.append(line)
        elif "static " in line:
            lines.append(line.replace("static", "inline"))
        else:
            idx = len(line) - len(line.lstrip())
            lines.append(line[:idx] + "inline " + line[idx:])
    return "\n".join(lines)

def get_wings_header():
    for filename in filenames:
        if filename.endswith("wings.h"):
            return filename
    raise Exception

def remove_pragma_once(s):
    return "\n".join(line for line in s.splitlines() if not line.startswith("#pragma once"))

def add_header_guard(s):
    s = remove_pragma_once(s)
    return f"#ifndef WINGS_H\n#define WINGS_H\n\n{s}\n#endif // #ifndef WINGS_H\n"

def generate_header_only():
    seen.clear()
    wings_header = get_wings_header()
    output = process_file(wings_header, True)
    output += "///////////////// Implementation ////////////////////////\n\n"
    for f in source_files:
        output += process_file(f, True)
    
    with open(h_output_folder + "wings.h", "w") as f:
        f.write(add_header_guard(output))

def generate_header_and_source():
    seen.clear()
    wings_header = get_wings_header()
    output = process_file(wings_header, False)
    output += "///////////////// Implementation ////////////////////////\n\n"
    for f in source_files:
        output += process_file(f, False)
    
    with open(hs_output_folder + "wings.cpp", "w") as f:
        f.write(remove_pragma_once(output))
    with open(hs_output_folder + "wings.h", "w") as f:
        with open(wings_header) as rf:
            f.write(add_header_guard(rf.read()))

generate_header_only()
generate_header_and_source()
print(f"Merged {len(seen)} files.")
