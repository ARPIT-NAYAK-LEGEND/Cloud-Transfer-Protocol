from setuptools import setup, find_packages

setup(
    name="utp-client",
    version="2.5.0",
    packages=find_packages(),
    install_requires=[
        "requests>=2.25.0",
    ],
    author="UTP Project",
    description="Python SDK for the Universal Cloud Transfer Protocol (UTP)",
    python_requires=">=3.7",
)
