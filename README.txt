                          NUL: NOVA Userland
                          ==================

Author: Julian Stecklina <jsteckli@tudos.org>
Date: 2011-02-16 21:38:54 CET


Table of Contents
=================
1 Introduction 
2 Prerequisites 
3 Development 
    3.1 Directory Layout 
    3.2 Building 
4 Booting a Vancouver System 
5 Feedback 


1 Introduction 
~~~~~~~~~~~~~~~

  This is the development version of NUL, the NOVA UserLand.  It
  complements the NOVA microhypervisor with applications, mainly a
  virtual machine monitor and a hardware resource multiplexer.

  The code is still experimental and far from feature complete.  Use
  it on your own risk.  If it breaks, you get to keep both pieces.

2 Prerequisites 
~~~~~~~~~~~~~~~~

  - *Binutils* 2.20 or later
  - *GCC* 4.4.4, 4.5.2 or later
  - *SCons* 1.2.0 or later
  - *Python* 2.6 or later

3 Development 
~~~~~~~~~~~~~~

  This section is aimed at the prospective NUL hacker. Its purpose is
  to boot your knowledge about nul to a point where you can start
  hacking on it.

3.1 Directory Layout 
=====================

   The NUL tree consists of several applications and libraries, which
   are loosely grouped by topic into repositories. Each subdirectory
   of the top-level directory is a repository.

3.2 Building 
=============

  To build NUL you need a build directory. Luckily, there is already a
  preconfigured one in /build/. We use a minimal SCons-based build
  system that mostly does what we want. So to build the default
  configuration just cd into build and type `scons'. Binaries will be
  installed in build/bin and build/boot by default.

  To use a customized build configuration, copy the default build
  repository and adapt its SConstruct file.

  There are some variables that can be passed to SCons in order to
  control the build process.

    *Argument*         *Description*                
   ------------------+-----------------------------
    target_cc=mycc     Compile C code with mycc.    
   ------------------+-----------------------------
    target_cxx=myc++   Compile C++ code with myc++  

  To compile nul with an old version of GCC, type:
  `scons target_cc=gcc-4.4.4 target_cxx=g++-4.4.4'

4 Booting a Vancouver System 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  An example GRUB configuration can be found in
  demo/example.conf. Another example is the Demo CD. For explanation
  of the individual parameters refer to README.sigma0 and README.vmm.

5 Feedback 
~~~~~~~~~~~

  NUL is the work of Bernhard Kauer <bk@vmmon.org>, Alexander
  Böttcher <boettcher@tudos.org>, and Julian Stecklina
  <jsteckli@tudos.org>. The author of the NOVA hypervisor is Udo
  Steinberg <udo@hypervisor.org>.
