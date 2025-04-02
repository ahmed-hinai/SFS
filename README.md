![sfs](https://github.com/user-attachments/assets/39acb1c7-459b-4476-b52f-1e52182f6dc8)

# SFS - Show Fan Speeds

SFS is a TUI for nbfc-linux I made while learning C. I personally wanted an alternative for Acer NitroSense working on linux as I just like to monitor my fan speeds :).  


## Features

- **Fan speed monitor**
- **A keybind to set fan speed target to max/auto**
- **Temperature and utilization monitor**

![sfs](https://github.com/user-attachments/assets/daeb74fa-92e6-483d-bcd9-65ded4a3c991)


## Installation

1. Install [nbfc-linux](https://github.com/nbfc-linux/nbfc-linux).
2. Install Figlet using your package manager.
3. Clone and install.

  ```bash
git clone https://github.com/ahmed-hinai/SFS.git
   ```

  ```bash
cd SFS
  ```

  ```bash
make install
  ```
## Uninstall

  ```bash
make uninstall clean
  ```

## Binding The NitroSense Key

The NitroSense key has a scancode "f5" and mapped to keycode 425, which is not recognized in KDE. You can use udev or setkeycodes (download from your pkg manager) to map it to another keycode. 
More info here: ![Map scancodes to keycodes](https://wiki.archlinux.org/title/Map_scancodes_to_keycodes)

I chose to map it to ScreenLock (keycode 70), like so:

  ```bash
setkeycodes f5 70
   ```

Then you can bind the NitroSense key to SFS using your desktop environment. 
