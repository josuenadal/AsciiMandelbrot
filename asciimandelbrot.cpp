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

const uint32_t num_threads = std::thread::hardware_concurrency() * 2 - 2;
// const uint32_t num_threads = 1;

class Mandelbrot
{
    public:
    // Number of iterations
    long int maxIterations = 50;

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

    // The length of traversal in X or Y - controlled by transl_factor
    mpreal mpf_transl_x;
    mpreal mpf_transl_y;

    // The factor which when multiplied by each dimension gives 
    // the distance to move the plane in x or y. 
    mpreal mpf_transl_factor;

    Mandelbrot_Viewport()
    {
        // Set center point.
        mpf_real_coordinate = "0";
        mpf_imag_coordinate = "0";

        mpf_zoom_factor = "0.9";

        // Set visible area.
        mpf_real_min = "-3";
        mpf_real_max = "3";
        mpf_imag_min = "-2";
        mpf_imag_max = "2";

        // Set the dimensions of the area.
        mpf_height = mpf_imag_max - mpf_imag_min;
	    mpf_width = mpf_real_max - mpf_real_min;

        mpf_transl_factor = 0.1;

        // Set plane movement distances
        set_translation_distance();

        // Set zoom factors
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

    void set_translation_distance()
    {
        mpf_transl_x = mpf_width * mpf_transl_factor;
        mpf_transl_y = mpf_height * mpf_transl_factor;
    }

    int calculate_point(mpreal realc, mpreal imaginaryc)
    {
        int iter = mandelbrot.calculate_point(realc, imaginaryc);
        // set_translation_distance();
        return iter;
    }

    void move_up()
    {
        // int vert_chunk = std::floor(buffer_height / screen_chunks);
        //memset(reinterpret_cast<void*>(buffer), ' ', vert_chunk * buffer_width);
        // memmove(buffer + vert_chunk * buffer_width, buffer, buffer_size);
        mpf_imag_min -= mpf_transl_y;
        mpf_imag_max += mpf_transl_y;
        mpf_imag_coordinate -= mpf_transl_y;
        // reset_specific_chunks(1, screen_chunks, 1, 1);
    }

    void move_down()
    {
        // int vert_chunk = std::floor(buffer_height / screen_chunks);
        //memset(reinterpret_cast<void*>(buffer), ' ', vert_chunk * buffer_width);
        // memmove(buffer, buffer + vert_chunk * buffer_width, buffer_size - vert_chunk * buffer_width);
        mpf_imag_min += mpf_transl_y;
        mpf_imag_max -= mpf_transl_y;
        mpf_imag_coordinate += mpf_transl_y;
        // reset_specific_chunks(1, screen_chunks, screen_chunks, screen_chunks);
    }

    void move_left()
    {
        mpf_real_min -= mpf_transl_x;
        mpf_real_max += mpf_transl_x;
        mpf_real_coordinate -= mpf_transl_x;
    }

    void move_right()
    {
        mpf_real_min += mpf_transl_x;
        mpf_real_max -= mpf_transl_x;
        mpf_real_coordinate += mpf_transl_x;
    }

    void zoom()
    {
        // Zooming is done by reducing the area calculated by zoom factor, then adding each 
        // half of the new area to each side of the coordenate. Thus always zooming around the chosen coordinate.
        mpreal half_width, half_height;

        // I use half of the factor to immediately get the half width with only one multiplication.
        // That way i do not have to multiply by the zoom factor then divide by 2. In order to-
        // add each half to each side of the coordinate.
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

    void center_around_set_coords()
    {
        mpreal half_width, half_height;

        half_width = mpf_width * 0.5;
        mpf_real_min =  mpf_real_coordinate - half_width;
        mpf_real_max = mpf_real_coordinate + half_width;

        half_height = mpf_height * 0.5;
        mpf_imag_min = mpf_imag_coordinate - half_height;
        mpf_imag_max = mpf_imag_coordinate + half_height;

        // set_translation_distance();
    }

    void set_coords(mpreal real, mpreal imag)
    {
        mpf_real_coordinate = real;	
        mpf_imag_coordinate = imag;
        center_around_set_coords();
    }

    // Output some useful information about the map and program data.
    void output_status_bar()
    {
        // printw("depth = %s\n", (mpf_real_max - mpf_real_min).toString().c_str() );
        // printw("real coord = %s\nimag coord = %s\n", mpf_real_coordinate.toString().c_str(), mpf_imag_coordinate.toString().c_str());
        // printw("real_min = %s \nreal_max = %s\n", mpf_real_min.toString().c_str(), mpf_real_max.toString().c_str());
        // printw("imag_min = %s \nimag_max = %s \n", mpf_imag_min.toString().c_str(), mpf_imag_max.toString().c_str());
        // printw("Iterations: %ld\n", getMaxIterations());
        // printw("buffer_width: %d buffer_height = %d", buffer_width, buffer_height);
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

    Mandelbrot mandelbrot;
};

class Display
{
    public:

    std::mutex sizes_mutex;

    std::vector<char> buffer;

    long int buffer_width;
    long int buffer_height;

    long int buffer_size;

    bool frame_ready = true;

    std::string stats = "";

    int coord(int x, int y)
    {
        return x+(buffer_width*y);
    }

    // Draw the frame on the terminal.
    // void draw_nobuff()
    // {
    //     move(0,0);
    //     for (int pos = 0; pos < buffer_size-1; pos += buffer_width)
    //     {
    //         write(1, buffer.data() + pos, buffer_width);
    //         write(1, "\n", sizeof(char));
    //         write(1, "\r", sizeof(char));
    //     }
    // }

    void draw()
    {
        std::unique_lock lock(sizes_mutex);
        printf("\033[%d;%dH", 1, 1);
        for (int h = 0; h < buffer_height; h++)
        {
            for(int w = 0; w < buffer_width; w++)
            {
                printf("%c",buffer[(h*buffer_width)+w]);
            }
            printf("%c",'\r');
            printf("%c",'\n');
        }
        // printf("\x1B[1A");
    }

    void print_stats()
    {
        printf("\033[%ld;%dH", buffer_height+1, 1);
        // printf("\x1B[0J");
        printf("%s",stats.c_str());
        printf("\033[%d;%dH", 1, 1);
    }

    void clear_stats()
    {
        printf("\033[%ld;%dH", buffer_height+1, 1);
        printf("\x1B[0J");
    }

    Display()
    {
        // Ncurses init
        initscr();			/* Start curses mode 		*/
        raw();				/* Line buffering disabled	*/
        keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
        noecho();			/* Don't echo() while we do getch */

        adjust_screen_size();

        std::fill(buffer.begin(), buffer.end(), '|');
    }

    void display_img()
    {
        print_stats();
        draw();
    }

    void display_threaded()
    {
        while(true)
        {
            if(frame_ready == true)
            {
                clear();
                draw();
                print_stats();
            }
        }
    }

    // void generate_draw_thread()
    // {
    //     std::thread T1(&Display::display_threaded, this);
    //     T1.detach();
    // }

    void set_stats(std::string s)
    {
        stats = s;
    }

    void adjust_screen_size()
    {
        std::unique_lock lock(sizes_mutex);
        long int window_width = 0;
        long int window_height = 0;
        getmaxyx(stdscr, window_height, window_width);

        if((buffer_width != window_width) | (buffer_height != window_height))
        {
            //Set buffer dimensions.
            buffer_height = window_height - 9;
            buffer_width = window_width - 1;
            buffer_size = buffer_height * buffer_width;
            buffer.resize(buffer_size);
            std::fill(buffer.begin(), buffer.end(), '|');
        }
    }
};

class Renderer
{
    public:
    // Shading array
    const char* shade_chars = " .,-~o:;*=><!?HX#$@🮙";
    // const char* shade_chars = " ░▒▓█";
    
    bool shade_toggle = false;
    unsigned long int shade_char_size = 0;


    // Work load for the threads in <start_line, end_line> tuple.
    std::queue<std::tuple<int, int>> work_loads;
    
    mpreal mpf_width_scale;
    mpreal mpf_height_scale;

    // Screen Buffer Size in Character width/height
    static long int buffer_width;
    static long int buffer_height;
    long int buffer_size = 0;

    long int window_width = 0;
    long int window_height = 0;

    bool ghosting = false;

    Mandelbrot_Viewport& mandelbrot_viewport_ptr;
    Display& display_ptr;

    Renderer(Mandelbrot_Viewport& _m_v_ptr, Display& _display_ptr) : mandelbrot_viewport_ptr(_m_v_ptr), display_ptr(_display_ptr) 
    {
        calc_scale();
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

    // Calculate the points in the mandelbrot_viewport_ptr. 
    void calculate_whole_frame()
    {
        for (int y = 0; y < display_ptr.buffer_height; y++)
        {
            for (int x = 0; x <= display_ptr.buffer_width; x++)
            {
                display_ptr.buffer[display_ptr.coord(x,y)] = get_shade( mandelbrot_viewport_ptr.calculate_point( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
            }
        }
    }

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

    void generate_thread_work()
    {
        display_ptr.frame_ready = false;

        work_loads = std::queue<std::tuple<int, int>>();
        // printw("width, height: %ld, %ld\n", display_ptr.buffer_width, display_ptr.buffer_height);
        // If there are more threads than lines make each thread process one line.
        if( num_threads >= display_ptr.buffer_size )
        {
            for(int i = 0; i < display_ptr.buffer_size; i++)
            {
                work_loads.emplace(std::make_tuple(i,1));
            }
            return;
        }

        // Calculate amount of lines per each thread.
        int floored = floor(display_ptr.buffer_size / num_threads);
        long int end = 0;

        // Make the work loads, which consist of a tuple of beggining and ending indexes to calculate.
        for(int i = 0; i < display_ptr.buffer_size; i += floored)
        {
            end = i + floored;
            work_loads.emplace(std::make_tuple(i,  end));
            // printw("Workload %d, %ld\n", i, end);
        }

        // Check if there is a remainder due to rounding and add it to the last work load.
        int remainder = display_ptr.buffer_size % floored;  

        if(remainder == 0)
        {
            return;
        }

        // Add remainder to last work load
        int first_offset = std::get<0>(work_loads.back());
        int last_offset = display_ptr.buffer_size;

        // printw("Workload %d, %d\n", first_offset, last_offset);
        work_loads.back() = std::make_tuple(first_offset, last_offset);
        // refresh();
    }
};

class ThreadPool
{
    private:

    bool terminate = false;
    bool frame_ready = false;
    bool pause_draw_thread = false;
    bool terminate_draw = false;

    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    std::condition_variable job_condition;

    std::mutex draw_mutex;
    std::condition_variable pause_draw_condition;
    std::condition_variable frame_wait_condition;
    
    std::vector<std::thread> thread_vector;

    std::queue<std::tuple<int, int>> jobs = {};
    std::queue<std::tuple<int, int>> const_jobs = {};

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
        if(duration.count() > 1)
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

            if(renderer_ptr.shade_toggle)
            {
                cycle_shade();
            }
            
            if(check_frame_time())
            {
                if(frame_ready)
                {
                    renderer_ptr.display_ptr.display_img();
                    frame_wait_condition.notify_all();
                    frame_ready = false;
                }
                else
                {
                    renderer_ptr.display_ptr.clear_stats();
                    renderer_ptr.display_ptr.print_stats();
                }
            }
        }
    }

    void signal_done()
    {
        done_threads++;
        if(done_threads == (num_threads-1))
        {
            frame_ready = true;
            frame_wait_condition.notify_all();
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

    public:

    void raise_pause_draw_flag()
    {
        pause_draw_thread = true;
    }

    void unpause_draw_thread()
    {
        pause_draw_thread = false;
        pause_draw_condition.notify_all();
    }

    int get_num_done_threads()
    {
        return done_threads;
    }

    void spawn_threads(uint32_t num_threads)
    {
        terminate_draw = false;
        terminate = false;
        for (uint32_t i = 0; i < num_threads - 1; i++)
        {
            thread_vector.emplace_back(std::thread(&ThreadPool::ThreadLoop,this));
        }
        DT = std::thread(&ThreadPool::draw_loop, this);
    }

    void init_draw_thread()
    {
        DT.join();
        terminate_draw = false;
        erase();
        DT = std::thread(&ThreadPool::draw_loop, this);
    }

    void kill_draw_thread()
    {
        std::unique_lock lock(draw_mutex);
        addstr("killing draw thread now");
        terminate_draw = true;
    }
    
    void add_queue(std::queue<std::tuple<int, int>> _work_load)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        frame_ready = false;
        done_threads = 0;

        jobs = _work_load;
        const_jobs = _work_load;

        // When threads are waiting this notifies that the workload is ready.
        queue_condition.notify_all();
        pause_draw_condition.notify_all();
    }

    void render_frame()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        frame_ready = false;
        done_threads = 0;

        // Reset job queue to the previous
        jobs = const_jobs;
        
        // When threads are waiting this notifies that the workload is ready.
        queue_condition.notify_all();
    }

    void wait_for_frame()
    {
        std::unique_lock<std::mutex> lock(draw_mutex);

        // If mutex is occupied, the thread is still being generated.
        frame_wait_condition.wait(lock);

        lock.unlock();
    }

    void render_and_wait()
    {
        render_frame();
        wait_for_frame();
        wait_for_draw();
    }

    int get_job_num()
    {
        return jobs.size();
    }

    void clear_jobs()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs = {};
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
        add_queue(renderer_ptr.work_loads);
        spawn_threads(num_threads);
        render_and_wait();
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

    int c;

    void Navigate()
    {

        keypad(stdscr, TRUE);
        clear();
        noecho();
        std::ios_base::sync_with_stdio(false);
        // nodelay(stdscr, TRUE);
        
        while(true)
        {
            clear_status();
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
                    set_coords();
                    pool.render_and_wait();
                    break;
                case 73:	//uppercase I
                case 105:	//lowercase i
                    set_iterations();
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

    void toggle_shade_cycle()
    {
        if(renderer.shade_toggle)
        {
            renderer.shade_toggle = false;
        }
        else
        {
            renderer.shade_toggle = true;
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
        display.display_img();
    }

    void clear_status()
    {
        set_status("");
        update_status();
    }

    void resize_window()
    {
        // pool.Stop();

        // Resize screen
        display.adjust_screen_size();
        renderer.calc_scale();
        renderer.generate_thread_work();

        // Add recalculated workload to the thread pool.
        pool.add_queue(renderer.work_loads);

        pool.spawn_threads(num_threads);

        pool.render_and_wait();
    }

    void set_coords()
    {
        timeout(-1);
        echo();

        set_status("Set real coordinate.");
        bool success = false;

        char* c_real = new char[160];
        mpreal real;
        while(success == false)
        {
            move(display.buffer_height+2,13);
            clrtoeol();
            getstr(c_real);

            try
            {
                real = c_real;
                success = true;
            }
            catch(...)
            {
                success = false;
                set_status("Bad real coordinate try again.");
            }
        }

        char* c_imag = new char[160];
        mpreal imag;
        success = false;
        while(success == false)
        {
            set_status("Set imaginary coordinate.");
            move(display.buffer_height+3,13);
            clrtoeol();
            getstr(c_imag);

            try
            {
                imag = c_imag;
                success = true;
            }
            catch(...)
            {
                success = false;
                set_status("Bad imaginary coordinate try again.");
            }
        }

        mandelbrot_viewport.set_coords(real, imag);
        
        keypad(stdscr, TRUE);
        clear();
        noecho();
        std::ios_base::sync_with_stdio(false);
        nodelay(stdscr, TRUE);
        clear_status();
    }

    void set_iterations()
    {
        timeout(-1);
        echo();
        char* input = new char[160];

        bool success = false;

        set_status("Set iterations.");
        while( success == false )
        {
            move(display.buffer_height+8,12);
            clrtoeol();
            getstr(input);
            try
            {
                mandelbrot_viewport.setMaxIterations(std::stoi(input));
                success = true;
            }
            catch(...)
            {
                success = false;
                set_status("Bad number try again.");
            }
        }
        
        keypad(stdscr, TRUE);
        clear();
        noecho();
        std::ios_base::sync_with_stdio(false);
        nodelay(stdscr, TRUE);
        
        clear_status();
    }
};

int main(int argc, char *argv[])
{
    addstr("starting...");
    mpreal::set_default_prec(mpfr::digits2bits(20));

    UserInterface UI;
	UI.Navigate();
}
