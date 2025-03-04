#! /usr/bin/env python3

import argparse
import difflib
import filecmp
import glob
import os
import subprocess
import sys

# Test convert4qmc for GAMESS output files

# Each test directory contains
# *.inp - Gamess input file.  Not strictly necessary for this test, but useful for reproducing the run
# *.out - Gamess output file.  Serves as input to convert4qmc
# One of the following
#  gold.wfnoj.xml - expected version of output from convert4qmc
#  'expect_fail.txt' - if present, converter should fail

# Only the wavefunction conversion is tested currently.
# Structure conversion (gold.structure.xml) is not tested.

def compare(gold_file,test_file):
        if not filecmp.cmp(gold_file, test_file):
            print("Gold file comparison failed")
            with open(gold_file, 'r') as f_gold:
                gold_lines = f_gold.readlines()
            with open(test_file, 'r') as f_test:
                test_lines = f_test.readlines()

            diff = difflib.unified_diff(gold_lines, test_lines, fromfile=gold_file, tofile=test_file)
            diff_line_limit = 200
            for i,diff_line in enumerate(diff):
                print(diff_line,end="")
                if i > diff_line_limit:
                    print('< diff truncated due to line limit >')
                    break

            return False
        else:
            return True


def run_test(test_name, c4q_exe, h5diff_exe, conv_inp, gold_file, expect_fail, extra_cmd_args,code):
    okay = True

    # Example invocation of converter
    #convert4qmc -nojastrow -prefix gold -gamess be.out

    cmd = c4q_exe.split()
    if code=='generic':
        cmd.extend(['-nojastrow', '-prefix', 'test', '-orbitals', conv_inp])
    if code=='gamess':
        cmd.extend(['-nojastrow', '-prefix', 'test', '-gamess', conv_inp])
    if code=='dirac':
        cmd.extend(['-nojastrow', '-prefix', 'test', '-TargetState','14','-dirac', conv_inp])
    if code=='rmg':
        cmd.extend(['-nojastrow', '-prefix', 'test', '-rmg', conv_inp])

    for ex_arg in extra_cmd_args:
        if ex_arg == '-ci':
            cmd.extend(['-ci', conv_inp])
        else:
            if ex_arg == '-multidet':
                cmd.extend(['-multidet', conv_inp])
            else:
                cmd.append(ex_arg)

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    stdout, stderr = p.communicate()

    file_out = open('stdout.txt', 'w')
    file_out.write(stdout)
    file_out.close()
    if len(stderr) > 0 :
        file_err = open('stderr.txt', 'w')
        file_err.write(stderr)
        file_err.close()

    ret = p.returncode

    if expect_fail:
        if ret == 0:
            print("Return code zero, but expected failure")
    else:
        if ret != 0:
            print("Return code nonzero: ", ret)
            okay = False

        if len(stderr.strip()) != 0:
            print("Stderr not empty:")
            print(stderr)

        if not os.path.exists(gold_file):
            print("Gold file missing")
            okay = False
        else:
            if (code != 'generic'): 
                if '-hdf5' in extra_cmd_args or code=='dirac':
                   ret = os.system(h5diff_exe + ' -d 0.000001 gold.orbs.h5 test.orbs.h5')
                   # if it's okay up to this point
                   if ret==0 and okay:
                      print("  pass")
                      return True
                   else:
                      print("h5diff reported a difference")
                      print("  FAIL")
                      return False
            test_file = gold_file.replace('gold', 'test')
            okay_compare = compare(gold_file, test_file) 
            # test compare always, but only report if it fails
            if okay:
                okay = okay_compare

    if okay:
        print("  pass")
    else:
        print("  FAIL")

    return okay

def read_extra_args():
    extra_cmd_args = []
    if os.path.exists('cmd_args.txt'):
        with open('cmd_args.txt', 'r') as f_cmd_args:
            for line in f_cmd_args:
                line = line.strip()
                if line.startswith('#'):
                    continue
                extra_cmd_args.append(line)
    return extra_cmd_args


def run_one_converter_test(c4q_exe, h5diff_exe):
    code='gamess'
    if os.path.exists('orbitals'):
       code='generic'
    
    test_name = os.path.split(os.getcwd())[-1]

    if 'dirac' in test_name:
        code='dirac'

    if 'rmg' in test_name:
        code='rmg'

    if code=='gamess' or code=='dirac': 
       conv_input_files = glob.glob('*.out')

    if code=='rmg': 
       conv_input_files = glob.glob('*.h5')

    if code=='generic': 
       conv_input_files = glob.glob('*.h5')

    if len(conv_input_files) != 1:
        print("Unexpected number of inputs files (should be 1): ",
              len(conv_input_files))
        return False
    conv_input_file = conv_input_files[0]

    extra_cmd_args = read_extra_args()

    expect_fail = os.path.exists('expect_fail.txt')
    gold_file = 'gold.wfnoj.xml'
    if expect_fail:
        gold_file = ''
    else:
        if not os.path.exists(gold_file):
            print("Gold file missing")
            return False
    return run_test(test_name, c4q_exe, h5diff_exe, conv_input_file, gold_file,
                    expect_fail, extra_cmd_args,code)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Test convert4qmc')
    parser.add_argument('test_name',
                        help='Name of test to run (name of directory)')
    parser.add_argument('--exe',
                        default='convert4qmc',
                        help='Location of convert4qmc executable')
    parser.add_argument('--h5diff',
                        default='h5diff',
                        help='Location of h5diff executable')
    args = parser.parse_args()

    test_dir = args.test_name
    if not os.path.exists(test_dir):
        print("Test not found: ", test_dir)
        sys.exit(1)

    curr_dir = os.getcwd()
    os.chdir(test_dir)

    ret = run_one_converter_test(args.exe, args.h5diff)

    os.chdir(curr_dir)

    if ret:
        sys.exit(0)
    sys.exit(1)
