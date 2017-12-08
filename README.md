Geogram Tools
=============

A collection of small utilities made using the [geogram](http://alice.loria.fr/software/geogram/doc/html/index.html) library.

Compilation
-----------

Download geogram in the `3rdparty` folder:

```
cd 3rdparty/
./download.sh
cd ..
```

Compile with CMake:

```
mkdir build
cd build
cmake ..
make -j4
```


[Normalize Mesh](normalize_mesh)
---------------------------

Rescale a mesh to fit in a unit box, and set its min corner to 0.

[Poisson Disk Sampling](poisson_disk)
------------------------------------

Simple Poisson disk sampling in a 3D grid based on Bridson's 2007 [paper](http://dx.doi.org/10.1145/1278780.1278807).

[VoxMesh](voxmesh)
------------------

A simple voxelization program, which takes a surface mesh as input.
