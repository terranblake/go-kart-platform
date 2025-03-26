#!/usr/bin/env python3
"""
Run all tests for the dashboard server component
"""
import sys
import unittest
import os
import argparse

def discover_and_run_tests(pattern='test_*.py', verbosity=2, failfast=False, specific_test=None):
    """Discover and run tests matching the specified pattern"""
    # Make sure we're in the server directory
    print(f"Running tests from: {os.getcwd()}")
    
    # Create a test loader
    loader = unittest.TestLoader()
    
    if specific_test:
        # Load specific test file or module
        if os.path.exists(specific_test):
            # It's a file path
            print(f"Running tests from file: {specific_test}")
            suite = loader.discover(os.path.dirname(specific_test), pattern=os.path.basename(specific_test))
        else:
            # It's a module name
            print(f"Running tests from module: {specific_test}")
            suite = loader.loadTestsFromName(specific_test)
    else:
        # Discover all tests in the test directory and subdirectories
        print(f"Discovering tests matching pattern: {pattern}")
        suite = loader.discover('test', pattern=pattern)
    
    # Run the tests
    runner = unittest.TextTestRunner(verbosity=verbosity, failfast=failfast)
    result = runner.run(suite)
    
    # Return non-zero exit code if tests failed
    return 0 if result.wasSuccessful() else 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run dashboard server tests')
    parser.add_argument('--pattern', '-p', default='test_*.py',
                        help='Pattern to match test files (default: test_*.py)')
    parser.add_argument('--verbosity', '-v', type=int, default=2,
                        help='Verbosity level (default: 2)')
    parser.add_argument('--failfast', '-f', action='store_true',
                        help='Stop on first failure')
    parser.add_argument('test', nargs='?', default=None,
                        help='Specific test file or module to run')
    
    args = parser.parse_args()
    sys.exit(discover_and_run_tests(
        pattern=args.pattern,
        verbosity=args.verbosity,
        failfast=args.failfast,
        specific_test=args.test
    )) 