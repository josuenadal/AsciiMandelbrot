#include <iostream>
#include <math.h>
#include <unistd.h>
#include <functional>
#include <iomanip>
#include <fstream>
#include <format>
#include <string>
#include <vector>
#include <gmp.h>
#include <gmpxx.h>
#include <ncurses.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include "./mpreal.h"
#include <queue>
#include <condition_variable>
#include "thread_pool.hpp"

using mpfr::mpreal;

bool DEBUG = false;

// Will use all available threads, reserving 2, one for drawing and one for input.
const uint32_t num_threads = std::thread::hardware_concurrency() - 2;

//
// Mandelbrot object containing the function that calculates each point.
//
class Mandelbrot
{
    public:
    // Max Iterations to calculate orbit for. Related to visible depth.
    long int maxIterations = 50;

    std::mutex mutex;

    // The limits of the calculated plane
    mpreal real_min;
    mpreal real_max;

    mpreal imag_min;
    mpreal imag_max;

    // Dimensions of plane to be calculated
    mpreal width;
    mpreal height;

    // Desired Focus point
    mpreal real_coordinate;
    mpreal imag_coordinate;

    // Zoom Factor - The area gets multiplied by this in order to shrink
    mpreal zoom_factor;
    mpreal half_zoom;
    mpreal zoom_out_factor;

    // The factor which when multiplied by each dimension gives 
    // the distance to move the plane in x or y. 
    mpreal transl_factor;

    // The length of traversal in X or Y - controlled by transl_factor
    mpreal transl_x;
    mpreal transl_y;

    std::atomic<bool> changed = true;

    // Mandelbrot orbit calculator. Checks when it converges or shoots off, max is maxIterations. 
    int calculate_point(mpreal realc, mpreal imaginaryc)
    {
        int iter_count = 0;

        mpreal zx, zy;
        zx = "0";
        zy = "0";

        mpreal xsqr, ysqr;
        xsqr = "0";
        ysqr = "0";

        while(iter_count < maxIterations && xsqr + ysqr < 4.0)
        {
            zy *= zx;
            zy += zy + imaginaryc;
            zx = xsqr - ysqr + realc;
            xsqr = zx * zx;
            ysqr = zy * zy;
            iter_count++;
        }
        return iter_count;
    }

    // Calculate translation distance at each level.
    void set_translation_distance()
    {
        transl_x = width * transl_factor;
        transl_y = height * transl_factor;
    }

    // Move viewport up around the point. 
    void move_up()
    {
        std::unique_lock lock(mutex);
        imag_min -= transl_y;
        imag_max += transl_y;
        imag_coordinate -= transl_y;
        update();
    }
    
    // Move viewport down around the point. 
    void move_down()
    {
        std::unique_lock lock(mutex);
        imag_min += transl_y;
        imag_max -= transl_y;
        imag_coordinate += transl_y;
        update();
    }

    // Move left up around the point. 
    void move_left()
    {
        std::unique_lock lock(mutex);
        real_min -= transl_x;
        real_max += transl_x;
        real_coordinate -= transl_x;
        update();
    }

    // Move viewport right around the point. 
    void move_right()
    {
        std::unique_lock lock(mutex);
        real_min += transl_x;
        real_max -= transl_x;
        real_coordinate += transl_x;
        update();
    }

    // Zoom into the mandelbrot by reducing the height of the viewport around the center point.
    void zoom()
    {
        std::unique_lock lock(mutex);
        // Zooming is done by reducing the area calculated by zoom_factor, then adding each 
        // half of the new area to each side of the coordenate that is centered. Thus always zooming around the chosen coordinate.
        mpreal half_width, half_height;

        /*  
            I use half of the factor to immediately get the half width with only one multiplication.
            That way i do not have to multiply by the zoom factor then divide by 2. In order to-
            add each half to each side of the coordinate, thereby ensuring the movements are always centered.
        */
        half_width = width * half_zoom;
        real_min =  real_coordinate - half_width;
        real_max = real_coordinate + half_width;

        half_height = height * half_zoom;
        imag_min = imag_coordinate - half_height;
        imag_max = imag_coordinate + half_height;

        width = half_width * 2;
        height = half_height * 2;
        
        set_translation_distance();
        update();
    }

    // Zoom out of the mandelbrot by reducing the hight of the viewport around the center point.
    void zoom_out()
    {
        std::unique_lock lock(mutex);
        mpreal half_width, half_height;

        half_width = width * zoom_out_factor;
        real_min =  real_coordinate - half_width;
        real_max = real_coordinate + half_width;

        half_height = height * zoom_out_factor;
        imag_min = imag_coordinate - half_height;
        imag_max = imag_coordinate + half_height;

        width = half_width * 2;
        height = half_height * 2;
        
        set_translation_distance();
        update();
    }

    // Center viewport around specified coords.
    void set_coords(mpreal real, mpreal imag)
    {
        std::unique_lock lock(mutex);
        real_coordinate = real;	
        imag_coordinate = imag;
        
        mpreal half_width, half_height;

        half_width = width * 0.5;
        real_min =  real_coordinate - half_width;
        real_max = real_coordinate + half_width;

        half_height = height * 0.5;
        imag_min = imag_coordinate - half_height;
        imag_max = imag_coordinate + half_height;

        set_translation_distance();
        update();
    }
    
    void set_max_iterations(int i)
    {
        maxIterations = i;
        update();
    }

    bool updated()
    {
        if (changed)
        {
            changed = false;
            return true;
        }
        return false;
    }

    void update()
    {
        changed = true;
    }

    Mandelbrot()
    {
        // Set center point.
        real_coordinate = "0";
        imag_coordinate = "0";
        
        // Set factor of the screen area to zoom by.
        // Screen will be zoomed out by the inverse of this factor (1 - zoom_factor).
        zoom_factor = "0.9";
        
        // Set translation factor, will move screen horz and vert by this factor.
        transl_factor = 0.06;

        // Set visible area.
        real_min = "-3";
        real_max = "3";
        imag_min = "-2";
        imag_max = "2";
        
        // Calculate the dimensions of the area.
        height = imag_max - imag_min;
	    width = real_max - real_min;
        
        // Calculate plane movement distances.
        set_translation_distance();
        
        // Calculate zoom factors.
        half_zoom = zoom_factor * 0.5;
        zoom_out_factor = (1 + (1 - zoom_factor)) * 0.5;
    }
};

//
// Display class that handles all drawing to the screen.
//
class Display
{
    public:
    std::thread thread;
    bool terminate = false;

    // Mutex for utilizing screen sizes. Prevents overflow when resizing.
    std::mutex draw_mutex;

    // Editable buffer.
    std::vector<char> buffer;

    // Display buffer.
    std::vector<char> display_buffer;

    // Buffer dimmensions.    
    long int buffer_width;
    long int buffer_height;
    long int buffer_length;

    // 
    std::chrono::time_point<std::chrono::system_clock> shade_start_time = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> frame_start_time = std::chrono::system_clock::now();

    // Variable for screen info details.
    std::string stats = "";
    int status_lines = 1;

    // Draw whole display buffer at position 1, 1 of terminal.
    void draw_display_buffer()
    {
        // This program uses ncurses and multithreading. 
        // Those 2 don't mix. That's why i use printf to print here.
        // It is imperative to only use printf because ncurses functions
        // are being reserved for the user interace.
        std::unique_lock lock(draw_mutex);
        printf("\033[%d;%dH", 1, 1);
        for (int h = 0; h < buffer_height; h++)
        {
            for(int w = 0; w < buffer_width; w++)
            {
                printf("%c",display_buffer[(h*buffer_width)+w]);
            }
            printf("%c",'\r');
            printf("%c",'\n');
        }
        refresh();
    }

    // Print stats to bottom of mandelbrot display area.
    void print_stats(std::string s)
    {
        // This program uses ncurses and multithreading. 
        // Those 2 don't mix. That's why i use printf to print here.
        // It is imperative to only use printf because ncurses functions
        // are being reserved for the user interace.
        stats = s;

        // Erase stats
        printf("\033[%ld;%dH", buffer_height+1, 1);
        printf("\x1B[0J"); // Erase from cursor to end of screen.

        // Print stats
        printf("\033[%ld;%dH", buffer_height+1, 1);
        printf("%s",s.c_str());
        printf("\033[%d;%dH", 1, 1);
    }

    // Set how many lines of the display will be dedicated to status info.
    void set_print_status_line_length(int l)
    {
        status_lines = l;
        set_screen_size();
    }

    // Readjust screensize if necessary.
    void set_screen_size()
    {
        // Lock sizes_mutex so it doesnt interfere with draw.
        std::unique_lock lock(draw_mutex);
        long int window_width = 0;
        long int window_height = 0;
        getmaxyx(stdscr, window_height, window_width);

        if((buffer_width != window_width) | (buffer_height != window_height))
        {
            // Set buffer dimensions.
            buffer_height = window_height - status_lines;
            buffer_width = window_width - 1;
            buffer_length = buffer_height * buffer_width;

            buffer.resize(buffer_length);
            std::fill(buffer.begin(), buffer.end(), ' ');

            display_buffer.resize(buffer_length);
            std::fill(display_buffer.begin(), display_buffer.end(), ' ');
        }
        lock.unlock();
    }

    // Put the finished rendered buffer into the display buffer for presentation.
    void draw()
    {
        // display_buffer.swap(buffer);
        draw_display_buffer();
    }
    
    Display()
    {
        // Ncurses init.
        initscr();              // Start curses mode.
        raw();                  // Line buffering disabled
        keypad(stdscr, TRUE);   // We get F1, F2 etc..
        noecho();               // Don't echo() while we do getch.

        // Here to init screen dimensions and set buffer dimmensions.
        set_screen_size();

        // Fill buffers with whatever.
        std::fill(buffer.begin(), buffer.end(), '|');
        std::fill(display_buffer.begin(), display_buffer.end(), '|');
    }
};

//
// Handles all rendering.
//
class Renderer
{
    private:
    tp::ThreadPool threadPool{num_threads};

    public:
    Mandelbrot& mandelbrot;
    Display& display;

    std::thread thread;
    bool running = true;

    // Shading array, this is what the mandelbrot looks like.
    const char* shade_chars = " .,-~o:;*=><!?HX#$@";
    unsigned long int shade_char_size = 0;
    
    // Cycles shades for pulsating appearance.
    bool shade_cycle_toggle = false;
    
    // Scales for conversions between a continous point and a discrete buffer index.
    mpreal width_scale;
    mpreal height_scale;

    // Screen buffer sizes.
    static long int buffer_width;
    static long int buffer_height;
    long int buffer_length = 0;

    std::chrono::time_point<std::chrono::system_clock> render_start_time = std::chrono::system_clock::now();

    Renderer(Mandelbrot& mandel_ptr, Display& display_ptr) : mandelbrot(mandel_ptr), display(display_ptr)
    {
        thread = std::thread(&Renderer::render_loop, this);
        if (DEBUG) { display.set_print_status_line_length(9); }
        else { display.set_print_status_line_length(5); }
    }

    ~Renderer()
    {
        running = false;
    }

    // When to render.
    bool render_clock()
    {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::chrono::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - render_start_time);
        // Render time, set for around 60 fps.
        if(duration.count() > 15)
        {
            render_start_time = now;
            return true;
        }
        return false;
    }

    // Get character from shader array.
    char get_shade(int iter)
    {
        if(iter == mandelbrot.maxIterations)
        {
            return ' ';
        }
        return shade_chars[(iter % sizeof(shade_chars))+shade_char_size];
    }

    // From buffer index calculate the corresponding point on the mandelbrot.
    void raster_range(int start, int end)
    {
        for(int buff_pos = start; buff_pos <= end; buff_pos++)
        {
            // Convert linear buffer position to plane coords.
            int buff_x = buff_pos % display.buffer_width;
            int buff_y = buff_pos / display.buffer_width;
            
            // Project buffer position onto mandelbrot.
            mpreal x = mandelbrot.real_min + buff_x * width_scale;
            mpreal y = mandelbrot.imag_min + buff_y * height_scale;
            
            // Get iteration, assign a shade and place into buffer for display.
            int iter = mandelbrot.calculate_point( x, y );
            display.display_buffer[buff_pos] = get_shade( iter );
        }
    }

    // Thread that checks if mandelbrot needs rendering.
    void render_loop()
    {
        while(running)
        {
            // If the frametime is right and the mandelbrot has been updated then render it.
            if(render_clock() && mandelbrot.updated())
            {
                std::unique_lock lock(mandelbrot.mutex);
                
                // Calculate scales for projection.
                width_scale = mandelbrot.width / display.buffer_width;
                height_scale = mandelbrot.height / display.buffer_height;

                using namespace std::placeholders; 
                threadPool.create_work_queue(display.buffer_length, std::bind(&Renderer::raster_range, this, _1, _2));
                display.draw();
                print_stats("");
            }
        }
    }

    // Kill render loop thread.
    void stop()
    {
        running = false;
        if(thread.joinable())
        {
            thread.join();
        }
    }

    // Print statistics.
    void print_stats(std::string stat = "")
    {
        std::string s;
        s += std::format("{}\n\r", stat);
        s += std::format("depth = {}\n\r", mandelbrot.width.toString());
        if(DEBUG)
        {
            s += std::format("real coord = {}\n\r", mandelbrot.real_coordinate.toString());
            s += std::format("imag coord = {}\n\r", mandelbrot.imag_coordinate.toString());
            s += std::format("real_min = {}\n\r",  mandelbrot.real_min.toString());
            s += std::format("real_max = {}\n\r", mandelbrot.real_max.toString());
            s += std::format("imag_min = {}\n\r", mandelbrot.imag_min.toString());
            s += std::format("imag_max = {}\n\r", mandelbrot.imag_max.toString());
        }
        else
        {
            s += std::format("coords = ({}, {}i)\n\r", mandelbrot.real_coordinate.toString(), mandelbrot.imag_coordinate.toString());
        }
        s += std::format("Iterations = {}\n\r", std::to_string(mandelbrot.maxIterations));

        display.print_stats(s);
    }
};

//
// Handles user input through ncurses, also resizes screen through ncurses.
//
class UserInterface
{
    public:
    Mandelbrot& mandelbrot;
    Display& display;
    Renderer& renderer;


    void Navigate()
    {
        keypad(stdscr, TRUE);
        clear();
        noecho();
        // Hold the keyboards input value.
        int c;
        while(true)
        {
            move(display.buffer_height, 0);
            c = getch();
            switch(c)
            {			
                case 10:	//10 is enter on normal keyboard
                    print_status("Zooming in...");
                    mandelbrot.zoom();
                    break;
                case KEY_BACKSPACE:
                    print_status("Zooming out...");
                    mandelbrot.zoom_out();
                    break;
                case KEY_UP:
                    print_status("Moving up...");
                    mandelbrot.move_up();
                    break;
                case KEY_DOWN:
                    print_status("Moving down...");
                    mandelbrot.move_down();
                    break;
                case KEY_LEFT:
                    print_status("Moving left...");
                    mandelbrot.move_left();
                    break;
                case KEY_RIGHT:
                    print_status("Moving right...");
                    mandelbrot.move_right();
                    break;
                case KEY_RESIZE:
                    display.set_screen_size();
                    mandelbrot.update();
                    break;
                case 67: // uppercase C
                case 99: // lowercase c
                    // Broke with the refactoring. Not yet implemented.
                    // print_status("Toggle Shade Cycling");
                    // toggle_shade_cycle();
                    break;
                case 88: 	// uppercase X
                case 120: 	// lowercase X
                    set_coords();
                    break;
                case 73:	//uppercase I
                case 105:	//lowercase i
                    set_iterations();
                    break;
                case 113: //letter q, for quit
                case 27:  //escape key
                    endwin();
                    return;
                default:
                    print_status("");
                    continue;
            }
        }
    }

    void set_coords()
    {
        timeout(-1);
        echo();

        print_status("Set real coordinate: ");
        move(display.buffer_height, 21);

        char* c_real = new char[160];
        mpreal real;
        while(true)
        {
            wclrtoeol(stdscr);
            getstr(c_real);

            if(c_real[0] == '\0')
            {
                real = mandelbrot.real_coordinate;
                break;
            }

            try
            {
                real = c_real;
                break;
            }
            catch(...)
            {
                print_status("Bad real coordinate try again:");
                move(display.buffer_height, 30);
            }
        }

        char* c_imag = new char[160];
        mpreal imag;

        print_status("Set imaginary coordinate: ");
        move(display.buffer_height, 26);

        while(true)
        {
            clrtoeol();
            getstr(c_imag);

            if(c_imag[0] == '\0')
            {
                print_status("");
                imag = mandelbrot.imag_coordinate;
                if(mandelbrot.real_coordinate == real)
                {
                    // Both of the coordinates are the same, so just return.. 
                    return;
                }
                break;
            }

            try
            {
                imag = c_imag;
                break;
            }
            catch(...)
            {
                print_status("Bad imaginary coordinate try again.");
                move(display.buffer_height, 35);
            }
        }

        mandelbrot.set_coords(real, imag);
        raw(); // Line buffering disabled
    }

    void set_iterations()
    {
        timeout(-1);
        echo();
        char* input = new char[160];

        bool success = false;

        print_status("Set iterations: ");
        move(display.buffer_height, 16);

        while( success == false )
        {
            clrtoeol();
            getstr(input);

            if(input[0] == '\0')
            {
                print_status("");
                break;
            }

            try
            {
                print_status("Calculating using new iteration.");
                mandelbrot.set_max_iterations(std::stoi(input));
                break;
            }
            catch(...)
            {
                success = false;
                print_status("Iteration must be a number: ");
                move(display.buffer_height, 28);
            }
        }
        raw(); // Line buffering disabled
    }
    
    void toggle_shade_cycle()
    {
        renderer.shade_cycle_toggle = !renderer.shade_cycle_toggle;
    }

    void print_status(std::string s)
    {
        renderer.print_stats(s);
    }

    void clear_status()
    {
        print_status("");
    }

    UserInterface(Mandelbrot& _mandelbrot, Display& _display, Renderer& _renderer): mandelbrot(_mandelbrot), display(_display), renderer(_renderer)
    {
    }
};

//
// Application API.
//
class AsciiMandelbrot
{
    public:
    Display display;
    Mandelbrot mandelbrot;
    Renderer renderer{mandelbrot, display};
    UserInterface userInterface{mandelbrot, display, renderer};

    void run()
    {
        userInterface.Navigate();
    }

    void stop()
    {   
        renderer.stop();
        endwin();
    }

    AsciiMandelbrot(int prec)
    {
        mpreal::set_default_prec(mpfr::digits2bits(prec));
    }

    ~AsciiMandelbrot()
    {
        stop();
    }
};

int main(int argc, char *argv[])
{
    int digits_of_precision = 500;

    AsciiMandelbrot app(digits_of_precision);

    app.run();
}
