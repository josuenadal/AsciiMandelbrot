#include <iostream>
#include <math.h>
#include <unistd.h>
#include <iomanip>
#include <fstream>
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

using mpfr::mpreal;

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

    // Mandelbrot orbit calculator. Checks when it converges or shoots off, cap is maxIterations. 
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

};

// Defines the camera view and its movements.
class Mandelbrot_Viewport
{
    public:

    // The limits of the calculated plane
    mpreal mpf_real_min;
    mpreal mpf_real_max;

    mpreal mpf_imag_min;
    mpreal mpf_imag_max;

    // Dimensions of plane to be calculated
    mpreal mpf_width;
    mpreal mpf_height;

    // Desired Focus point
    mpreal mpf_real_coordinate;
    mpreal mpf_imag_coordinate;

    // Zoom Factor - The area gets multiplied by this in order to shrink
    mpreal mpf_zoom_factor;
    mpreal mpf_half_zoom;
    mpreal mpf_zoom_out_factor;

    // The factor which when multiplied by each dimension gives 
    // the distance to move the plane in x or y. 
    mpreal mpf_transl_factor;

    // The length of traversal in X or Y - controlled by transl_factor
    mpreal mpf_transl_x;
    mpreal mpf_transl_y;

    Mandelbrot_Viewport()
    {
        // Set center point.
        mpf_real_coordinate = "0";
        mpf_imag_coordinate = "0";

        // Set factor of the screen area to zoom by.
        // Screen will be zoomed out by the inverse of this factor (1 - mpf_zoom_factor).
        mpf_zoom_factor = "0.9";

        // Set translation factor, will move screen horz and vert by this factor.
        mpf_transl_factor = 0.075;

        // Set visible area.
        mpf_real_min = "-3";
        mpf_real_max = "3";
        mpf_imag_min = "-2";
        mpf_imag_max = "2";

        // Calculate the dimensions of the area.
        mpf_height = mpf_imag_max - mpf_imag_min;
	    mpf_width = mpf_real_max - mpf_real_min;

        // Calculate plane movement distances.
        set_translation_distance();

        // Calculate zoom factors.
        mpf_half_zoom = mpf_zoom_factor * 0.5;
        mpf_zoom_out_factor = (1 + (1 - mpf_zoom_factor)) * 0.5;

    }

    long getMaxIterations()
    {
        return mandelbrot.maxIterations;
    }

    void setMaxIterations(long iterations)
    {
        mandelbrot.maxIterations = iterations;
    }

    // Calculate translation distance at each level.
    void set_translation_distance()
    {
        mpf_transl_x = mpf_width * mpf_transl_factor;
        mpf_transl_y = mpf_height * mpf_transl_factor;
    }

    // Call the mandelbrot point function.
    int calculate_point(mpreal realc, mpreal imaginaryc)
    {
        int iter = mandelbrot.calculate_point(realc, imaginaryc);
        // set_translation_distance();
        return iter;
    }

    // Move viewport up around the point. 
    void move_up()
    {
        mpf_imag_min -= mpf_transl_y;
        mpf_imag_max += mpf_transl_y;
        mpf_imag_coordinate -= mpf_transl_y;
    }
    
    // Move viewport down around the point. 
    void move_down()
    {
        // int vert_chunk = std::floor(buffer_height / screen_chunks);
        //memset(reinterpret_cast<void*>(buffer), ' ', vert_chunk * buffer_width);
        // memmove(buffer, buffer + vert_chunk * buffer_width, buffer_length - vert_chunk * buffer_width);
        mpf_imag_min += mpf_transl_y;
        mpf_imag_max -= mpf_transl_y;
        mpf_imag_coordinate += mpf_transl_y;
        // reset_specific_chunks(1, screen_chunks, screen_chunks, screen_chunks);
    }

    // Move left up around the point. 
    void move_left()
    {
        mpf_real_min -= mpf_transl_x;
        mpf_real_max += mpf_transl_x;
        mpf_real_coordinate -= mpf_transl_x;
    }

    // Move viewport right around the point. 
    void move_right()
    {
        mpf_real_min += mpf_transl_x;
        mpf_real_max -= mpf_transl_x;
        mpf_real_coordinate += mpf_transl_x;
    }

    // Zoom into the mandelbrot by reducing the height of the viewport around the center point.
    void zoom()
    {
        // Zooming is done by reducing the area calculated by zoom_factor, then adding each 
        // half of the new area to each side of the coordenate that is centered. Thus always zooming around the chosen coordinate.
        mpreal half_width, half_height;

        /*  
            I use half of the factor to immediately get the half width with only one multiplication.
            That way i do not have to multiply by the zoom factor then divide by 2. In order to-
            add each half to each side of the coordinate, thereby ensuring the movements are always centered.
        */
        half_width = mpf_width * mpf_half_zoom;
        mpf_real_min =  mpf_real_coordinate - half_width;
        mpf_real_max = mpf_real_coordinate + half_width;

        half_height = mpf_height * mpf_half_zoom;
        mpf_imag_min = mpf_imag_coordinate - half_height;
        mpf_imag_max = mpf_imag_coordinate + half_height;

        mpf_width = half_width + half_width;
        mpf_height = half_height + half_height;
        
        set_translation_distance();
    }

    // Zoom out of the mandelbrot by reducing the hight of the viewport around the center point.
    void zoom_out()
    {
        mpreal half_width, half_height;

        half_width = mpf_width * mpf_zoom_out_factor;
        mpf_real_min =  mpf_real_coordinate - half_width;
        mpf_real_max = mpf_real_coordinate + half_width;

        half_height = mpf_height * mpf_zoom_out_factor;
        mpf_imag_min = mpf_imag_coordinate - half_height;
        mpf_imag_max = mpf_imag_coordinate + half_height;

        mpf_width = half_width + half_width;
        mpf_height = half_height + half_height;
        
        set_translation_distance();
    }

    // Center viewport around specified coords.
    void set_coords(mpreal real, mpreal imag)
    {
        mpf_real_coordinate = real;	
        mpf_imag_coordinate = imag;
        
        mpreal half_width, half_height;

        half_width = mpf_width * 0.5;
        mpf_real_min =  mpf_real_coordinate - half_width;
        mpf_real_max = mpf_real_coordinate + half_width;

        half_height = mpf_height * 0.5;
        mpf_imag_min = mpf_imag_coordinate - half_height;
        mpf_imag_max = mpf_imag_coordinate + half_height;

        set_translation_distance();
    }
    
    // Output some useful information about the viewport.
    std::string get_status()
    {
        std::string s;
        s += "depth = " + (mpf_real_max - mpf_real_min).toString();
        s += "\n\rreal coord = " + mpf_real_coordinate.toString();
        s += "\n\rimag coord = " + mpf_imag_coordinate.toString();
        s += "\n\rreal_min = " +  mpf_real_min.toString();
        s += "\n\rreal_max = " + mpf_real_max.toString();
        s += "\n\rimag_min = " + mpf_imag_min.toString();
        s += "\n\rimag_max = " + mpf_imag_max.toString();
        s += "\n\rIterations = " + std::to_string(getMaxIterations());
        return s;
    }
    
    private:

    // The mandelbrot obj.
    Mandelbrot mandelbrot;
};

//
// Display class that handles all drawing to the screen.
//
class Display
{
    public:

    // Mutex for utilizing screen sizes. Prevents overflow when resizing.
    std::mutex sizes_mutex;

    // Editable buffer.
    std::vector<char> buffer;

    // Display buffer.
    std::vector<char> display_buffer;

    // Buffer dimmensions.    
    long int buffer_width;
    long int buffer_height;
    long int buffer_length;

    // Variable for screen info details.
    std::string stats = "";

    // Function for converting a coordenate to a buffer position.
    int coord_to_buffer_pos(int x, int y)
    {
        return x+(buffer_width*y);
    }

    // Draw whole display buffer at position 1, 1 of terminal.
    void draw_display_buffer()
    {
        // This program uses ncurses and multithreading. 
        // Those 2 don't mix. That's why i use printf to print here.
        // It is imperative to only use printf because ncurses functions
        // are being reserved for the user interace.
        std::unique_lock lock(sizes_mutex);
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
    }

    // Set the stats.
    void set_stats(std::string s)
    {
        stats = s;
    }

    // Print stats to bottom of mandelbrot display area.
    void print_stats()
    {
        // This program uses ncurses and multithreading. 
        // Those 2 don't mix. That's why i use printf to print here.
        // It is imperative to only use printf because ncurses functions
        // are being reserved for the user interace.
        clear_stats();
        printf("\033[%ld;%dH", buffer_height+1, 1);
        printf("\x1B[0J");
        printf("%s",stats.c_str());
        printf("\033[%d;%dH", 1, 1);
    }

    // Clear from the bottom of the mandelbrot display area to the end of the screen.
    void clear_stats()
    {
        printf("\033[%ld;%dH", buffer_height+1, 1);
        printf("\x1B[0J");
    }

    Display()
    {
        // Ncurses init.
        initscr();              // Start curses mode.
        raw();                  // Line buffering disabled
        keypad(stdscr, TRUE);   // We get F1, F2 etc..
        noecho();               // Don't echo() while we do getch.

        // Here to init screen dimensions and set buffer dimmensions.
        adjust_screen_size();

        // Fill buffers with whatever.
        std::fill(buffer.begin(), buffer.end(), '|');
        std::fill(display_buffer.begin(), display_buffer.end(), '|');
    }

    // Main draw function. Presents main content.
    void draw_screen()
    {
        print_stats();
        draw_display_buffer();
    }

    // Readjust screensize if necessary.
    void adjust_screen_size()
    {
        // Lock sizes_mutex so it doesnt interfere with draw.
        std::unique_lock lock(sizes_mutex);
        long int window_width = 0;
        long int window_height = 0;
        getmaxyx(stdscr, window_height, window_width);

        if((buffer_width != window_width) | (buffer_height != window_height))
        {
            //Set buffer dimensions.
            buffer_height = window_height - 9;
            buffer_width = window_width - 1;
            buffer_length = buffer_height * buffer_width;

            buffer.resize(buffer_length);
            std::fill(buffer.begin(), buffer.end(), '|');

            display_buffer.resize(buffer_length);
            std::fill(display_buffer.begin(), display_buffer.end(), '|');
        }
        lock.unlock();
    }

    // Put the finished rendered buffer into the display buffer for presentation.
    void swap_buffer()
    {
        display_buffer.swap(buffer);
    }
};

// 
// Handles all rendering.
//
class Renderer
{
    public:

    // Shading array, this is what the mandelbrot looks like.
    const char* shade_chars = " .,-~o:;*=><!?HX#$@🮙";
    unsigned long int shade_char_size = 0;
    
    // Cycles shades for pulsating appearance.
    bool shade_cycle_toggle = false;

    // Contains every threads work as a tuple of beggining and end positions of the buffer.
    std::queue<std::tuple<int, int>> work_queue;
    
    // Scales for conversions between a continous point and a discrete buffer index.
    mpreal mpf_width_scale;
    mpreal mpf_height_scale;

    // Screen buffer sizes.
    static long int buffer_width;
    static long int buffer_height;
    long int buffer_length = 0;

    // Objects required for rendering.
    Mandelbrot_Viewport& mandelbrot_viewport_ptr;
    Display& display_ptr;

    Renderer(Mandelbrot_Viewport& _m_v_ptr, Display& _display_ptr) : mandelbrot_viewport_ptr(_m_v_ptr), display_ptr(_display_ptr) 
    {
        // Init window sizes and buffer sizes.
        calc_scale();

        // Generate work for threads.
        generate_thread_work();
    }

    // Get character from shader array.
    char get_shade(int iter)
    {
        if(iter == mandelbrot_viewport_ptr.getMaxIterations())
        {
            return ' ';
        }
        return shade_chars[(iter % sizeof(shade_chars))+shade_char_size];
    }

    // Calculate the whole screen.
    // void calculate_whole_frame()
    // {
    //     for (int y = 0; y < display_ptr.buffer_height; y++)
    //     {
    //         for (int x = 0; x <= display_ptr.buffer_width; x++)
    //         {
    //             display_ptr.buffer[display_ptr.coord_to_buffer_pos(x,y)] = get_shade( mandelbrot_viewport_ptr.calculate_point( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
    //         }
    //     }
    // }

    // From buffer index calculate the corresponding point on the mandelbrot.
    void calculate_from_buff_pos(int buff_pos)
    {
        int x = buff_pos % display_ptr.buffer_width;
        int y = buff_pos / display_ptr.buffer_width;
        display_ptr.buffer[buff_pos] = get_shade( mandelbrot_viewport_ptr.calculate_point( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
    }

    // Converts the buffer value into a point value on the mandelbrot plane.
    mpreal map_horz_buffer_to_plane(int buffer_x)
    {	
        return mandelbrot_viewport_ptr.mpf_real_min + buffer_x * mpf_width_scale;
    }
    
    // Converts the buffer value into a point value on the mandelbrot plane.
    mpreal map_vert_buffer_to_plane(int buffer_y)
    {
        return mandelbrot_viewport_ptr.mpf_imag_min + buffer_y * mpf_height_scale;
    }

    // Adjusts the buffer pixel to point conversion for the current window to plane size.
    void calc_scale()
    {
        mpf_width_scale = mandelbrot_viewport_ptr.mpf_width / display_ptr.buffer_width;
        mpf_height_scale = mandelbrot_viewport_ptr.mpf_height / display_ptr.buffer_height;
    }

    // Divides the buffer into equal pieces, one for each thread.
    void generate_thread_work()
    {

        // Reset work queue.
        work_queue = std::queue<std::tuple<int, int>>();
        
        // If there are more threads than indexes make each thread process one line and return.
        if( num_threads >= display_ptr.buffer_length )
        {
            for(int i = 0; i < display_ptr.buffer_length; i++)
            {
                work_queue.emplace(std::make_tuple(i,1));
            }
            return;
        }

        // Calculate amount of lines per each thread.
        int floored = floor(display_ptr.buffer_length / num_threads);
        long int end = 0;

        // Make the work loads, which consist of a tuple of beggining and ending indexes to calculate.
        for(int i = 0; i < display_ptr.buffer_length; i += floored)
        {
            end = i + floored;
            work_queue.emplace(std::make_tuple(i,  end));
        }

        // Check if there is a remainder due to rounding and add it to the last work load.
        int remainder = display_ptr.buffer_length % floored;  

        if(remainder == 0)
        {
            return;
        }

        // Add remainder to last work load
        int first_offset = std::get<0>(work_queue.back());
        int last_offset = display_ptr.buffer_length;

        work_queue.back() = std::make_tuple(first_offset, last_offset);
    }
};

//
//  Handles the rendering and drawing thread.
//
class ThreadPool
{
    public:

    // Create all rendering threads and the draw thread.
    void spawn_threads(uint32_t num_threads)
    {
        terminate_draw = false;
        terminate = false;
        for (uint32_t i = 0; i < num_threads; i++)
        {
            thread_vector.emplace_back(std::thread(&ThreadPool::ThreadLoop,this));
        }
        DT = std::thread(&ThreadPool::draw_loop, this);
    }
    
    // Set the work queue.
    void add_queue(std::queue<std::tuple<int, int>> _work_load)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        done_threads = 0;

        jobs = _work_load;
        // prev_job is kept to to use the same work_load without having to generate it again.
        prev_job = _work_load;

        // When threads are waiting this notifies that the workload is ready.
        queue_condition.notify_all();
        pause_draw_condition.notify_all();
    }

    // Calculate frame using the mandelbrot_viewports current configuration.
    void render_frame()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        done_threads = 0;

        // Reset job queue to the previous
        jobs = prev_job;
        
        lock.unlock();

        // When threads are waiting this notifies that the workload is ready.
        queue_condition.notify_all();
    }

    // Hold draw_mutex until frame_wait_condition is notified.
    void wait_for_frame()
    {
        std::unique_lock<std::mutex> lock(draw_mutex);
        frame_wait_condition.wait(lock);
        lock.unlock();
    }

    // Will render and hold draw until frame is done.
    void render_and_wait()
    {
        render_frame();
        wait_for_frame();
    }

    int get_job_num()
    {
        return jobs.size();
    }

    void clear_jobs()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs = {};
        lock.unlock();
        queue_condition.notify_one();
    }

    void Stop()
    {
        terminate = true;
        terminate_draw = true;

        for (std::thread& active_thread : thread_vector) 
        {
            queue_condition.notify_all();
            active_thread.join();
        }
        thread_vector.clear();

        pause_draw_condition.notify_all();
        DT.join();
    }
    
    bool busy()
    {
        bool poolbusy;

        std::unique_lock<std::mutex> lock(queue_mutex);
        poolbusy = !jobs.empty();
        
        return poolbusy;
    }

    ThreadPool(uint32_t num_threads, Renderer& _renderer_ptr) : renderer_ptr(_renderer_ptr)
    {
        addstr("INIT");
        add_queue(renderer_ptr.work_queue);
        spawn_threads(num_threads);
        render_and_wait();
    }

    private:

    bool terminate = false;
    bool pause_draw_thread = false;
    bool terminate_draw = false;

    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    std::condition_variable job_condition;

    std::mutex draw_mutex;
    std::condition_variable pause_draw_condition;
    std::condition_variable frame_wait_condition;
    std::condition_variable queue_wait_condition;
    
    std::vector<std::thread> thread_vector;

    std::queue<std::tuple<int, int>> jobs = {};
    std::queue<std::tuple<int, int>> prev_job = {};

    std::chrono::time_point<std::chrono::system_clock> shade_start_time = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> frame_start_time = std::chrono::system_clock::now();
    std::thread DT;

    u_int32_t done_threads = num_threads;

    Renderer& renderer_ptr;

    void ThreadLoop()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // If mutex is occupied, wait until its not.
            queue_condition.wait(lock,[this] 
            {
                return (jobs.empty() == false) | terminate;
            });

            if (terminate) 
            {
                queue_condition.notify_one();
                lock.unlock();
                return;
            }

            // Take and delete job from the queue
            std::tuple<int, int> job = jobs.front();
            jobs.pop();

            // Allow other thread to engage.
            lock.unlock();

            // Do the work.
            process_pixels(std::get<0>(job), std::get<1>(job));
        }
    }

    bool check_shade_time()
    {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::chrono::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - shade_start_time);
        if(duration.count() > 200)
        {
            shade_start_time = now;
            render_frame();
            return true;
        }
        return false;
    }

    bool check_frame_time()
    {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::chrono::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - frame_start_time);
        if(duration.count() > 100)
        {
            frame_start_time = now;
            return true;
        }
        return false;
    }

    void cycle_shade()
    {
        if(check_shade_time())
        {
            if(renderer_ptr.shade_char_size > sizeof(renderer_ptr.shade_chars))
            {
                renderer_ptr.shade_char_size = 0;
            }
            else
            {
                renderer_ptr.shade_char_size++;
            }
        }
    }

    void draw_loop()
    {
        while(true)
        {
            std::unique_lock lock(draw_mutex);

            pause_draw_condition.wait(lock, [this]{
                return (pause_draw_thread == false) | terminate_draw ;
            });

            if (terminate_draw) 
            {
                return;
            }

            if(renderer_ptr.shade_cycle_toggle)
            {
                cycle_shade();
            }
            
            if(check_frame_time())
            {
                renderer_ptr.display_ptr.draw_screen();
            }

            lock.unlock();

            frame_wait_condition.notify_one();
        }
    }

    void signal_done()
    {
        done_threads++;
        if(done_threads == prev_job.size())
        {
            std::unique_lock<std::mutex> q_lock(queue_mutex);
            std::unique_lock d_lock(draw_mutex);

            renderer_ptr.display_ptr.swap_buffer();

            d_lock.unlock();
            q_lock.unlock();
            // std::fill(renderer_ptr.display_ptr.buffer.begin(), renderer_ptr.display_ptr.buffer.end(), 'X');

            frame_wait_condition.notify_one();

        }
    }

    void process_pixels(int beg, int end)
    {
        for( int i = beg; i < end; i++)
        {
            if(terminate)
            {
                return;
            }
            renderer_ptr.calculate_from_buff_pos(i);
        }
        signal_done();
    }
};

class UserInterface
{
    public:

    std::string status = "\n\r";

    Mandelbrot_Viewport mandelbrot_viewport;

    Display display;

    // Pass a pointer to the TerminalRenderer class 
    // so it can use it.
    Renderer renderer{mandelbrot_viewport, display};

    ThreadPool pool{num_threads, renderer};

    std::chrono::time_point<std::chrono::system_clock> ui_start_time = std::chrono::system_clock::now();

    int c;

    void Navigate()
    {

        keypad(stdscr, TRUE);
        clear();
        noecho();

        renderer.calc_scale();
        pool.render_and_wait();
        
        while(true)
        {
            clear_status();
            flushinp();
            c = getch();
            switch(c)
            {			
                case 10:	//10 is enter on normal keyboard
                    set_status("Zooming in...");
                    mandelbrot_viewport.zoom();
                    renderer.calc_scale();
                    pool.render_and_wait();
                    break;
                case KEY_BACKSPACE:
                    set_status("Zooming out...");
                    mandelbrot_viewport.zoom_out();
                    renderer.calc_scale();
                    pool.render_and_wait();
                    break;
                case KEY_UP:
                    set_status("Moving up...");
                    mandelbrot_viewport.move_up();
                    pool.render_and_wait();
                    break;
                case KEY_DOWN:
                    set_status("Moving down...");
                    mandelbrot_viewport.move_down();
                    pool.render_and_wait();
                    break;
                case KEY_LEFT:
                    set_status("Moving left...");
                    mandelbrot_viewport.move_left();
                    pool.render_and_wait();
                    break;
                case KEY_RIGHT:
                    set_status("Moving right...");
                    mandelbrot_viewport.move_right();
                    pool.render_and_wait();
                    break;
                case KEY_RESIZE:
                    resize_window();
                    pool.render_and_wait();
                    break;
                case 67: // uppercase C
                case 99: // lowercase c
                    set_status("Toggle Shade Cycling");
                    toggle_shade_cycle();
                    break;
                case 88: 	// uppercase X
                case 120: 	// lowercase X
                    pool.Stop();
                    set_coords();
                    pool.add_queue(renderer.work_queue);
                    pool.spawn_threads(num_threads);
                    pool.render_and_wait();
                    break;
                case 73:	//uppercase I
                case 105:	//lowercase i
                    pool.Stop();
                    set_iterations();
                    pool.add_queue(renderer.work_queue);
                    pool.spawn_threads(num_threads);
                    pool.render_and_wait();
                    break;
                case 113: //letter q, for quit
                case 27:  //escape key
                    pool.Stop();
                    endwin();
                    return;
                    break;
            }
        }
    }

    bool check_ui_time()
    {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::chrono::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - ui_start_time);
        if(duration.count() > 100)
        {
            ui_start_time = now;
            return true;
        }
        return false;
    }

    void toggle_shade_cycle()
    {
        if(renderer.shade_cycle_toggle)
        {
            renderer.shade_cycle_toggle = false;
        }
        else
        {
            renderer.shade_cycle_toggle = true;
        }
    }

    void update_status()
    {
        display.set_stats(status + mandelbrot_viewport.get_status());
    }

    void set_status(std::string s)
    {
        status = s + "\n\r";
        update_status();
    }

    void draw()
    {
        pool.wait_for_frame();
        display.draw_screen();
    }

    void clear_status()
    {
        set_status("");
        update_status();
    }

    void resize_window()
    {
        pool.Stop();

        // Resize screen
        display.adjust_screen_size();
        renderer.calc_scale();
        renderer.generate_thread_work();

        // Add recalculated workload to the thread pool.
        pool.add_queue(renderer.work_queue);

        pool.spawn_threads(num_threads);

        pool.render_and_wait();
    }

    void set_coords()
    {
        timeout(-1);
        echo();

        mvprintw(display.buffer_height,0,"Set real coordinate.");
        
        bool success = false;

        char* c_real = new char[160];
        mpreal real;
        while(success == false)
        {
            move(display.buffer_height+2,13);
            clrtoeol();
            getstr(c_real);

            if(c_real[0] == '\0')
            {
                break;
            }

            try
            {
                real = c_real;
                break;
            }
            catch(...)
            {
                success = false;
                mvprintw(display.buffer_height+1,0,"Bad real coordinate try again.");
            }
        }

        char* c_imag = new char[160];
        mpreal imag;
        success = false;
        while(success == false)
        {
            mvprintw(display.buffer_height,0,"Set imaginary coordinate.");
            move(display.buffer_height+3,13);
            clrtoeol();
            getstr(c_imag);

            if(c_imag[0] == '\0')
            {
                break;
            }

            try
            {
                imag = c_imag;
                break;
            }
            catch(...)
            {
                success = false;
                mvprintw(display.buffer_height,0,"Bad imaginary coordinate try again.");
            }
        }

        mandelbrot_viewport.set_coords(real, imag);
        
        keypad(stdscr, TRUE);
        clear();
        noecho();        
        clear_status();
    }

    void set_iterations()
    {
        timeout(-1);
        echo();
        char* input = new char[160];

        bool success = false;

        mvprintw(display.buffer_height,0,"Set iterations.");

        while( success == false )
        {
            mvprintw(display.buffer_height+8,0,"Iterations = ");
            clrtoeol();
            getstr(input);

            if(input[0] == '\0')
            {
                break;
            }

            try
            {
                mandelbrot_viewport.setMaxIterations(std::stoi(input));
                break;
            }
            catch(...)
            {
                success = false;
                mvprintw(display.buffer_height,0,"Bad number try again.");
            }
        }
        
        keypad(stdscr, TRUE);
        clear();
        noecho();        
        clear_status();
    }
};

int main(int argc, char *argv[])
{
    addstr("starting...");
    mpreal::set_default_prec(mpfr::digits2bits(50));

    UserInterface UI;
	UI.Navigate();
}
