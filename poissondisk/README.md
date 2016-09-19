Poisson Disk Sampling
=====================

Simple Poisson disk sampling in a 3D grid based on Bridson's 2007 [paper](http://dx.doi.org/10.1145/1278780.1278807).


Usage
-----

Sample points in a unit box:

    ./poisson_disk 0.1 out.xyz

Open result with Meshlab:

    meshlab out.xyz