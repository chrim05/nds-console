# why
* for fun
* having a terminal application on your ds which gets more control over it

# requirements
* devkitpro
* make
* desmume (optional)

# how to compile it
```
clone https://github.com/christallo/nds-console
cd nds-console
make
```

# how to run it
* on desmume (for debugging)
```
make && desmume nds-console.nds
```
* on physical nintendo ds console (for distrubution)
* * `make`
* * move `nds-console.nds` to your R4 or your modded sd

# how to edit it in vscode
* create a configuration json file for c/cpp
* open `.vscode\c_cpp_properties.json`
* write
```json
{
    "configurations": [
        {
            "name": "Win32", // for windows
            "name": "Linux", // for linux

            "includePath": [
                "C:/devkitpro/devkitARM/arm-none-eabi/include",   // for windows
                "/opt/devkitpro/devkitARM/arm-none-eabi/include", // for linux

                "C:/devkitpro/libnds/include",   // for windows
                "/opt/devkitpro/libnds/include", // for linux

                "${workspaceFolder}/**"
            ],
        
            "defines": ["ARM9"],

            "compilerPath": "C:/devkitPro/devkitARM/bin/arm-none-eabi-g++.exe",   // for windows
            "compilerPath": "/opt/devkitPro/devkitARM/bin/arm-none-eabi-g++.exe", // for linux

            "cStandard": "gnu17",
            "cppStandard": "gnu++14",
            "intelliSenseMode": "windows-gcc-x64", // for windows
            "intelliSenseMode": "linux-gcc-x64",   // for linux
        }
    ],
    "version": 4
}
```
* delete the line you don't care with comments about your os