# Vulkan

Vulkan restores PE images by dumping them from memory. It was specifically designed for processes protected by dynamic code encryption, implemented by the [hyperion](https://roblox.fandom.com/wiki/Hyperion) and [theia](https://reversingthread.info/index.php/2024/01/10/the-finals-defeating-theia-packer/) anti-tamper solutions. Vulkan can also dump regular images from memory as well as modules loaded by a process.

Vulkan has been tested on [Roblox](https://roblox.com) and [The Finals](https://www.reachthefinals.com/).

## How to use

After downloading the latest version from the [releases](https://github.com/atrexus/vulkan/releases) tab, you can run it from the command line like so:

```
vulkan.exe -p <TARGET_PROCESS> -o <OUTPUT_FILE> --resolve-imports
```

If no output file is specified, the file will be saved to the current working directory.

To view the help message use the `-h` or `--help` option.

### Decryption

As mentioned, Vulkan will continue to query pages of code in the target module untill all `NOACCESS` pages are resolved. Depending on the application, there might always be `NOACCESS` pages so decryption could end in an infinate loop. To terminate the decryption task, you can use the `Ctrl+C` keyboard shortcut (for best results wait until 50% of the module is decrypted).

If you would like to automate termination, you can use the `-d` or `--decryption-factor` option, and provide it with a threshold ranging from `0.0` to `0.1` (again, for best results provide a value of `0.5` or above):
```
vulkan.exe -p <TARGET_PROCESS> --decryption-factor 0.5
```

### Imports

To resolve imports for the main module, you can use the `i` or `--resolve-imports` flag. This will locate the custom IAT and restore the import directory in a new section. This may take a while, depending on how many pages were decrypted. This will have no effect on any modules other than the main one:
```
vulkan.exe -p <TARGET_PROCESS> --resolve-imports
```

## Contributing

If you have anything to contribute to this project, please send a pull request, and I will review it. If you want to contribute but are unsure what to do, check out the [issues](https://github.com/atrexus/vulkan/issues) tab for the latest stuff I need help with.
