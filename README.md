## Description
A Monte Carlo ray tracer implementation.

## Sample images
This is the second image that was rendered by this ray tracer (using only Lambertian reflections) after about 4 days of coding:

<img src="https://github.com/Friduric/raytracer/blob/master/second_lambertian_test.png" width="400" height="400">

This is the first image that was rendered by this ray tracer (using only Lambertian reflections) after 2 days of coding: 

<img src="https://github.com/Friduric/raytracer/blob/master/first_lambertian_test.png" width="400" height="400">

## A few troubleshooting tips
- IMPORTANT: Use the 32-bit binaries (build using x86!). Otherwise GLM might bug out.

- OPTIONAL: Linker -> Command Line -> Add "/NODEFAULTLIB:MSVCRT /NODEFAULTLIB:LIBCMT"

- OPTIONAL: Remove warning LNK4099:

  - Change C/C++ -> Output Files -> Program Database File to: "$(TargetDir)$(TargetName).pdb"

  - Change Linker -> Debugging -> Generate Program Database File to: "$(TargetDir)$(TargetName).pdb"

- OPTIONAL: Adding /FORCE:MULTIPLE to the Linker command line helps remove errors LNK1169 and LNK2005.
