from setuptools import setup, find_packages

setup(
    name="utp-client",
    version="2.6.2",
    packages=find_packages(),
    install_requires=[
        "requests>=2.25.0",
    ],
    author="UTP Project",
    description="Python SDK for the Universal Cloud Transfer Protocol (UTP) [Experimental Post-MVP Surface]",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: System :: Networking",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.7",
)
