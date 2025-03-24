import sys
import unittest
import os

# Make sure we're in the server directory
print("Running from:", os.getcwd())

# Create a test loader and runner
loader = unittest.TestLoader()
suite = loader.loadTestsFromModule(sys.modules[__name__])
runner = unittest.TextTestRunner(verbosity=2)

# Run the tests
if __name__ == "__main__":
    result = runner.run(suite)
    # Return non-zero exit code if tests failed
    sys.exit(0 if result.wasSuccessful() else 1) 