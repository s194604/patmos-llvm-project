import os
import subprocess
import sys
from shutil import which

if which("patmos-ld") is None:
    print("Patmos port of the Gold linker 'patmos-ld' could not be found.")
    sys.exit(1)
    
if which("pasim") is None:
    print("Patmos simulator 'pasim' could not be found.")
    sys.exit(1)

first_exec_arg_index = 6
if len(sys.argv) <= (first_exec_arg_index + 1):
    print("Must have at least 2 execution arguments but was:", sys.argv[first_exec_arg_index:])
    sys.exit(1)
   
# Parse arguments
   
# Takes the path to one LLVM build binary and extracts the directory
bin_dir = os.path.dirname(sys.argv[1])

# The source file to test
source_to_test = sys.argv[2]

# Substitute source file
substitute_source = sys.argv[3]

if substitute_source != "":
    source_to_test = substitute_source
    
# The object file of the program
compiled = sys.argv[4]   

# The object file of the start function to be linked with the program
start_function = sys.argv[5]

# The first execution argument
exec_arg = sys.argv[first_exec_arg_index]

# It cleans the given pasim statistics
# leaving only the stats needed to ensure two run of a singlepath
# program are identical (execution-wise).
def pasim_stat_clean(stats):
    input = stats.splitlines()
    output = ""
    currentLine = 0;
    #Find the instruction statistics
    while input[currentLine].strip() != "Instruction Statistics:":
        if input[currentLine].strip() == "Pasim options:":
            raise ValueError("No pasim statistics given.")
        currentLine+=1
    currentLine +=2 #Discard the found header and the following line
    
    #output cleaned instruction statistics
    while not input[currentLine].strip().startswith("all:"):
        split_line = input[currentLine].split()
        name = split_line[0]
        fetch_count = int(split_line[1]) + int(split_line[4])
        output += name + " " + str(fetch_count)+"\n"
        currentLine+=1
    currentLine+=1 # Discard the "all:"
    
    #Find and output cycle count
    while not input[currentLine].strip().startswith("Cycles:"):
        currentLine+=1
    split = input[currentLine].split()
    output += split[0] + " " + split[1] + "\n"
    currentLine+=1
    
    #Find profiling information
    while not input[currentLine].strip().startswith("Profiling information:"):
        currentLine+=1
    #Discard the header and next 3 lines, which are just table headers
    currentLine+=4
            
    #Output how many times each function is called
    while True:
        line = input[currentLine].strip()
        currentLine+=1
        if line.startswith("<"):
            currentLine+=1 #Discard next line
            count = input[currentLine].strip().split()[0]
            currentLine+=1
            output += line[1:].split(">")[0] + "(): " + count + "\n"
        else:
            #Not part of the profiling
            break;
    
    return output
    
# Executes the given program (arg 1), running statistics on the given function (arg 2).
# Argument 3 is the execution arguments (see top of file for description).
# Tests that the output of the program match the expected output. If not, reports an error.
# Returns the cleaned statistics.
def execute_and_stat(program, args):
	# Split the execution argument into input and expected output.
    split = args.split("=", 1)
    input = split[0]
    expected_out = split[1]
    
    # The final name of the ELF to execute
    exec_name = program + input
    
    # Final generation of ELF with added input
    if subprocess.run(["patmos-ld", "-nostdlib", "-static", "-o", exec_name, program, 
        "--defsym", "input=" + input]).returncode != 0:
        return True, args + "\nFailed to generate executable from '" + program + "' for argument '" + input + "'"
        
    pasim_result = subprocess.run(["pasim", exec_name, "-V", "-D", "ideal"], 
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    program_output=pasim_result.stdout
    pasim_stats=pasim_result.stderr
    
    if int(expected_out) != pasim_result.returncode:
        sys.stderr.write("The execution of '" + program + "' for input argument '" + input + "' gave the wrong output through stdout.\n")
        sys.stderr.write("-------------------- Expected --------------------\n")
        sys.stderr.write(expected_out + "\n")
        sys.stderr.write("--------------------- Actual ---------------------\n")
        sys.stderr.write(str(pasim_result.returncode) + "\n")
        sys.stderr.write("--------------------- stdout ---------------------\n")
        sys.stderr.write(program_output + "\n")
        sys.stderr.write("--------------------- stderr ---------------------\n")
        sys.stderr.write(pasim_stats + "\n")
        sys.stderr.write("--------------------------------------------------\n")
        return True, ""
    
    # Clean 'pasim's statistics
    try: 
        return False, pasim_stat_clean(pasim_stats)
    except ValueError:
		# If cleaning failed it means the stderr is not statistics and some other
		# error was printed
        sys.stderr.write("--------------------- stderr ---------------------\n")
        sys.stderr.write(pasim_stats + "\n")
        sys.stderr.write("--------------------------------------------------\n")
        return True, ""
       
# Compile and test using the given LLC arguments.
def compile_and_test(llc_args):
    def throw_error(*msgs):
        for msg in msgs:
            print(msg, end = '')
        print("\nLLC args: ", llc_args)
        sys.exit(1)
        
    using_singlepath = "-mpatmos-singlepath=" in llc_args
        
    # Try to compile the program to rule out compile errors. Throw out the result.
    if subprocess.run([bin_dir+"/llc", source_to_test] + llc_args.split() + ["-filetype=obj", "-o", compiled]).returncode != 0:
        throw_error("Failed to compile ", source_to_test)

    # Link start function with program
    if subprocess.run([bin_dir+"/llvm-link", start_function, source_to_test, "-o", compiled]).returncode != 0:
        throw_error("Failed to link '", source_to_test, "' and '", start_function, "'")
        
    # Compile into object file (not ELF yet)
    if subprocess.run([bin_dir+"/llc", compiled] + llc_args.split() + ["-filetype=obj", "-o", compiled]).returncode != 0:
        throw_error("Failed to compile '", source_to_test, "'")

     
    # Run the first execution argument on its own,
    # such that its stats result can be compared to
    # all other executions
    first_stats_failed, first_stats=execute_and_stat(compiled, exec_arg)
    if first_stats_failed:
        throw_error(first_stats)

      
    # Run the rest of the execution arguments.
    # For each one, compare to the first. If they all
    # are equal to the first, they must also be equal to each other,
    # so we don't need to compare them to each other.
    for i in sys.argv[first_exec_arg_index:]:
            
        rest_failed, rest_stats=execute_and_stat(compiled, i)
        if rest_failed:
            throw_error()
        
        if using_singlepath:
            if first_stats != rest_stats:
                import difflib
                sys.stderr.writelines(difflib.context_diff(first_stats.split("\n"), rest_stats.split("\n")))
                throw_error("The execution of '", compiled, "' for execution arguments '", exec_arg, "' and '", i, "' weren't equivalent")

# Compile and test all compinations in the given matrix
# Each list in the matrix is a group of flags that each
# should be compled/test with every element in the other groups.
#
# The matrix argument are added to the given arguments
def compile_and_test_matrix(llc_args, matrix):
    if len(matrix) == 0:
        compile_and_test(llc_args)
    else:
        for arg in matrix[0]:
            compile_and_test_matrix(llc_args + " " + arg, matrix[1:])

compile_and_test_matrix("", [
    [
        "", # Traditional code
        "-mpatmos-singlepath=main", # Single-path code without dual-issue
        "-mpatmos-singlepath=main -mpatmos-disable-vliw=false" # Single-path with dual-issue
    ],
    # Optimization levels
    ["", "-O1", "-O2"]
])

# Success
sys.exit(0)
