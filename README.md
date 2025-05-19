# AsciiMandelbrot

A Multithreaded Mandelbrot Explorer for your terminal. Written in C++ for Ncurses. 

The application makes use of GMP and MPFR libraries.

![image](https://github.com/josuenadal/AsciiMandelbrot/assets/13826541/a2e96b89-4832-4526-a1b3-2260af58e1ba)
Application at max resolution on my system.

## Controls

| Key       | Control          |
|-----------|------------------|
| Enter     | Zoom in.         |
| Backspace | Zoom out.        |
| x         | Set Coordinates. |
| i         | Set iterations.  |
|Arrow Keys | Move camera.     |
| q or ESC  | Quit application.|
| c         | Shade cycle.     |

## Compile

To compile use:
```bash
g++ asciimandelbrot.cpp -o ./AsciiMandelbrot -lgmp -lgmpxx -lmpfr -lncurses --fast-math
```

### Notes

Started this in college and just decided to upload it after I fixed some things. Was kind of inspired by a1k0n's donut.c. It kinda lost it's shape after the optimizations, lol.

Still a work in progress. 

To do: 
- Need to do some sort of makefile/compilation process.
- Want to add horizon mirroring/reflection so only the top half needs to be calculated and the bottom half is mirrored when screen crosses the horizon.
- Further harden thread functionality. 