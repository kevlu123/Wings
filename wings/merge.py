from pathlib import Path
from os import sep
import os

IGNORE = ["main.cpp", "tests.h", "tests.cpp", "cpp.hint"]

seen = set()

def should_ignore(path: Path):
    return str(path).split(sep)[-1] in IGNORE

def get_includes(s: str):
    lines = [x for x in s.splitlines() if x.startswith('#include "')]
    return [x.split()[1].strip('"') for x in lines]

def remove_includes(s: str):
    return "\n".join([
        x for x in s.splitlines()
        if not x.startswith("#include \"")
    ])

def remove_pragma_once(s: str):
    return "\n".join(
        line for line in s.splitlines()
        if not line.startswith("#pragma once")
    ) + "\n"

def add_header_guard(s: str):
    s = remove_pragma_once(s)
    return f"#ifndef WINGS_H\n#define WINGS_H\n\n{s}\n#endif // #ifndef WINGS_H\n"

def process_file(path: Path):
    if str(path) in seen or should_ignore(path):
        return ""
    seen.add(str(path))

    with open(path) as f:
        raw = f.read()

    content = ""
    for include in get_includes(raw):
        content += process_file(path.parent.joinpath(include))
    content += remove_includes(raw)
    return content + "\n\n"

def main():
    # .
    input_path = Path(__file__).parent.absolute()
    # ../single_include
    output_path = Path(__file__).parent.parent.joinpath("single_include").absolute()

    source = [f for f in input_path.glob("*.cpp") if not should_ignore(f)]
    main_header = input_path.joinpath("wings.h")

    os.makedirs(output_path, exist_ok=True)

    fmt = """
{}
///////////////// Implementation ////////////////////////
#ifdef WINGS_IMPL
{}
#endif // #ifdef WINGS_IMPL
"""
    merged = fmt.format(
        process_file(main_header),
        "".join(process_file(f) for f in source)
    )
    
    with open(output_path.joinpath("wings.h"), "w") as f:
        f.write(remove_pragma_once(merged))
        
    print(f"Merged {len(seen)} files.")

main()
