# Cleanup Summary

## Changes Made for Standalone Release

This document summarizes the cleanup performed to make the `sim/` directory standalone and ready for student use.

### 1. Platform Specification
- **Target Platform**: Ubuntu 24.04 LTS only
- Removed all references to macOS, Fedora, WSL, and PowerShell
- All installation instructions now use `apt-get` commands

### 2. Directory Structure
Created new directories:
- `sim/downloads/` - For Linux kernel source, busybox, and binaries
  - Includes `.gitignore` to prevent large files from being committed
  - Includes `README.md` with download and build instructions
- `sim/assets/` - Local copy of needed assets (e.g., waveform diagrams)

### 3. Path Standardization

#### Environment Variables (Added to main README)
```bash
# Cross-compilation
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARM_NONE_EABI=arm-none-eabi-

# Renode installation (for co-simulation)
export RENODE_PATH=/path/to/renode_portable
```

#### Fixed Paths
- **Kernel images**: Changed from absolute paths to `downloads/linux-6.6/vmlinux`
- **Renode**: Replaced hardcoded `/home/nitin/.../renode_portable` with `${RENODE_PATH}`
- **Assets**: Moved from `../../assets/` (outside sim) to `sim/assets/`
- **Binfiles**: Kept per-week structure, documented how to symlink from downloads

### 4. Documentation Updates

#### Main README (`sim/README.md`)
- Platform requirements (Ubuntu 24.04)
- Complete system package installation
- Python environment setup with uv
- Cross-compilation environment configuration
- Quick start guide (Week 0 demo)
- Improved troubleshooting section

#### Week-Specific READMEs
- **week04_renode**: Updated busybox and kernel build paths to use downloads/
- **week05_renode**: Normalized all paths to be relative
- **week07_renode_demos**: Replaced hardcoded Renode paths with ${RENODE_PATH}
- **week03_driver**: Ubuntu-specific kernel headers installation

### 5. External References Removed
- Removed references to `../../notes/` (course materials not included)
- Removed references to `../../assessments/`
- Changed "Week 12" references to generic "FPGA deployment"
- Removed external specification document references

### 6. Files Modified
- `sim/README.md` - Major rewrite for Ubuntu 24.04
- `sim/week00_basics/README.md` - Asset path fix
- `sim/week03_driver/README.md` - Platform-specific cleanup
- `sim/week04_renode/README.md` - Kernel build paths
- `sim/week05_renode/README.md` - Relative paths
- `sim/week07_renode_demos/*/README.md` - RENODE_PATH usage
- Various other documentation cleanups

### 7. Git Commits
All changes tracked in 7 commits:
1. Initial setup (README, PLAN.md, downloads structure)
2. Asset fixes
3. Week 4 updates
4. Week 5 updates
5. Week 7 updates
6. Platform reference cleanup
7. Final hardcoded path fixes

## What Students Need to Do

1. **Install system packages** (Ubuntu 24.04):
   ```bash
   sudo apt-get install verilator gtkwave device-tree-compiler \
     gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
     gcc-arm-none-eabi build-essential cmake git
   ```

2. **Set up Python environment**:
   ```bash
   cd sim
   uv venv .venv
   source .venv/bin/activate
   uv pip install -r requirements.txt
   ```

3. **Configure environment** (add to ~/.bashrc):
   ```bash
   export ARCH=arm
   export CROSS_COMPILE=arm-linux-gnueabihf-
   export RENODE_PATH=/path/to/renode_portable  # For week 7+
   ```

4. **Download and build kernel/busybox** (for Renode demos):
   - Follow instructions in `downloads/README.md`

5. **Run demos**:
   - Start with `week00_basics` for a simple test
   - Progress through weeks as needed

## Files Not Modified
- `.resc` files: Already use relative paths (e.g., `@binfiles/vmlinux`)
- RTL and source code: No changes needed
- Makefiles and CMake: Minimal changes (use environment variables)

## Verification
- No absolute paths remain (except in PLAN.md for reference)
- No references to notes/ or assessments/ directories
- All platform-specific instructions are Ubuntu 24.04 only
- All external links removed or made local
- RENODE_PATH consistently used for Renode installation

## Future Improvements
- Consider consolidating binfiles across weeks into downloads/binfiles
- Add pre-built kernel/busybox option for quick start
- Create automated setup script
- Add more troubleshooting scenarios
