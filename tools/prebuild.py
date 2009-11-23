#!/usr/bin/env python

import os

def read_text_file(path):
    if not os.path.exists(path):
        return ""
    f = open(path, 'r')
    return f.read()[:-1]

def write_text_file(path, contents):
    # read the file first, and only write if the contents are different. This saves
    # us from triggering a rebuild on build systems that check the modified time.
    if contents == read_text_file(path):
        return

    f = open(path, 'w')
    f.write(contents)
    f.write("\n")

def main():
    if not os.path.exists('src/generated'):
        os.mkdir('src/generated')

    # generate setup_builtin_functions.cpp and register_all_tests.cpp
    import generate_cpp_registration
    write_text_file('src/generated/setup_builtin_functions.cpp',
            generate_cpp_registration.do_builtin_functions('src/builtin_functions'))

    write_text_file('src/generated/register_all_tests.cpp',
            generate_cpp_registration.do_register_all_tests('src/tests'))

    def source_directory_into_one_cpp(dir, name):
        generated_cpp = []
        generated_filename = 'src/generated/'+name+'.cpp'
        for path in os.listdir(dir):
            full_path = os.path.join(dir,path)
            if not os.path.isfile(full_path):
                continue

            generated_cpp.append("#include \"../" + full_path + "\"")

        generated_cpp = "\n".join(generated_cpp)
        write_text_file(generated_filename, generated_cpp)

    source_directory_into_one_cpp('src/tests', 'all_tests')
    source_directory_into_one_cpp('src/builtin_functions', 'all_builtin_functions')

if __name__ == '__main__':
    main()
