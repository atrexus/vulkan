# Vulkan
Vulkan dumps the main PE image of a process from memory to your disk. It targets processes protected by dynamic code encryption, implemented by the [hyperion](https://roblox.fandom.com/wiki/Hyperion) and [theia](https://reversingthread.info/index.php/2024/01/10/the-finals-defeating-theia-packer/) anti-tamper solutions. Vulkan constantly queries pages in the `.text` section of the target process and caches pages of code as they decrypt themselves. After a desired amount of the application has been decrypted (configurable via `--decrypt`), the restored PE image is saved to the disk and ready for analysis.

Vulkan has been tested on [Roblox](https://roblox.com) and [The Finals](https://www.reachthefinals.com/).

## How to use
After downloading the latest version from the [releases](https://github.com/atrexus/vulkan/releases) tab, you can run it from the command line like so:
```
vulkan.exe -p <TARGET_PROCESS> -o <OUTPUT_DIRECTORY>
```
If no output directory is specified, the file will be saved to the current working directory (`.`). An optional `-D` parameter can also be specified if you want to run Vulkan in debug mode. This will prevent any regular user-mode application from querying information about Vulkan. 

### Decryption
By default, the decryption factor is set to `1.0`, meaning the dumping process will only conclude once the application's code has been decrypted. This can take an incredibly long time, so if at any moment you want to terminate the current decryption task, type `Ctrl+C` in the command line, and the dump will be saved as is.

If you want to set the target decryption factor manually, you can do it from the command line like so:
```
vulkan.exe -p <TARGET_PROCESS> -o <OUTPUT_DIRECTORY> --decrypt <TARGET_FACTOR>
```
> **Note**: Increasing the decryption factor will significantly increase the time it takes to produce the dump. It is recommended that the factor be between `0.6` and `0.7`.

## Contributing
If you have anything to contribute to this project, please send a pull request, and I will review it. If you want to contribute but are unsure what to do, check out the [issues](https://github.com/atrexus/vulkan/issues) tab for the latest stuff I need help with.
