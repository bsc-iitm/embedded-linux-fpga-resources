# Cleanup Plan for Standalone sim/ Release

## Overview
Clean up the `sim/` folder for standalone release to students, removing course-specific content and making paths self-contained.

## Target Platform
- Ubuntu 24.04 LTS (other platforms at user's risk)
- All instructions will be Ubuntu-specific

## Major Changes Required

### 1. Directory Structure Changes

#### 1.1 Create standard download locations
- Create `sim/downloads/` directory for students to place:
  - Linux kernel source (e.g., `linux-6.6/`)
  - Busybox source
  - Pre-built kernel images (vmlinux, dtb files)
- Update all paths to reference `downloads/` relatively

#### 1.2 Clean up external references
- Remove or relocate content from:
  - `../../assets/` - copy needed assets into sim/assets/ or remove references
  - `../../notes/` - no references should exist
  - `../../assessments/` - no references should exist

### 2. Fix External Links and Paths

#### 2.1 Kernel image paths
Currently: `binfiles/vmlinux` is a symlink to `/home/nitin/scratch/linux/linux-6.6/vmlinux`

Change to:
- `sim/downloads/linux-6.6/vmlinux` (students build this)
- Update all .resc files to reference relative paths
- Files affected:
  - `week04_renode/binfiles/vmlinux` (symlink)
  - `week04_renode/week4_zynq.resc`
  - `week04_renode/zedboard.resc`
  - `week05_renode/week5_zynq.resc`
  - `week06_renode/week6_zynq.resc`
  - `week07_renode_demos/demo1_smarttimer_baremetal/renode_baremetal/renode/demo1_linux.resc`
  - `week07_renode_demos/demo2_fir_linux/renode/demo2.resc`
  - `week08_interrupts/demo_smarttimer_irq/renode/demo_irq.resc`

#### 2.2 Renode installation paths
Currently: Hardcoded paths like `/home/nitin/scratch/bses/renode_portable`

Change to:
- Document RENODE_PATH environment variable
- Update CMAKE commands to use `${RENODE_PATH}` or similar
- Files affected:
  - `week07_renode_demos/demo1_smarttimer_baremetal/README.md`
  - `week07_renode_demos/demo1_smarttimer_baremetal/COSIM_GUIDE.md`
  - `week07_renode_demos/demo2_fir_linux/README.md`

#### 2.3 Asset references
Currently: `../../assets/week00-counter-wave.svg`

Options:
- Copy needed assets into `sim/assets/`
- Remove the reference if not critical
- Files affected:
  - `week00_basics/README.md`

### 3. Update Main README.md

#### 3.1 Platform-specific content
Remove references to:
- macOS (Homebrew commands)
- Fedora (dnf commands)
- WSL (Windows instructions)
- PowerShell activation

Keep only Ubuntu 24.04 instructions

#### 3.2 Add environment setup
Add section for `.bashrc` snippet:
```bash
# Cross-compilation toolchain for ARM
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

# Renode installation (if using co-simulation)
export RENODE_PATH=/path/to/renode_portable
```

#### 3.3 Add downloads section
Document where to download and place:
- Linux kernel source (kernel.org)
- Busybox source
- ARM toolchain installation for Ubuntu

### 4. Update Week-Specific READMEs

#### 4.1 Week 4 Renode
- Update kernel compilation instructions to reference `sim/downloads/linux-6.6/`
- Update busybox paths to `sim/downloads/busybox/`
- Remove teaching notes if too detailed
- Keep practical how-to content

#### 4.2 Week 5 Renode
- Update DTB paths from `sim/week04_renode/binfiles/` to relative `../week04_renode/binfiles/`
- Or consolidate binfiles into `sim/downloads/binfiles/`

#### 4.3 Week 7 Renode Demos
- Update cmake commands: replace hardcoded `/home/nitin/...` with `${RENODE_PATH}`
- Update README to explain RENODE_PATH requirement
- Example: `cmake -DUSER_RENODE_DIR=${RENODE_PATH} ..`

#### 4.4 All weeks
- Review for external references (../ paths outside sim/)
- Review for teaching-specific content that should be removed
- Review for absolute paths that should be relative

### 5. Remove Course-Specific Content

#### 5.1 Files to remove
None identified yet in sim/ itself, but check for:
- Assessment files
- Detailed lecture notes (keep practical guides)
- Grading rubrics
- Course schedule references

#### 5.2 References to remove
- Week 12 references (FPGA deployment) if those materials aren't included
- Links to slides or external course materials
- References to course-specific timelines

### 6. Configuration Files

#### 6.1 .resc files
Update all Renode script files to use relative paths:
- `$elf=@../../downloads/linux-6.6/vmlinux` (or similar)
- `$dtb=@../../downloads/binfiles/zynq-zed.dtb`

#### 6.2 CMakeLists.txt files
- Replace hardcoded RENODE paths with variable references
- Add clear error messages if RENODE_PATH not set

### 7. Documentation Quality

#### 7.1 Prerequisites sections
Ensure each README clearly states:
- Required Ubuntu packages
- Required environment variables
- Required downloads and their locations

#### 7.2 Getting started
Main README should have clear:
1. System requirements (Ubuntu 24.04)
2. Package installation
3. Download instructions
4. Environment setup
5. Quick start (Week 0 or Week 1 demo)

## Implementation Order

1. **Create directory structure**
   - `sim/downloads/` (with .gitignore to exclude large files)
   - `sim/assets/` (if needed)

2. **Update main README.md**
   - Remove non-Ubuntu content
   - Add environment setup section
   - Add downloads section
   - Improve getting started

3. **Fix asset references**
   - Copy needed assets or remove references
   - Update paths in week00_basics/README.md

4. **Update kernel/busybox paths**
   - Update week04_renode/README.md compilation instructions
   - Create example directory structure documentation

5. **Fix .resc files**
   - Update all vmlinux/dtb paths to reference downloads/
   - Test that relative paths work from their locations

6. **Fix Renode references**
   - Update week07 READMEs
   - Replace hardcoded paths with ${RENODE_PATH}
   - Document RENODE_PATH requirement

7. **Clean up individual week READMEs**
   - Remove overly detailed teaching notes
   - Keep practical how-to content
   - Remove references to future weeks if content missing
   - Fix relative paths (../ outside sim/)

8. **Verify and test**
   - Check all relative paths resolve correctly
   - Verify no absolute paths remain
   - Check no references to notes/ or assessments/
   - Create test checklist

## Files Requiring Updates

### README files (priority order)
1. `sim/README.md` - main entry point
2. `week00_basics/README.md` - asset reference
3. `week04_renode/README.md` - kernel compilation paths
4. `week05_renode/README.md` - DTB paths
5. `week07_renode_demos/README.md` - overview
6. `week07_renode_demos/demo1_smarttimer_baremetal/README.md` - RENODE_PATH
7. `week07_renode_demos/demo1_smarttimer_baremetal/COSIM_GUIDE.md` - RENODE_PATH
8. `week07_renode_demos/demo2_fir_linux/README.md` - RENODE_PATH
9. All other week*/README.md files - review for issues

### Configuration files
1. All `*.resc` files with vmlinux references (8 files)
2. CMakeLists.txt files in week07_renode_demos subdirectories

### Other
1. Create `.gitignore` for downloads/
2. Create `downloads/README.md` with instructions

## Post-Cleanup Verification

- [ ] No absolute paths remain (except in examples showing what to replace)
- [ ] No references to parent directories outside sim/
- [ ] No references to notes/ or assessments/
- [ ] No platform-specific instructions except Ubuntu 24.04
- [ ] All .resc files use relative paths
- [ ] Environment variables documented
- [ ] Downloads directory structure documented
- [ ] Main README has complete getting started guide
- [ ] At least one demo can run following only sim/ documentation
