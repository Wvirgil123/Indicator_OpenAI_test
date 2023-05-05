# Assistant indicator

A brief description of the project.

## Installation and Usage

Instructions on how to install and use the project.

### Installation

If using Windows, you would need those basic tools below:
git, cmake, make, python3(also pip3), vscode(optional)

#### IDF and IDF-Tools
using esp-idf tools installer to setup the environment, which helps you install all the compile tools and python packages. You don't need to install them one by one and the git repo.
- [ESP-IDF Tools Installer](https://dl.espressif.com/dl/esp-idf/), download v5.0 or v5.0.1 version.
For detailed information, please refer to [Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
### Usage

####  Projection structure
the component is from Seeed Studio company, which is from the repo [SenseCAP_Indicator_ESP32](https://github.com/Seeed-Solution/SenseCAP_Indicator_ESP32).
the structure is like this:
```bash
├── components
│   ├── assistant_indicator
│   │   ├── CMakeLists.txt
│   │   ├── include
│   │   │   ├── assistant_indicator.h
│   │   │   ├── assistant_indicator_config.h
├── main
│   ├── controller
│   ├── model
│   ├── ui
│   ├── util
│   ├── view
│   ├── config.h
│   ├── Kconfig.projbuild
│   ├── main.c
│   ├── CMakeLists.txt
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig
├── sdkconfig.defaults
```

### Possible errors
Q: `spi_timing_config.h:174:1: error: static assertion failed: "FLASH and PSRAM Mode configuration are not supported"   174 | _Static_assert(CHECK_POWER_OF_2(SPI_TIMING_CORE_CLOCK_MHZ / SPI_TIMING_PSRAM_MODULE_CLOCK), "FLASH and PSRAM Mode configuration are not supported");`
A:
IDF Patch, refer to [details](https://github.com/Seeed-Solution/sensecap_indicator_esp32/blob/main/examples/factory/README.md)
if your idf-version is v5.x version, just patch the idf, which is 'idf_psram_120m.patch' inclued in this repo.
```bash
# go to the idf path (which is the root path of the idf, git repo)
git apply <this_project_path>/idf_psram_120m.patch
```

## Features

List the main features and use cases of the project.
- OpenAI
- DELL

## Examples

Provide an example of how to use the project and the output.

## Development and Contribution

Instructions on how to contribute to the project.

### Development

How to set up a development environment and run tests.

```
$ npm install
$ npm test
```

### Contribution

How to contribute to the project and submit code changes.

1. Fork the project
2. Create a new branch (`git checkout -b feature/fooBar`)
3. Commit your changes (`git commit -am 'Add some fooBar'`)
4. Push to the branch (`git push origin feature/fooBar`)
5. Create a new Pull Request

## License

The license information for the project.

---

You can modify and expand the above template according to your project needs. Please note that the README is an important part of your project, as it can help users understand and use your project better, so please write your README file as detailed and clear as possible.