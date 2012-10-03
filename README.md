# raytracer

This is a raytracing library that is focused on speed and interactive rendering. It uses multiple threads to render parts of an image in parallel.

I started this project in my spare time in order to refresh my C++ skills and learn about the Google Test framework. Turns out you CAN write tests in C++ :-)

## prerequisites

To compile, you need ruby and rake. You also need a development version of Qt4 installed on your system. I recommend MacPorts for that.

## compile

To compile, simply type:

    rake

You might have to adjust some variables to make things work on your system.

A couple of examples will be compiled. The most important one is

    examples/SceneBrowser/SceneBrowser

## features

* Templatized library
* Pluggable cameras:
  * Pinhole camera
  * Orthographic camera
  * Spherical camera
  * Fish eye camera
* Pluggable materials
  * Matte
  * Phong
  * Reflective
  * Transparent
  * Portal (like in the game)
* Pluggable view planes for different interactive experiences (mostly for fun)
* Pluggable shapes
  * Sphere, Box, Plane, Disk, Rectangle, Triangle, Mesh
* Shape compositions
  * Composition, Union, Intersection, Difference, Instancing
* Support for the PLY mesh format
* SSE3 optimizations

## hack

* Fork
* Use the coding style
* Write code
* Write tests
* Send me a pull request
