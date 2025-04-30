from setuptools import setup, find_packages

setup(
    name='go_kart_shared',
    version='0.1.0',
    description='Shared Python libraries for Go-Kart Platform (CAN, Telemetry)',
    author='Your Name / Team',
    packages=find_packages(where='.'),
    package_dir={'': '.'},
    # Add dependencies if any are strictly required by the shared lib itself
    # install_requires=[
    #     'cffi>=1.15', 
    # ],
) 