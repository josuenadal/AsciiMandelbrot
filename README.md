# AsciiMandelbrot
A Multithreaded Mandelbrot Explorer for your terminal. Written in C++ for Ncurses. The application makes use of GMP and MPFR libraries.

| Key       | Control          |
|-----------|------------------|
| Enter     | Zoom in.         |
| Backspace | Zoom out.        |
| x         | Set Coordinates. |
| i         | Set iterations.  |
|Arrow Keys | Move camera.     |
| q or ESC  | Quit application.|

To compile use:
g++ asciimandelbrot.cpp -o ./AsciiMandelbrot -lgmp -lgmpxx -lmpfr -lncurses --fast-math

![image](https://github.com/josuenadal/AsciiMandelbrot/assets/13826541/a2e96b89-4832-4526-a1b3-2260af58e1ba)
Application at max resolution on my system.

Started this in college and just decided to upload it after I fixed some things. Was kind of inspired by a1k0n's donut.c.
Still a work in progress. 

To do: 
- Thinking of doing some sort of makefile/compilation process.
- Parametrise many of the variables so user can configure performance behaviors.
- Want to add horizon mirroring/reflection so only the top half needs to be calculated and the bottom half is mirrored when screen crosses the horizon.
- Fix seemingly random overflow in calculate_buffer_area_threaded. (Might be related to next point)
- Function get_screen_chunks needs more proofing, sort out edge cases. 
