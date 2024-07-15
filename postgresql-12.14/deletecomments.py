#!/usr/bin/env python3
import re
import os,sys

def remove_comments(file_path, output_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    in_multiline_comment = False
    output_lines = []
    
    for line in lines:
#        ori=line
        if in_multiline_comment:
            end_comment_pos = line.find('*/')
            if end_comment_pos != -1:
                in_multiline_comment = False
                
                line = ' ' * (end_comment_pos + 2) + line[end_comment_pos + 2:]
                # # handle case when the last line of comment is `*/ \` in macro
                # if(line.strip()=="\\"):
                #     line = "\n"
                
            else:#in multi line, but not the last line
                # handle case for
                '''
                    /*                                                                                                       \
                     * Convert null-terminated representation of result to standard text.                                    \
                     * The result is usually much bigger than it needs to be, but there                                      \
                     * seems little point in realloc'ing it smaller.                                                         \
                     */                                                                                                      \
                '''
                # where each backslash at the end should retain
                backslash_pos = line.find("\\")
                if(line[backslash_pos:].strip()=="\\"):# \ at the end of the line within a comment block
                    line = ' ' * backslash_pos + "\\\n"
                else:
                    line = ' ' * len(line) + "\n"  # replace entire line with spaces and linebreak
        if not in_multiline_comment:
            start_comment_pos = line.find('/*')
            if start_comment_pos != -1: # have /*
                in_multiline_comment = True
                end_comment_pos = line.find('*/', start_comment_pos)
                if end_comment_pos != -1:
                    in_multiline_comment = False
                    line = line[:start_comment_pos] + ' ' * (end_comment_pos - start_comment_pos + 2) + line[end_comment_pos + 2:]
                else: # /*, without */ in the same line
                    if(line.count('"',0,start_comment_pos)%2==0 or line.count('"',0,start_comment_pos)%2==0): # not in string ('/*' as a charcter)
                        backslash_pos = line.find("\\")
                        if(line[backslash_pos:].strip()=="\\"):# \ at the end of the line within a comment block
                            line = line[:start_comment_pos] + ' ' * (len(line) - start_comment_pos - 2) + "\\\n"
                        else:
                            line = line[:start_comment_pos] + ' ' * (len(line) - start_comment_pos) + '\n'
                    else: # /* is a char, shouldn't remove anything
                        in_multiline_comment = False
            #line = re.sub(r'//.*', lambda match: ' ' * len(match.group()), line)  # replace single-line comments with spaces
#        print(['+']+list(ori),'\n',['-']+list(line))
        output_lines.append(line)  # Always append the modified line, preserving the original structure
    
    with open(output_path, 'w') as file:
        file.writelines(output_lines)

inputf=sys.argv[1]
remove_comments(inputf, inputf)
