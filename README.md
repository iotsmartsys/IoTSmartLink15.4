| Supported Target | ESP32-H2 |
| ---------------- | -------- |

# Hello World Example

Starts a FreeRTOS task to print "Hello World".

This is the repository's root diagnostic project, kept from the ESP-IDF
`hello_world` template. It is bound to ESP32-H2, the only target it may be
configured for, per `docs/specs/Repository-Test-Execution-Policy.md`
(TESTEXEC-008). The repository as a whole admits ESP32-H2 and ESP32-C6 only;
the generic target table of the original template was never this project's
policy. The `linux` cases in `pytest_hello_world.py` are the host-native
exception of TESTEXEC-003: pure logic, no firmware, no board.

## How to use example

Follow detailed instructions provided specifically for this example.

The board must be an ESP32-H2; no other chip may be selected:

- [ESP32-H2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/get-started/index.html)


## Example folder contents

The project **hello_world** contains one source file in C language [hello_world_main.c](main/hello_world_main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.
