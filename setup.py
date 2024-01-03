import os
import pathlib
import shutil
import struct
from distutils.command.install_data import install_data

from setuptools import find_packages, setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.install_lib import install_lib
from setuptools.command.install_scripts import install_scripts

BITS = struct.calcsize("P") * 8


def my_version():
    from setuptools_scm.version import get_local_dirty_tag

    def clean_scheme(version):
        return get_local_dirty_tag(version) if version.dirty else ''

    def version_scheme(version):
        return str(version.format_with('{tag}.{distance}'))

    return {'local_scheme': clean_scheme, 'version_scheme': version_scheme}


class CMakeExtension(Extension):
    """
    An extension to run the cmake build

    This simply overrides the base extension class so that setuptools
    doesn't try to build your sources for you
    """

    def __init__(self, name):
        super().__init__(name=name, sources=[])


class InstallCMakeLibsData(install_data):
    """
    Just a wrapper to get the installed data into the egg-info

    Listing the installed files in the egg-info guarantees that
    all the package files will be uninstalled when the user
    uninstalls your package through pip
    """

    def run(self):
        """
        Output files are the libraries that were built using cmake
        """

        # There seems to be no other way to do this; I tried listing the
        # libraries during the execution of the InstallCMakeLibs.run() but
        # setuptools never tracked them, seems like setuptools wants to
        # track the libraries through package data more than anything...
        # help would be appreciated

        self.outfiles = self.distribution.data_files


class InstallCMakeLibs(install_lib):
    """
    Get the libraries from the parent distribution, use those as the outfiles

    Skip building anything; everything is already built, forward libraries to
    the installation step
    """

    def run(self):
        """
        Copy libraries from the bin directory and place them as appropriate
        """

        self.announce("Moving library files", level=3)

        # We have already built the libraries in the previous build_ext step
        self.skip_build = True

        bin_dir = self.distribution.bin_dir
        lib_dir = self.distribution.package_dir
        libs = [
            os.path.join(bin_dir, _lib) for _lib in os.listdir(bin_dir)
            if os.path.isfile(os.path.join(bin_dir, _lib)) and _lib.endswith('.so')
        ]

        for lib in libs:
            shutil.move(lib, lib_dir)

        self.distribution.data_files = [os.path.join(self.install_dir, os.path.basename(lib)) for lib in libs]

        # Must be forced to run after adding the libs to data_files
        self.distribution.run_command("install_data")
        super().run()


class InstallCMakeScripts(install_scripts):
    """
    Install the scripts in the build dir
    """

    def run(self):
        """
        Copy the required directory to the build directory and super().run()
        """
        self.outfiles = []
        self.announce("Recording Executables", level=3)

        # Scripts were already built in a previous step
        self.skip_build = True

        bin_dir = self.distribution.bin_dir
        scripts = [os.path.join(bin_dir, file) for file in os.listdir(bin_dir) if not file.endswith('.so')]

        # Mark the scripts for installation, adding them to 
        # distribution.scripts seems to ensure that the setuptools' record 
        # writer appends them to installed-files.txt in the package's egg-info

        self.distribution.scripts = scripts
        pathlib.Path(self.install_dir).mkdir(parents=True, exist_ok=True)
        for script in scripts:
            script_path = pathlib.Path(script)
            target = os.path.join(self.install_dir, script_path.name)
            self.outfiles.append(target)
            shutil.copy(script_path, target)


class BuildCMakeExt(build_ext):
    """
    Builds using cmake instead of the python setuptools implicit build
    """

    def run(self):
        """
        Perform build_cmake before doing the 'normal' stuff
        """

        for extension in self.extensions:
            self.build_cmake(extension)

        super().run()

    # def get_ext_fullpath(self, ext_name):
    #     return str(pathlib.Path(super().get_ext_fullpath(ext_name)).parent.parent / f'{ext_name}.so')

    def build_cmake(self, ext):
        self.announce("Preparing the build environment", level=3)
        cwd = pathlib.Path().absolute()

        # these dirs will be created in build_py, so if you don't have
        # any python sources to bundle, the dirs will be missing
        build_temp = pathlib.Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)
        ext_path = pathlib.Path(self.get_ext_fullpath(ext.name))
        ext_path.mkdir(parents=True, exist_ok=True)

        bin_path = ext_path.parent.parent / 'bin'
        bin_path.mkdir(parents=True, exist_ok=True)
        bin_dir = str(bin_path.absolute())

        # example of cmake args
        config = 'Debug' if self.debug else 'Release'
        cmake_args = [
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + str(ext_path.parent.absolute()),
            '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=' + bin_dir,
            '-DCMAKE_BUILD_TYPE=' + config
        ]

        self.announce("Configuring cmake project", level=3)
        build_args = ['--config', config,  '--', '-j4']
        os.chdir(str(build_temp))
        self.spawn(['cmake', str(cwd)] + cmake_args)
        self.spawn(['cmake', '--build', '.'] + build_args)
        # Troubleshooting: if it fails on the line above then delete all possible
        # temporary CMake files including "CMakeCache.txt" in top level dir.
        os.chdir(str(cwd))

        self.announce("Moving built python module", level=3)
        self.distribution.bin_dir = bin_dir
        self.distribution.package_dir = str(ext_path.parent.absolute())


setup(
    name='snob-factor',
    use_scm_version=my_version,
    packages=find_packages(),
    ext_modules=[CMakeExtension(name="snob._snob")],
    cmdclass={
        'build_ext': BuildCMakeExt,
        'install_data': InstallCMakeLibsData,
        'install_lib': InstallCMakeLibs,
        'install_scripts': InstallCMakeScripts
    }
)
