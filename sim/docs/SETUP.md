# Simulation Environment Setup

## Platform Requirements

**Supported Platform:** Ubuntu 24.04

These instructions are tested on Linux Ubuntu 24.04 and Debian 13.1. Other platforms may work but are not officially supported.  

The problem here is that `verilator` and `cocotb` have been undergoing extensive changes recently, and the version of `verilator` packaged in most distros does not match the minimum requirement of `cocotb` - which is installed directly from python.

Your best bet is to boot into a virtual machine with the Xubuntu 24.04 installation CD - install the XFCE environment as it is lightweight and fast.  If you are using Windows Subsystem for Linux (WSL) then a Ubuntu 24.04 or Debian 13 environment should also work well.  

MacOS has not been tested: it is quite possible most of these packages work as expected, but we cannot guarantee this as there are many version changes.

### Note on paths

The `sim` path that we refer to throughout here is the folder that you have cloned itself.  If this was cloned from github for example there will not be a separate `sim` folder to change into.

There are some environment variables that specify different path entries such as the location of the Linux kernel source code, renode installation etc.  Some of these need their paths to be specified.  To keep things simple, we are going to assume you can unpack the folder wherever you like, but we will use a symbolic link in Linux to point to the actual downloads folder so you can use an absolute path.

Therefore, 

1. Clone this folder into an appropriate place
2. From the command line change directory to the folder containing this README file
3. Ensure there is no file or folder named `downloads` in `~/Desktop`
4. Enter the command:

```bash
ln -s $PWD/downloads ~/Desktop/
```

## Prerequisites

### Required System Packages

Install the following packages on Ubuntu 24.04:

```bash
# Install various packages
sudo apt-get update
sudo apt-get install curl

# Verilog simulator (Verilator recommended)
sudo apt-get install verilator

# Alternative: Icarus Verilog (if you prefer)
# sudo apt-get install iverilog

# Waveform viewer (optional but useful)
sudo apt-get install gtkwave

# Device tree compiler (required for Renode demos)
sudo apt-get install device-tree-compiler

# ARM cross-compilation toolchain
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt-get install gcc-arm-none-eabi  # For bare-metal demos

# Build essentials
sudo apt-get install build-essential flex bison libssl-dev libelf-dev cmake git
```

**Notes:**
- Verilator is faster and integrates well with cocotb
- Icarus Verilog is simpler and sufficient for basic verification
- GTKWave is essential for viewing waveforms

### Install uv (Python Environment Manager)

Install `uv` for Python dependency management:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Restart your shell or source your profile, then verify:
```bash
uv --version
```

## Python Environment Setup

### Create the Shared Virtual Environment

*Note*: We use python version 3.12 here since that has been tested with the examples.  Since then, cocotb has undergone a major version change, and python itself has gone through multiple updates.  These changes will be brought in later since they also break the dependency on verilator and other tools.  For now use the versions here.

From the `sim/` directory:

```bash
cd sim
uv venv --python 3.12 .venv
```

### Activate the Environment

```bash
source .venv/bin/activate
```

You'll need to activate this environment every time you start a new shell session before running simulations.

### Install Python Dependencies

With the environment activated:

```bash
uv pip install -r requirements.txt
```

This installs cocotb and other simulation dependencies listed in `requirements.txt`.

## Environment Configuration

### Cross-Compilation Setup

For kernel and bare-metal development, set up cross-compilation environment variables. Add this to your `~/.bashrc` or `~/.profile`:

```bash
# ARM cross-compilation environment
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

# For bare-metal ARM examples
export ARM_NONE_EABI=arm-none-eabi-
```

We assume that you are going to download Linux and busybox into `~/Desktop/downloads`

```bash
export KDIR=~/Desktop/downloads/linux-6.6
```

For Renode co-simulation demos (Week 7 and later), also add:

```bash
# Renode installation path (adjust to your installation)
export RENODE_PATH=~/Desktop/downloads/renode_1.16.0_portable
# Add to path so you can just run the renode command directly
export PATH=$RENODE_PATH:$PATH
```

After editing `.bashrc`, reload it:
```bash
source ~/.bashrc
```

### Download Required Sources

Before starting the Renode-based demos (Week 4+), download and build the required components:

1. Follow instructions in `downloads/README.md` to:
   - Download Linux kernel source
   - Download and build Busybox
   - Build the kernel with appropriate configuration

## Verify Setup

Check that everything is installed correctly:

```bash
# Python environment
python -c "import cocotb; print('cocotb', cocotb.__version__)"

# Simulators
verilator --version

# Cross-compilation toolchain
arm-linux-gnueabihf-gcc --version
arm-none-eabi-gcc --version

# Device tree compiler
dtc --version
```

## Quick Start

### Run Your First Demo (Week 0)

1. Activate the Python environment:
```bash
cd sim
source .venv/bin/activate
```

2. Navigate to the Week 0 basics demo:
```bash
cd week00_basics
```

3. Run the simulation:
```bash
make  # Uses Verilator by default
# Or: make SIM=icarus  # Use Icarus Verilog instead
```

4. View the waveforms (if generated):
```bash
gtkwave sim_build/*.vcd  # Or *.fst depending on configuration
```

### Next Steps

- **Week 1-3**: Basic Verilog testbenches with cocotb and AXI-Lite interfaces
- **Week 4-6**: Renode system emulation, Linux kernel, device drivers, device tree
- **Week 7+**: Advanced topics with co-simulation (Verilator + Renode)

See individual week directories for specific exercises and READMEs.

## Troubleshooting

### Common Issues

**`cocotb-config` not found or `/Makefile.sim` error**
- Make sure the Python environment is activated: `source sim/.venv/bin/activate`
- Verify cocotb is installed: `python -c "import cocotb"`

**Permission errors on package installation**
- Use `sudo apt-get install` for system packages, not pip
- Never install Verilator or other simulators via pip

**No waveforms generated**
- For Verilator: Ensure `VERILATOR_TRACE=1` is set (check Makefile)
- For Icarus: VCD generation should be automatic
- Check `sim_build/` directory for generated files

**Cross-compilation fails**
- Verify toolchain installation: `arm-linux-gnueabihf-gcc --version`
- Check environment variables: `echo $ARCH $CROSS_COMPILE`
- Source your `.bashrc` if you just added the variables

**Renode demos fail**
- Ensure `RENODE_PATH` is set and points to valid installation
- Check that kernel/busybox are built in `downloads/` directory
- Verify relative paths in `.resc` files are correct

## What's Included

### Python Packages (see `requirements.txt`)
- `cocotb`: Coroutine-based testbench framework
- `cocotbext-axi`: AXI/AXI-Lite bus functional models

### Directory Structure
- `week00_basics/`: Introduction to cocotb with simple counter
- `week01_axil/`: AXI-Lite interface examples
- `week02_timer/`: Smart timer peripheral verification
- `week03_driver/`: Basic driver concepts
- `week04_renode/`: Renode system emulation introduction
- `week04_mmio_demo/`: MMIO driver examples
- `week05_renode/`: Device tree workflow
- `week05_mmio_demo/`: Advanced MMIO examples
- `week06_*/`: Platform drivers and FFT examples
- `week07_*/`: Verilator/Renode co-simulation
- `week08_interrupts/`: Interrupt handling
- `week09_interrupts_advanced/`: Advanced interrupt patterns
- `downloads/`: Place for kernel source, busybox, and binaries (see `downloads/README.md`)
- `assets/`: Supporting assets and diagrams

## Contributing

This is a standalone practice environment. If you extend it with new examples or find issues, feel free to share improvements.
