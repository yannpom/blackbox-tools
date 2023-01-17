from distutils.core import setup, Extension


def main():
    module = Extension("_blackbox_tools", ["src/python_decode.c", "src/parser.c", "src/tools.c", "src/platform.c", "src/stream.c", "src/decoders.c", "src/units.c", "src/blackbox_fielddefs.c", "src/imu.c", "src/gpxwriter.c", "src/battery.c", "src/stats.c"])
    setup(
        name="BlackBoxDecoder",
        version="0.0.1",
        description="Betaflight BlackBox Decoder in python",
        author="Yann",
        author_email="yann.pomarede@gmail.com",
        packages=['blackbox'],
        ext_modules=[module],
        install_requires=[
            "numpy",
            "scipy",
            "pandas",
        ],
    )

if (__name__ == "__main__"):
    main()
